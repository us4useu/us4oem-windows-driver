#include "ioctl.h"
#include "ioctl.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemIoctlGetDriverInfo)
#pragma alloc_text (PAGE, us4oemIoctlMmap)
#pragma alloc_text (PAGE, us4oemIoctlReadStats)
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

    char driverInfo[] = DRIVER_INFO_STRING; // TODO: Include some meaningful driver info, version?

    RtlCopyMemory(OutputBuffer, driverInfo, sizeof(driverInfo));

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_IOCTL,
        "Driver info sent: %s",
        driverInfo);

    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(driverInfo));
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

    PBAR_INFO bar = NULL;

    switch (arg.area) {
    case MMAP_AREA_BAR_0:
        if (!deviceContext->BarPciDma.MappedAddress) {
            WdfRequestComplete(Request, STATUS_DEVICE_UNREACHABLE);
        }
        bar = &deviceContext->BarPciDma;
        break;
    case MMAP_AREA_BAR_4:
        if (!deviceContext->BarUs4Oem.MappedAddress) {
            WdfRequestComplete(Request, STATUS_DEVICE_UNREACHABLE);
        }
        bar = &deviceContext->BarUs4Oem;
        break;
    case MMAP_AREA_DMA:
        // TODO: Implement
        WdfRequestComplete(Request, STATUS_NOT_IMPLEMENTED);
        return;
    }

    // To map the BAR to user-mode we need to:
    // - Build an MDL (MmBuildMdlForNonPagedPool)
    // - Map the MDL to user-mode memory (MmMapLockedPagesSpecifyCache)

#pragma warning(disable: 6011, justification: "there's no case in which bar is NULL and execution gets here")
    ULONG length = (arg.length_limit != 0 && bar->Length > arg.length_limit) ?
        arg.length_limit : bar->Length;

    PMDL mdl = IoAllocateMdl(
        bar->MappedAddress,
        length,
        FALSE,
        FALSE,
        NULL
    );
#pragma warning(default: 6011)

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