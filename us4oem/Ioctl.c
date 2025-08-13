#include "ioctl.h"
#include "ioctl.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemIoctlGetDriverInfo)
#pragma alloc_text (PAGE, us4oemIoctlReadStats)
#endif

IOCTL_HANDLER handlers[] = {
    {
        US4OEM_WIN32_IOCTL_GET_DRIVER_INFO,
        0, // No input buffer needed
        sizeof(us4oem_driver_info), // Output buffer size
        us4oemIoctlGetDriverInfo
    },
    {
        US4OEM_WIN32_IOCTL_MMAP,
        sizeof(us4oem_mmap_argument), // Input buffer size
        sizeof(us4oem_mmap_response), // Output buffer size
        us4oemIoctlMmap
    },
    {
        US4OEM_WIN32_IOCTL_READ_STATS,
        0, // No input buffer needed
        sizeof(us4oem_stats), // Output buffer size
        us4oemIoctlReadStats
    },
    {
        US4OEM_WIN32_IOCTL_POLL,
        0, // No input buffer needed
        0, // No output buffer needed
        us4oemIoctlPoll
    },
    {
        US4OEM_WIN32_IOCTL_POLL_NONBLOCKING,
        0, // No input buffer needed
        0, // No output buffer needed
		us4oemIoctlPollNonBlocking
    },
    {
        US4OEM_WIN32_IOCTL_CLEAR_PENDING,
        0, // No input buffer needed
        0, // No output buffer needed
        us4oemIoctlClearPending
    },
    {
        US4OEM_WIN32_IOCTL_ALLOCATE_DMA_CONTIGIOUS_BUFFER,
		sizeof(us4oem_dma_allocation_argument), // Input buffer size
		sizeof(us4oem_dma_contiguous_buffer_response), // Output buffer size
        us4oemIoctlAllocateDmaContiguousBuffer
    },
    {
        US4OEM_WIN32_IOCTL_DEALLOCATE_DMA_CONTIGIOUS_BUFFER,
		sizeof(unsigned long long), // Input buffer size - PA of the allocated buffer
		0, // No output buffer needed
		us4oemIoctlDeallocateContigousDmaBuffer
    },
    {
        US4OEM_WIN32_IOCTL_DEALLOCATE_ALL_DMA_BUFFERS,
        0, // No input buffer needed
        0, // No output buffer needed
        us4oemIoctlDeallocateAllDmaBuffers
    }
};

PIOCTL_HANDLER us4oemGetIoctlHandler() {
	return handlers;
}

ULONG us4oemGetIoctlHandlerCount() {
	return sizeof(handlers) / sizeof(IOCTL_HANDLER);
}

VOID us4oemIoctlGetDriverInfo(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(Device);

	PAGED_CODE();

	us4oem_driver_info* driverInfo = (us4oem_driver_info*)OutputBuffer;

	driverInfo->version = US4OEM_DRIVER_VERSION;

	RtlCopyBytes(driverInfo->name, US4OEM_DRIVER_INFO_STRING, sizeof(US4OEM_DRIVER_INFO_STRING));

    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(us4oem_driver_info));
}

VOID us4oemIoctlReadStats(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
    UNREFERENCED_PARAMETER(InputBuffer);

    PAGED_CODE();

    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

    // Copy the stats to the output buffer
    RtlCopyMemory(OutputBuffer, &deviceContext->Stats, sizeof(us4oem_stats));
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(us4oem_stats));
}