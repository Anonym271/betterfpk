#pragma once
#include <string>
#include <cstdint>

enum class ExecutionMode
{
	EXTRACT,
	PACK,
	LIST
};


struct Options 
{
	ExecutionMode mode = ExecutionMode::EXTRACT;
	bool verbose = false;
	bool rle = false;
	bool zlc = true;
	int threads = 0;
	int version = 2;
	uint32_t key = 0;
	std::string input;
	std::string output;
};

extern Options options;