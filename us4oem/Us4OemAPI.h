// This file is shared between user mode and kernel mode.
// No kernel mode specific code should be added here.
#pragma once

#ifndef DONT_INCLUDE_INITGUID
// This header is needed for GUIDs to work properly in win32 user mode applications,
// however it's possible that an app would want to avoid using the regular Windows SDK,
// so leave an option to not include it. Note that in that case the app must provide
// a macro for DEFINE_GUID.
#include <initguid.h> 
#endif

//
// Define an Interface Guid so that apps can find the device and talk to it.
//
DEFINE_GUID (GUID_DEVINTERFACE_us4oem,
    0x5d6d47d5,0x5cfa,0x48d0,0x9e,0x12,0xa5,0x10,0xed,0xe8,0x66,0xbd);

#define US4OEM_WIN32_IOCTL_BASE 0xA00 // Arbitrary base value for IOCTLs

// Read the driver information. Returns a null-terminated string.
#define US4OEM_WIN32_IOCTL_GET_DRIVER_INFO \
    CTL_CODE(FILE_DEVICE_UNKNOWN, US4OEM_WIN32_IOCTL_BASE + 0, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Map an area (DMA buffer/BAR) to user-mode memory. Call with us4oem_mmap_argument in the input buffer.
// Returns us4oem_mmap_response in the output buffer.
#define US4OEM_WIN32_IOCTL_MMAP \
    CTL_CODE(FILE_DEVICE_UNKNOWN, US4OEM_WIN32_IOCTL_BASE + 1, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Read statistics from the device. Returns us4oem_stats in the output buffer.
#define US4OEM_WIN32_IOCTL_READ_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, US4OEM_WIN32_IOCTL_BASE + 2, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Synchronization mechanism for user-mode applications to access the device.
// The request will complete when there's a pending IRQ to be handled, if there's none it will wait
// until an IRQ is received. Note this might block the thread, so use with caution.
#define US4OEM_WIN32_IOCTL_POLL \
    CTL_CODE(FILE_DEVICE_UNKNOWN, US4OEM_WIN32_IOCTL_BASE + 3, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Non-blocking version of the above. It will complete immediately if there's a pending IRQ,
// otherwise it will complete with STATUS_DEVICE_BUSY.
#define US4OEM_WIN32_IOCTL_POLL_NONBLOCKING \
    CTL_CODE(FILE_DEVICE_UNKNOWN, US4OEM_WIN32_IOCTL_BASE + 4, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Clear pending IRQs. Note that this will not complete any poll requests.
#define US4OEM_WIN32_IOCTL_CLEAR_PENDING \
    CTL_CODE(FILE_DEVICE_UNKNOWN, US4OEM_WIN32_IOCTL_BASE + 5, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Allocate a contiguous DMA buffer. Call with us4oem_dma_allocation_argument in the input buffer.
// Returns us4oem_dma_contiguous_buffer_response in the output buffer.
#define US4OEM_WIN32_IOCTL_ALLOCATE_DMA_CONTIGIOUS_BUFFER \
    CTL_CODE(FILE_DEVICE_UNKNOWN, US4OEM_WIN32_IOCTL_BASE + 6, METHOD_BUFFERED, FILE_ANY_ACCESS)

// <== (US4OEM_WIN32_IOCTL_BASE + 7) reserved for Scatter-Gather DMA alloc ==>

// Deallocate a contiguous DMA buffer. Call with unsigned long long PA in the input buffer.
#define US4OEM_WIN32_IOCTL_DEALLOCATE_DMA_CONTIGIOUS_BUFFER \
    CTL_CODE(FILE_DEVICE_UNKNOWN, US4OEM_WIN32_IOCTL_BASE + 8, METHOD_BUFFERED, FILE_ANY_ACCESS)

// <== (US4OEM_WIN32_IOCTL_BASE + 9) reserved for Scatter-Gather DMA dealloc ==>

// Deallocate all DMA buffers allocated by the device.
#define US4OEM_WIN32_IOCTL_DEALLOCATE_ALL_DMA_BUFFERS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, US4OEM_WIN32_IOCTL_BASE + 10, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ====== Memory Mapping Area Definitions ======

typedef enum _us4oem_mmap_area {
    MMAP_AREA_BAR_0 = 0, // BAR 0 ("PCIDMA", 512 KiB = 0x0200)
    MMAP_AREA_BAR_4 = 1, // BAR 4 ("US4OEM", 64 MiB = 0x0400_0000)
    MMAP_AREA_DMA = 2, // Any DMA allocation (specify VA)
    MMAP_AREA_MAX = MMAP_AREA_DMA
} us4oem_mmap_area;

typedef struct _us4oem_mmap_argument {
    us4oem_mmap_area area;
	void* va; // Virtual address for DMA allocations

    unsigned long length_limit; // Maps the whole area if 0
} us4oem_mmap_argument;

typedef struct _us4oem_mmap_response {
    void* address;
    unsigned long length_mapped;
} us4oem_mmap_response;

// ====== Statistics Structure ======

typedef struct _us4oem_stats
{
    unsigned long irq_count; // Total number of IRQs received
	unsigned long irq_pending_count; // Number of IRQs pending to be handled

	unsigned long dma_contig_alloc_count; // Number of contiguous DMA buffers currently allocated
	unsigned long dma_contig_free_count; // Number of DMA buffers freed total

} us4oem_stats;

// ====== DMA Allocation Structure ======

typedef struct _us4oem_dma_allocation_argument {
    unsigned long length; // Length of the DMA buffer to allocate
} us4oem_dma_allocation_argument;

typedef struct _us4oem_dma_contiguous_buffer_response {
	void* va; // Virtual address of the allocated buffer - note: this is NOT mapped to user-mode memory
    unsigned long long pa; // Physical address of the allocated buffer
} us4oem_dma_contiguous_buffer_response;