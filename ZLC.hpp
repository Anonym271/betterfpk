#pragma once
#include <tuple>
#include <vector>
#include <cassert>

#include "ZlcDict.hpp"

class zlc
{
private:
	static constexpr size_t WINDOW_SIZE = 4096;
	static constexpr size_t MIN_LENGTH = 3;
	static constexpr size_t MAX_LENGTH = MIN_LENGTH + 15;

public:
	template <typename D>
	static std::vector<uint8_t> compress(const std::vector<uint8_t>& input)
	{
		D dict;

		// worst case: size = 9/8 * in_size; 10/8 for rounding - edit: wtf did I do here? 10/8*in_size vs. 3*in_size? Whatever, it is working... 
		// use minimum size of 1KB for small files (ISSUE #1)
		std::vector<uint8_t> output(std::max<size_t>(input.size() * 3, 1024));

		// write header
		uint32_t* out_32 = (uint32_t*)output.data();
		*out_32++ = (uint32_t)'2CLZ';
		*out_32++ = (uint32_t)input.size();
		
		// compress
		const uint8_t* in_start = input.data();
		const uint8_t* in_pos = in_start;
		const uint8_t* in_end = in_start + input.size();
		uint8_t* out_pos = (uint8_t*)out_32;

		const uint8_t* window;
		const uint8_t* win_start;
		const uint8_t* win_end;

		int count = 0;
		uint8_t flag_pos = 0x80;
		uint8_t flag = 0;
		uint8_t* flag_offset = out_pos++; // first byte will be flags
		const uint8_t* end;
		int str_len;
		int match_offs;
		uint16_t control;

		uint8_t control1, control2;

		while (in_pos < in_end)
		{
			if (!flag_pos) // 8 flags set
			{
				*flag_offset = flag;
				flag_offset = out_pos++;
				flag = 0;
				flag_pos = 0x80;
			}

			end = in_pos + MAX_LENGTH;
			if (end > in_end)
				end = in_end;
			str_len = end - in_pos;
			win_start = in_pos - WINDOW_SIZE;
			if (win_start < in_start)
				win_start = in_start;
			win_end = in_pos;

			auto [match_start, match_length] = dict.find_best_match(in_pos, str_len, win_start, win_end);

			if (match_length >= MIN_LENGTH) // match found, save compressed
			{
				flag |= flag_pos; // set compression flag
				str_len = match_length - 3;
				match_offs = in_pos - match_start;

				control1 = match_offs;
				control2 = (match_offs >> 4) & 0xF0;
				control2 |= str_len;

				*out_pos++ = control1;
				*out_pos++ = control2;
				while (match_length != 0)
				{
					dict.add(in_pos);
					++in_pos;
					--match_length;
				}
			}
			else // no (sufficient) match found, save uncompressed
			{
				dict.add(in_pos);
				*out_pos++ = *in_pos++;
			}
			flag_pos >>= 1;
		}
		*flag_offset = flag; // set flags one last time!!!

		size_t total_size = out_pos - output.data();
		output.resize(total_size);
		return output;
	}

	static std::vector<uint8_t> decompress(const std::vector<uint8_t>& input)
	{
		// read header
		if (input.size() < 8)
			return input; // TODO: avoid copy
		uint32_t* in_32 = (uint32_t*)input.data();
		uint32_t magic = *in_32++;
		if (magic != (uint32_t)'2CLZ')
			return input; // TODO: avoid copy
		uint32_t original_size = *in_32++;

		// decompress
		std::vector<uint8_t> output(original_size);
		uint32_t out_len = output.size();
		auto     out_buff = output.data();
		auto buff = input.data();
		auto len = input.size();
		auto in_p = (uint8_t*)in_32;

		auto end = buff + len;
		auto out_p = out_buff;
		auto out_end = out_buff + out_len;

		while (in_p < end && out_p < out_end) {
			uint8_t flags = *in_p++;

			for (int i = 0; i < 8 && in_p < end && out_p < out_end; i++) {
				if (flags & 0x80) {
					if (end - in_p < 2)
						break;

					uint32_t offset = *in_p++;
					uint32_t cnt = *in_p++;

					offset |= (cnt & 0xF0) << 4;
					cnt = (cnt & 0x0F) + 3;

					if (offset == 0) {
						offset = 4096;
					}

					for (uint32_t j = 0; j < cnt && out_p < out_end; j++) {
						*out_p = *(out_p - offset);
						*out_p++;
					}
				}
				else {
					*out_p++ = *in_p++;
				}

				flags <<= 1;
			}
		}

		return output;
	}
};