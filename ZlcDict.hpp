#pragma once

#include <deque>
#include <tuple>

#include <cstdint>

struct ZlcSearch
{
	std::tuple<const uint8_t*, uint8_t> find_best_match(
		const uint8_t* str,
		uint8_t len,
		const uint8_t* window,
		const uint8_t* win_end)
	{
		if (len == 0 || window == win_end)
			return { nullptr, 0 };

		const uint8_t* best_start = nullptr;
		uint8_t best_length = 0;
		uint8_t cur_length;
		uint8_t c;

		const uint8_t* pos;
		const uint8_t* end;
		const uint8_t* str_pos;
		while (window < win_end)
		{
			if (*window == *str)
			{
				pos = window + 1;
				end = window + len;
				if (end > win_end)
					end = win_end;
				str_pos = str + 1;
				cur_length = 1;

				while (pos < end && *pos == *str_pos)
				{
					pos++;
					str_pos++;
					cur_length++;
				}

				if (cur_length > best_length)
				{
					best_start = window;
					best_length = cur_length;
					if (best_length == len) // best possible match already found
						break;
				}
			}

			window++;
		}

		return { best_start, best_length };
	}

	void add(uint8_t c, const uint8_t* pos) {}
};

class ZlcDict
{
public:
	typedef const uint8_t* position_t;
	typedef position_t match_t;
	typedef std::deque<position_t> queue_t;

	queue_t characters[256];

	queue_t& operator[](uint8_t index) { return characters[index]; }
	const queue_t& operator[](uint8_t index) const { return characters[index]; }

	std::tuple<const uint8_t*, uint8_t> find_best_match(
		const uint8_t* str, 
		uint8_t len,
		const uint8_t* window, 
		const uint8_t* win_end)
	{
		const uint8_t c0 = *str;
		auto& entries = characters[c0];

		if (entries.empty())
			return { win_end, 0 };

		const uint8_t* best_match = nullptr;
		uint8_t length, best_length = 0;
		auto it = entries.begin();
		for (auto it_end = entries.end(); it != it_end; ++it)
		{
			const uint8_t* strpos = str + 1; // start at +1 offset because 0 is already known to be equal
			const uint8_t* winpos = *it;
			if (winpos < window) // elements are too old from here on
			{
				entries.erase(it, entries.end());
				break;
			}
			const uint8_t* end = std::min(win_end, winpos + len);
			winpos++;
			length = 1;
			while (winpos < end && *strpos == *winpos)
			{
				strpos++;
				winpos++;
				length++;
			}
			if (length == len) // cannot get better
				return { *it, len };
			if (length > best_length)
			{
				best_length = length;
				best_match = *it;
			}
		}

		return { best_match, best_length };
	}

	void add(const uint8_t* pos) 
	{
		characters[*pos].push_front(pos);
	}

};