#pragma once

#include "common.hpp"

class Us4OemDeviceStats {
public:
	Us4OemDeviceStats(us4oem_stats raw) :
		irqCount(raw.irq_count),
		pendingIrqCount(raw.irq_pending_count),
		dmaContigAllocCount(raw.dma_contig_alloc_count),
		dmaContigFreeCount(raw.dma_contig_free_count),
		dmaSgAllocCount(raw.dma_sg_alloc_count),
		dmaSgFreeCount(raw.dma_sg_free_count),
		fileOpenCount(raw.file_open_count) {
	}

	std::string toString() const {
		return std::format("  IRQ Count: {}\n"
			"  Pending IRQ Count: {}\n"
			"  Contiguous DMA Allocations: {}\n"
			"  Contiguous DMA Frees: {}\n"
			"  SG DMA Allocations: {}\n"
			"  SG DMA Frees: {}\n"
			"  File Open Count: {}",
			irqCount,
			pendingIrqCount,
			dmaContigAllocCount,
			dmaContigFreeCount,
			dmaSgAllocCount,
			dmaSgFreeCount,
			fileOpenCount);
	}

	// Note: public, as this is more of a struct than a class.
	size_t irqCount; // Total number of IRQs received
	size_t pendingIrqCount; // Number of IRQs pending to be handled

	size_t dmaContigAllocCount; // Number of contiguous DMA buffers currently allocated
	size_t dmaContigFreeCount; // Number of DMA buffers freed total
	size_t dmaSgAllocCount; // Number of scatter-gather DMA buffers currently allocated
	size_t dmaSgFreeCount; // Number of scatter-gather DMA buffers freed total

	size_t fileOpenCount; // Number of times the device char device has been opened to be used by a client
};