#pragma once
#include <vector>
#include <exception>

class rle
{
private:
	struct Rle0Header
	{
		int32_t signature; // "RLE0"
		uint32_t depth; // ?
		uint32_t length;
		uint32_t original_length;
		uint32_t is_compressed;
		uint32_t unknown2;
	};


public:
	static std::vector<uint8_t> compress(const std::vector<uint8_t>& input)
	{
		//Rle0Header hdr;
		//hdr.signature = '0ELR';
		//hdr.depth = 

		throw std::exception("RLE compression is not yet implemented.");
	}

	static std::vector<uint8_t> decompress(const std::vector<uint8_t>& input)
	{
		if (input.size() < sizeof(Rle0Header))
			return input; // TODO: avoid copy
		const uint8_t* p = input.data();
		const Rle0Header& hdr = *(const Rle0Header*)p;
		p += sizeof(Rle0Header);
		if (hdr.signature != '0ELR')
			return input; // TODO: avoid copy

		std::vector<uint8_t> output(hdr.original_length);

		if (hdr.is_compressed)
		{
			auto out_p = output.data();
			auto out_end = output.data() + output.size();
			while (out_p < out_end) {
				uint8_t  c = *p++;
				uint32_t n = c & 0x3F;
				c >>= 6;

				if (!n) {
					n = 0x40;
				}

				switch (c) {
				case 0:
					while (n--) {
						*out_p++ = *p++;
					}
					break;

				case 1:
				case 2:
				case 3:
					n++;
					while (n--) {
						for (uint32_t i = 0; i < c; i++) {
							*out_p++ = *(p + i);
						}
					}
					p += c;
					break;
				}
			}
		}
		else
		{
			memcpy(output.data(), p, output.size());
		}

		return output;
	}
};