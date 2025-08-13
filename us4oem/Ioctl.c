#include "ioctl.h"
#include "ioctl.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemIoctlGetDriverInfo)
#pragma alloc_text (PAGE, us4oemIoctlMmap)
#pragma alloc_text (PAGE, us4oemIoctlReadStats)
#pragma alloc_text (PAGE, us4oemIoctlPoll)
#pragma alloc_text (PAGE, us4oemIoctlPollNonBlocking)
#pragma alloc_text (PAGE, us4oemIoctlClearPending)
#pragma alloc_text (PAGE, us4oemIoctlAllocateDmaContiguousBuffer)
#pragma alloc_text (PAGE, us4oemIoctlDeallocateContigousDmaBuffer)
#pragma alloc_text (PAGE, us4oemIoctlDeallocateAllDmaBuffers)
#endif

#define DRIVER_INFO_STRING "us4oem win32 driver"

IOCTL_HANDLER handlers[] = {
    {
        US4OEM_WIN32_IOCTL_GET_DRIVER_INFO,
        0, // No input buffer needed
        sizeof(DRIVER_INFO_STRING), // Output buffer size
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

VOID us4oemIoctlDeallocateAllDmaBuffers(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
    PAGED_CODE();

    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(InputBuffer);

	PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

    // Iterate over the linked list of common buffers and delete each one
    LINKED_LIST_FOR_EACH(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers, commonBuffer) {
        if (commonBuffer->Item != NULL) {
            WdfObjectDelete(*commonBuffer->Item);
            deviceContext->Stats.dma_contig_free_count++;
            deviceContext->Stats.dma_contig_alloc_count--;
        }
	}

    // Clear the linked list
    LINKED_LIST_CLEAR(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers);
    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_IOCTL,
        "Deallocated all DMA buffers");
	WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID us4oemIoctlDeallocateContigousDmaBuffer(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
    PAGED_CODE();

	UNREFERENCED_PARAMETER(OutputBuffer);

    long long pa = *(unsigned long long*)InputBuffer;
    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

	// Iterate over the contiguous buffers until we find the one with the matching PA
    LINKED_LIST_FOR_EACH(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers, commonBuffer) {
        if (commonBuffer->Item != NULL && 
            WdfCommonBufferGetAlignedLogicalAddress(*(commonBuffer->Item)).QuadPart == pa) {
            // Found the buffer, delete it
            WdfObjectDelete(*commonBuffer->Item);
            deviceContext->Stats.dma_contig_free_count++;
			deviceContext->Stats.dma_contig_alloc_count--;
            LINKED_LIST_REMOVE(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers, commonBuffer);
            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_IOCTL,
                "Deallocated contiguous DMA buffer with PA: 0x%llx",
                pa);
            WdfRequestComplete(Request, STATUS_SUCCESS);
            return;
        }
	}

	// Not found, complete with an error
    TraceEvents(TRACE_LEVEL_ERROR,
        TRACE_IOCTL,
        "Failed to find contiguous DMA buffer with PA: 0x%llx",
        pa);
    WdfRequestComplete(Request, STATUS_NOT_FOUND);
}

VOID us4oemIoctlAllocateDmaContiguousBuffer(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
    PAGED_CODE();

	us4oem_dma_allocation_argument* arg = (us4oem_dma_allocation_argument*)InputBuffer;

	PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

	// Allocate memory for the common buffer struct
    WDFCOMMONBUFFER* commonBuffer = MmAllocateNonCachedMemory(sizeof(WDFCOMMONBUFFER));

    NTSTATUS status = WdfCommonBufferCreate(deviceContext->DmaEnabler,
        arg->length,
        WDF_NO_OBJECT_ATTRIBUTES,
        commonBuffer);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_IOCTL,
            "WdfCommonBufferCreate failed with status: %!STATUS!",
            status);
		MmFreeNonCachedMemory(commonBuffer, sizeof(WDFCOMMONBUFFER));
        WdfRequestComplete(Request, status);
        return;
	}

	us4oem_dma_contiguous_buffer_response* response = (us4oem_dma_contiguous_buffer_response*)OutputBuffer;

	response->va = WdfCommonBufferGetAlignedVirtualAddress(
        *commonBuffer);

	// PHYSICAL_ADDRESS is technically a union, so we have to get the 64-bit int from QuadPart
    response->pa = (WdfCommonBufferGetAlignedLogicalAddress(
        *commonBuffer)).QuadPart;

	// Store information about the allocated buffer in the device context
	LINKED_LIST_PUSH(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers, commonBuffer);

    deviceContext->Stats.dma_contig_alloc_count++;

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(us4oem_dma_contiguous_buffer_response));
}

VOID us4oemIoctlGetDriverInfo(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(Device);

	PAGED_CODE();

    char driverInfo[] = DRIVER_INFO_STRING; // TODO: Include some meaningful driver info, version?

    RtlCopyMemory(OutputBuffer, driverInfo, sizeof(driverInfo));

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_IOCTL,
        "Driver info sent: %s",
        driverInfo);

    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(driverInfo));
}

VOID us4oemIoctlPoll(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(OutputBuffer);

    PAGED_CODE();

    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

	// If there are IRQs left to be processed, we can complete the request immediately
    if (deviceContext->Stats.irq_pending_count > 0) {
        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_IOCTL,
            "IRQ pending count is %d, completing request immediately",
            deviceContext->Stats.irq_pending_count);
        WdfRequestComplete(Request, STATUS_SUCCESS);
        deviceContext->Stats.irq_pending_count--;
        return;
	}

    // Wait for the interrupt to be signaled
    if (deviceContext->PendingRequest) {
        TraceEvents(TRACE_LEVEL_WARNING,
            TRACE_IOCTL,
            "A request is already pending, completing with STATUS_DEVICE_BUSY");
        WdfRequestComplete(Request, STATUS_DEVICE_BUSY);
        return;
    }
    deviceContext->PendingRequest = Request;
}

VOID us4oemIoctlPollNonBlocking(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(OutputBuffer);

    PAGED_CODE();

    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

    // If there are IRQs left to be processed, we can complete the request immediately
    if (deviceContext->Stats.irq_pending_count > 0) {
        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_IOCTL,
            "IRQ pending count is %d, completing request immediately",
            deviceContext->Stats.irq_pending_count);
        WdfRequestComplete(Request, STATUS_SUCCESS);
        deviceContext->Stats.irq_pending_count--;
        return;
    }
    // No pending IRQs, complete with STATUS_DEVICE_BUSY
    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_IOCTL,
        "No pending IRQs, completing request with STATUS_DEVICE_BUSY");
    WdfRequestComplete(Request, STATUS_DEVICE_BUSY);
}

VOID us4oemIoctlClearPending(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(InputBuffer);

    PAGED_CODE();

    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

    // Clear the pending IRQs
    deviceContext->Stats.irq_pending_count = 0;

    WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID us4oemIoctlMmap(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
	PAGED_CODE();

    us4oem_mmap_argument arg = *(us4oem_mmap_argument*)InputBuffer;

    if (arg.area > MMAP_AREA_MAX) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_IOCTL,
            "Area %d is out of range (0-%d)",
            arg.area,
            MMAP_AREA_MAX);
        WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
        return;
    }

    // Check if the bar index is valid and map the corresponding BAR to user-mode memory.
    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

	PVOID address = NULL;
	ULONG length = 0;

    switch (arg.area) {
    case MMAP_AREA_BAR_0:
        if (!deviceContext->BarPciDma.MappedAddress) {
            WdfRequestComplete(Request, STATUS_DEVICE_UNREACHABLE);
        }
        address = deviceContext->BarPciDma.MappedAddress;
        length = deviceContext->BarPciDma.Length;
        break;
    case MMAP_AREA_BAR_4:
        if (!deviceContext->BarUs4Oem.MappedAddress) {
            WdfRequestComplete(Request, STATUS_DEVICE_UNREACHABLE);
        }
        address = deviceContext->BarUs4Oem.MappedAddress;
        length = deviceContext->BarUs4Oem.Length;
        break;
    case MMAP_AREA_DMA:
        if (!arg.va) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_IOCTL,
                "Virtual address for DMA area is NULL");
            WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
            return;
        }

        // For DMA areas, we assume the user has provided a valid virtual address
        address = arg.va;
        length = arg.length_limit; // Use the length limit provided by the user
        break;
    }

    // To map the BAR to user-mode we need to:
    // - Build an MDL (MmBuildMdlForNonPagedPool)
    // - Map the MDL to user-mode memory (MmMapLockedPagesSpecifyCache)

    if (length > arg.length_limit && arg.length_limit != 0) {
        length = arg.length_limit; // Use the user-provided length limit
	}

    PMDL mdl = IoAllocateMdl(
        address,
        length,
        FALSE,
        FALSE,
        NULL
    );

    if (!mdl) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_IOCTL,
            "IoAllocateMdl failed to allocate MDL for area %d",
            arg.area);
        WdfRequestComplete(Request, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }
    __try {
        MmBuildMdlForNonPagedPool(mdl);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        IoFreeMdl(mdl);
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_IOCTL,
            "MmBuildMdlForNonPagedPool failed for area %d",
            arg.area);
        WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
        return;
    }

    PVOID mappedAddress = MmMapLockedPagesSpecifyCache(
        mdl,
        UserMode, // Map to user-mode
        MmNonCached, // Non-cached mapping
        NULL,
        FALSE,
        NormalPagePriority
    );

    if (!mappedAddress) {
        IoFreeMdl(mdl);
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_IOCTL,
            "MmMapLockedPagesSpecifyCache failed for area %d",
            arg.area);
        WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
        return;
    }

    // Copy the mapped address to the output buffer
    ((us4oem_mmap_response*)OutputBuffer)->address = mappedAddress;
    ((us4oem_mmap_response*)OutputBuffer)->length_mapped = length;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_IOCTL,
        "area %d mapped to user-mode memory at address %p",
        arg.area, mappedAddress);

    // TODO: (probably) a bunch of memory leaks that need to be handled properly

    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(us4oem_mmap_response));
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