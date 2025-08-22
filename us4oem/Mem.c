#include "ioctl.h"
#include "mem.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemIoctlMmap)
#endif

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

		// Try to find the DMA area by virtual address
		address = arg.va; // Use the virtual address provided by the user

		BOOLEAN found = FALSE;
        LINKED_LIST_FOR_EACH(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers, commonBuffer) {
            if (found) {
                break; // No need to continue if we already found it
            }
            if (commonBuffer->Item != NULL &&
                WdfCommonBufferGetAlignedVirtualAddress(*commonBuffer->Item) == address) {
                length = (ULONG)WdfCommonBufferGetLength(*commonBuffer->Item);
				found = TRUE;
                break;
            }
		}
        LINKED_LIST_FOR_EACH(MEMORY_ALLOCATION, deviceContext->DmaScatterGatherMemory, commonBuffer) {
            if (found) {
                break; // No need to continue if we already found it
			}
            size_t size;
            if (commonBuffer->Item != NULL &&
                WdfMemoryGetBuffer(commonBuffer->Item->memory, &size) == address) {
                length = (ULONG)size;
                found = TRUE;
                break;
            }
        }

        if (length == 0) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_IOCTL,
                "Failed to find DMA area with VA %p",
                address);
            WdfRequestComplete(Request, STATUS_NOT_FOUND);
            return;
		}

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