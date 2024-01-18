#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <filesystem>
#include <chrono>

#include <cstdint>

#include "Options.hpp"
#include "RLE.hpp"
#include "ZLC.hpp"
#include "MultithreadCompressor.hpp"

namespace fs = std::filesystem;
namespace ch = std::chrono;


Options options;


typedef std::pair<uint32_t, fs::path> file_info_t;

constexpr size_t MAX_MEMORY_USAGE = 1 << 31;

struct FpkTRL
{
	uint32_t key, toc_offset;
};

template <size_t S>
struct FpkEntry1
{
	uint32_t offset, length;
	char filename[S];
	uint32_t hash;

	FpkEntry1() = default;

	FpkEntry1(uint32_t offs, uint32_t len, const std::string& fn, uint32_t hash) :
		offset(offs), length(len), hash(hash)
	{
		std::memset(filename, 0, sizeof(filename));
		std::strncpy(filename, fn.c_str(), sizeof(filename) - 1);
	}
};

typedef FpkEntry1<24> FpkV2Entry;
typedef FpkEntry1<128> FpkV3Entry;

struct FpkEntry2
{
	uint32_t offset, length;
	char filename[24];
};

std::string str_tolower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return std::tolower(c); } // correct
	);
	return s;
}
inline const char* bool_to_str(bool b)
{
	return b ? "true" : "false";
}

void save_file(const std::vector<uint8_t>& file, const fs::path& filename)
{
	std::ofstream fout(filename, std::ios::binary);
	fout.write((char*)file.data(), file.size());
}
std::vector<uint8_t> load_file(const std::string& filename)
{
	std::ifstream fin(filename, std::ios::binary | std::ios::ate);
	fin.exceptions(std::ios::failbit | std::ios::badbit);
	std::vector<uint8_t> file(fin.tellg());
	fin.seekg(0);
	fin.read((char*)file.data(), file.size());
	return file;
}
std::vector<uint8_t> load_file(const fs::path& filename)
{
	return load_file(filename.string());
}

template<typename T>
T read(std::istream& f)
{
	T t;
	f.read((char*)&t, sizeof(t));
	return t;
}
template<typename T>
std::vector<T> read(std::istream& f, size_t count)
{
	std::vector<T> v(count);
	f.read((char*)v.data(), count * sizeof(T));
	return v;
}

template<typename T>
void read(std::istream& f, T& t)
{
	f.read((char*)&t, sizeof(t));
}
template<typename T>
void read(std::istream& f, std::vector<T>& v)
{
	f.read((char*)v.data(), v.size() * sizeof(T));
}

template<typename T>
void write(std::ostream& f, const T& t)
{
	f.write((const char*)&t, sizeof(T));
}
template<typename T>
void write(std::ofstream& f, const std::vector<T>& v)
{
	f.write((const char*)v.data(), v.size() * sizeof(T));
}

template<typename T>
void obfuscate(std::vector<T>& data, uint32_t key)
{
	uint32_t* pos = (uint32_t*)data.data();
	uint32_t* end = (uint32_t*)((size_t)pos + data.size() * sizeof(T));
	while (pos < end)
		*pos++ ^= key;
}

template <typename T>
void extract_toc(std::istream& fin, const std::vector<T>& toc, const fs::path& outpath, uint32_t entry_count)
{
	std::ofstream fout;
	fout.exceptions(std::ios::failbit | std::ios::badbit);
	for (auto& entry : toc)
	{
		fin.seekg(entry.offset);
		auto data = read<uint8_t>(fin, entry.length);

		if (options.verbose)
			std::cout << entry.filename << '\n';

		data = rle::decompress(data);
		data = zlc::decompress(data);

		fout.open(outpath / entry.filename, std::ios::binary);
		fout.write((char*)data.data(), data.size());
		fout.close();
	}
}

template <typename T>
void extract_obfuscated(std::istream& fin, const fs::path& outpath, uint32_t entry_count)
{
	fin.seekg(-(int)sizeof(FpkTRL), std::ios::end);
	
	auto trl = read<FpkTRL>(fin);
	if (options.verbose)
	{
		std::cout << "Obfuscation key: 0x" << std::right << std::hex << std::setfill('0') << std::setw(8) << trl.key << '\n'
			<< std::left << std::dec << std::setfill(' ')
			<< "TOC offset: " << std::setw(8) << trl.toc_offset << "\n\n";
	}

	fin.seekg(trl.toc_offset);
	auto toc = read<T>(fin, entry_count);
	
	obfuscate(toc, trl.key);
	extract_toc(fin, toc, outpath, entry_count);
}

void extract_fpk(const fs::path& inpath, const fs::path& outpath, int version = 2)
{
	std::ifstream fin(inpath, std::ios::binary);
	fin.exceptions(std::ios::failbit | std::ios::badbit);
	uint32_t entry_count = read<uint32_t>(fin);
	uint32_t obfuscated = entry_count & 0x80000000;
	entry_count &= ~0x80000000;
	
	if (options.verbose)
	{
		auto old_pos = fin.tellg();
		fin.seekg(0, std::ios::end);
		std::cout
			<< "File size: " << (size_t)fin.tellg() << '\n'
			<< "File version: " << version << '\n'
			<< "Entry count: " << entry_count << '\n'
			<< "Obfuscated: " << bool_to_str(obfuscated) << '\n';
		fin.seekg(old_pos);
	}

	fs::create_directories(outpath);

	if (obfuscated) 
	{
		if (version <= 2)
			extract_obfuscated<FpkV2Entry>(fin, outpath, entry_count);
		else extract_obfuscated<FpkV3Entry>(fin, outpath, entry_count);
	}
	else 
	{
		auto toc = read<FpkEntry2>(fin, entry_count);
		extract_toc(fin, toc, outpath, entry_count);
	}
}

uint32_t hash(const std::string& s)
{
	char c;
	uint16_t res = 0;
	for (int i = 0; i < s.length();)
	{
		c = ::toupper(s[i++]);
		res += c * i;
	}
	return res;
}

void pack_fpk_sync(const std::deque<fs::path>& files, const fs::path& outpath)
{
	if (options.verbose)
		std::cout << "Starting single threaded packing...\n";

	std::ofstream fout(outpath, std::ios::binary);
	fout.exceptions(std::ios::failbit | std::ios::badbit);
	
	uint32_t file_count = files.size();
	uint32_t fpk_header = file_count | 0x80000000;
	write(fout, fpk_header);
	
	std::multimap<uint32_t, FpkV2Entry> toc_map;

	uint32_t h;
	std::string fn;
	int i = 0;
	for (auto& filepath : files)
	{
		fn = filepath.filename().string();
		if (options.verbose)
			std::cout << '(' << i << '/' << file_count << ") " << fn << '\n';

		auto file = load_file(filepath);
		
		if (options.zlc)
			file = zlc::compress<ZlcDict>(file);
		//if (options.rle)
		//	file = rle::compress(file);

		h = hash(fn);
		toc_map.insert(std::make_pair(h, FpkV2Entry(fout.tellp(), file.size(), fn, h)));

		write(fout, file);

		i++;
	}

	FpkTRL trl;
	trl.toc_offset = fout.tellp();
	trl.key = 0; // TODO: custom key
	
	for (auto& [hash, entry] : toc_map)
		write(fout, entry);

	write(fout, trl);
}

void compression_started_callback(const std::pair<std::string, std::vector<uint8_t>>& task)
{
	std::cout << task.first + '\n';
}

void file_loader(
	const std::deque<fs::path>& files,
	MultithreadCompressor<zlc>& compressor)
{
	for (auto& filepath : files)
	{
		while (compressor.memory_usage() > MAX_MEMORY_USAGE)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		compressor.emplace(std::make_pair(filepath.filename().string(), load_file(filepath)));
	}
}

void pack_fpk_async(const std::deque<fs::path>& files, const fs::path& outpath)
{
	if (options.verbose)
		std::cout << "Starting multi threaded packing...\n";

	std::ofstream fout(outpath, std::ios::binary);
	fout.exceptions(std::ios::failbit | std::ios::badbit);

	uint32_t file_count = files.size();
	uint32_t fpk_header = file_count | 0x80000000;
	write(fout, fpk_header);

	MultithreadCompressor<zlc> compressor;
	if (options.verbose)
		compressor.task_started_callack(compression_started_callback);
	compressor.start(MultithreadCompressor<zlc>::Mode::compress);

	std::thread producer(file_loader, std::ref(files), std::ref(compressor));

	std::multimap<uint32_t, FpkV2Entry> toc_map;
	std::pair<std::string, std::vector<uint8_t>> result;
	uint32_t h;
	uint32_t files_processed = 0;
	while (files_processed < file_count)
	{
		while (!compressor.try_pop(result))
			std::this_thread::sleep_for(std::chrono::seconds(1));

		h = hash(result.first);
		toc_map.insert(std::make_pair(h, FpkV2Entry(fout.tellp(), result.second.size(), result.first, h)));

		write(fout, result.second);

		files_processed++;

		printf("%.1f%%\n", (float)(100 * files_processed) / (float)file_count);
	}

	FpkTRL trl;
	trl.toc_offset = fout.tellp();
	trl.key = options.key;

	write(fout, trl);
	producer.join();
}

void pack_fpk(const fs::path& inpath, const fs::path& outpath)
{
	if (!fs::exists(inpath))
		throw std::exception("The input path does not exist!");
	if (!fs::is_directory(inpath))
		throw std::exception("The input path must be a directory!");

	fs::path p;
	std::string name;
	
	std::deque<fs::path> files;
	for (auto& entry : fs::directory_iterator(inpath))
	{
		if (entry.is_regular_file())
		{
			auto fn = entry.path().filename().string();
			if (fn.length() >= sizeof(FpkV2Entry::filename))
				throw std::runtime_error("Filename \"" + fn + "\" too long. "
					"Maximum length: " + std::to_string(sizeof(FpkV2Entry::filename) - 1));
			files.emplace_back(entry.path());
		}
		else if (entry.is_directory())
			std::cout << "Warning: FPK archives do not support subdirectories, they will be ignored.\n";
		else
			std::cout << "Warning: Invalid file type of file " << entry.path() << ". This entry will be ignored.\n";
	}

	if (options.threads != 1)
		pack_fpk_async(files, outpath);
	else pack_fpk_sync(files, outpath);
}


void print_usage()
{
	std::cout <<
		"Usage: <me> [options] <input>\n"
		"Options should be from the following list:\n\n"
		"Modes:\n"
		"  -e, --extract       extract PFK archive (default)\n"
		"  -p, --pack          pack FPK archive\n"
		"  -l, --list          only list files in the archive\n\n"
		"Compressions:\n"
		"  -z, --zlc           enable ZLC compression (default)\n"
		"  -Z, --Zlc           disable ZLC compression\n"
		"  -r, --rle           enable RLE compression\n"
		"  -R, --Rle           disable RLE compression (default)\n\n"
		"Packing options:\n"
		"  -t, --threads <n>   number of threads to use while compression (default: #system threads)\n"
		"  -k, --key <key>     the key to use while obfuscating (default: 0)\n\n"
		"General options:\n"
		"  -h, --help          show this help message and exit\n"
		"  -o, --output        set the output path\n"
		"  -v, --verbose       print detailed information while processing\n";
}
void print_usage_and_exit(int code = 0)
{
	print_usage();
	exit(code);
}
void print_usage_error_and_exit(const std::string& msg, int code = 1)
{
	std::cerr << msg << '\n';
	std::cout << "Use the -h or --help option to print a detailed usage information.\n";
	exit(code);
}
void invalid_argument(const std::string& arg)
{
	print_usage_error_and_exit("Invalid argument: " + arg);
}


class argument_stream
{
	int argc;
	const char** argv;
	int pos;

public:
	argument_stream(int argc, const char** argv) :
		argc(argc),
		argv(argv),
		pos(1)
	{
		if (argc < 1)
			throw std::out_of_range("At least one argument required (self)");
	}

	std::string peek()
	{
		if (pos >= argc)
			throw std::out_of_range("Reached end of argument list");
		return argv[pos];
	}

	std::string next()
	{
		if (pos >= argc)
			throw std::out_of_range("Reached end of argument list");
		return argv[pos++];
	}

	unsigned long next_ulong()
	{
		std::string arg(next());
		try
		{
			int base = 10;
			if (arg.length() > 1 && arg[0] == '0') {
				if (arg[1] == 'x' || arg[1] == 'X')
					base = 16;
			}
			return std::stoul(arg, nullptr, base);
		}
		catch (const std::exception& exc)
		{
			throw std::invalid_argument("Unable to parse argument");
		}
	}

	bool has_next()
	{
		return pos < argc;
	}
};

std::string create_output_from_input(const std::string& input)
{
	if (options.mode == ExecutionMode::EXTRACT)
	{
		if (input.ends_with(".fpk")) {
			return input.substr(0, input.length() - 4);
		}
		else return input + "_out";
	}
	else if (options.mode == ExecutionMode::PACK)
		return input + ".fpk";
	return std::string();
}

void parse_args(int argc, const char** argv)
{
	argument_stream args(argc, argv);
	std::string arg;
	while (args.has_next())
	{
		arg = args.next();

		// flags
		if (arg == "-h" || arg == "--help")
			print_usage_and_exit();
		else if (arg == "-e" || arg == "--extract")
			options.mode = ExecutionMode::EXTRACT;
		else if (arg == "-p" || arg == "--pack")
			options.mode = ExecutionMode::PACK;
		else if (arg == "-l" || arg == "--list")
			options.mode = ExecutionMode::LIST;
		else if (arg == "-v" || arg == "--verbose")
			options.verbose = true;
		else if (arg == "-z" || arg == "--zlc")
			options.zlc = true;
		else if (arg == "-Z" || arg == "--Zlc")
			options.zlc = false;
		else if (arg == "-r" || arg == "--rle")
			options.rle = true;
		else if (arg == "-R" || arg == "--Rle")
			options.rle = false;

		// options and input
		else
		{
			try
			{
				if (arg == "-t" || arg == "--threads")
					options.threads = args.next_ulong();
				else if (arg == "-k" || arg == "--key")
					options.key = args.next_ulong();
				else if (arg == "-o" || arg == "--output")
					options.output = args.next();
				else
				{
					// no matching option found
					if (!args.has_next())
						// was last argument = input path
						options.input = arg;
					else
						invalid_argument(arg);
				}
			}
			catch (const std::out_of_range& exc)
			{
				print_usage_error_and_exit(std::string("The argument ") + arg + " requires a value!");
			}
			catch (const std::invalid_argument& exc)
			{
				print_usage_error_and_exit(std::string("The argument") + arg + " requires a 4 byte integer value!");
			}
		}
	}

	// validate input path
	if (options.input.length() == 0)
		print_usage_error_and_exit("An input argument is required.");
	if (!fs::exists(options.input))
	{
		std::cerr << "The input path does not exist: " << options.input;
		exit(1);
	}

	// check for output path if necessary
	if (options.mode != ExecutionMode::LIST && options.output.length() == 0) 
		options.output = create_output_from_input(options.input);
}


int main(int argc, const char** argv)
{
	try
	{
		parse_args(argc, argv);
	}
	catch (const std::exception& exc)
	{
		std::cerr << "An unexpected error occurred: " << exc.what() << std::endl;
		return 1;
	}

	if (options.verbose)
	{
		std::cout << "Running ";
		switch (options.mode)
		{
		case ExecutionMode::EXTRACT:
			std::cout << "extraction";
			break;
		case ExecutionMode::PACK:
			std::cout << "packing";
			break;
		case ExecutionMode::LIST:
			std::cout << "listing";
			break;
		default:
			std::cout << "unknown mode wtf\n";
			return 1;
		}
		std::cout << " mode with the following options:\n"
			<< "Input: " << options.input << '\n';

		if (options.mode != ExecutionMode::LIST)
			std::cout << "Output: " << options.output << '\n';

		std::cout << "Verbose: true\n";

		if (options.mode == ExecutionMode::PACK)
		{
			std::cout << "Compression threads: ";
			if (options.threads)
				std::cout << options.threads << '\n';
			else
				std::cout << "auto\n";
			std::cout
				<< "ZLC Compression: " << bool_to_str(options.zlc) << '\n'
				<< "RLE Compression: " << bool_to_str(options.rle) << '\n'
				<< "Obfuscation key: 0x" << std::right << std::hex << std::setfill('0') << std::setw(8) << options.key << std::dec << std::setfill(' ') << std::left << std::endl;
		}
		
		std::cout << std::endl;
	}

	try
	{
		if (options.mode == ExecutionMode::EXTRACT)
		{
			extract_fpk(options.input, options.output);
		}
		else if (options.mode == ExecutionMode::PACK)
		{
			pack_fpk(options.input, options.output);
		}
		else if (options.mode == ExecutionMode::LIST)
		{
			// TODO
			printf("Not implemented");
		}
	}
	catch (const std::ios::failure& fail)
	{
		std::cerr << "IO ERROR: " << fail.what() << '\n';
		return 1;
	}
	catch (const std::exception& exc)
	{
		std::cerr << "ERROR: " << exc.what() << '\n';
		return 1;
	}

	return 0;
}