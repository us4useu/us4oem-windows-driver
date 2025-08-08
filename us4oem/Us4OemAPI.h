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

// ====== Memory Mapping Area Definitions ======

typedef enum _us4oem_mmap_area {
    MMAP_AREA_BAR_0 = 0, // BAR 0 ("PCIDMA", 512 KiB = 0x0200)
    MMAP_AREA_BAR_4 = 1, // BAR 4 ("US4OEM", 64 MiB = 0x0400_0000)
    MMAP_AREA_DMA = 2, // Any DMA allocation (specify ID)
    MMAP_AREA_MAX = MMAP_AREA_DMA
} us4oem_mmap_area;

typedef struct _us4oem_mmap_argument {
    us4oem_mmap_area area;
    // TODO: ID for DMA regions

    unsigned long length_limit; // Maps the whole area if 0
} us4oem_mmap_argument;

typedef struct _us4oem_mmap_response {
    void* address;
    unsigned long length_mapped;
} us4oem_mmap_response;


typedef struct _us4oem_stats
{
    unsigned long irq_count; // Total number of IRQs handled

} us4oem_stats;