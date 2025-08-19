#pragma once

#include "common.hpp"

// This is the type that will be used to represent a scatter-gather chunk.
struct Us4OemDmaSgChunk {
	void* va; // Virtual address of the allocated buffer - note: this is NOT mapped to user-mode memory
	size_t pa; // Physical address of the allocated buffer
	size_t length; // Length of this chunk
};