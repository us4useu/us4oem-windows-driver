#pragma once

#include "common.hpp"

// This is the type that will be used to represent a scatter-gather chunk.
struct Us4OemDmaSgChunk {
	size_t vaOffset; // Offset from the top VA
	size_t pa; // Physical address of the allocated buffer
	size_t length; // Length of this chunk
};

// This is the type that will be used to return info from the SDK function
struct Us4OemDmaSgDescription {
	void* va; // VA of the allocated buffer
	size_t length; // Total length of all allocated chunks
	std::vector<Us4OemDmaSgChunk> chunks; // The allocated chunks
};