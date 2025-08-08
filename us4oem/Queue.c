#include "driver.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemQueueInitialize)
#endif

NTSTATUS
us4oemQueueInitialize(
    _In_ WDFDEVICE Device
    )
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;

    PAGED_CODE();

    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
         &queueConfig,
        WdfIoQueueDispatchParallel
        );

    queueConfig.EvtIoDeviceControl = us4oemEvtIoDeviceControl;
    queueConfig.EvtIoStop = us4oemEvtIoStop;

    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 WDF_NO_OBJECT_ATTRIBUTES,
                 &queue
                 );

    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    return status;
}

VOID
us4oemEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    PVOID OutputBuffer = NULL;
	PVOID InputBuffer = NULL;
    NTSTATUS Status;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_QUEUE,
        "%!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d",
        Queue, Request, (int)OutputBufferLength, (int)InputBufferLength, IoControlCode);

	// Retrieve the output and input buffers if they are of non-zero length.
    if (OutputBufferLength > 0) {
        Status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &OutputBuffer, NULL);
        if (!NT_SUCCESS(Status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestRetrieveOutputBuffer failed despite OutputBufferLength>0, status=%!STATUS!",
                Status);
            WdfRequestComplete(Request, Status);
            return;
        }
    }

	if (InputBufferLength > 0) {
		Status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &InputBuffer, NULL);
		if (!NT_SUCCESS(Status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestRetrieveInputBuffer failed despite InputBufferLength>0, status=%!STATUS!",
				Status);
			WdfRequestComplete(Request, Status);
			return;
		}
	}

    switch (IoControlCode) {
        case US4OEM_WIN32_IOCTL_GET_DRIVER_INFO:
        {
            char driverInfo[] = "us4oem win32 driver"; // TODO: Include some meaningful driver info, version?

            if (OutputBufferLength < sizeof(driverInfo)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "OutputBufferLength %d is too small for driver info",
                    (int)OutputBufferLength);
                WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
                return;
            }

            RtlCopyMemory(OutputBuffer, driverInfo, sizeof(driverInfo));

            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_QUEUE,
                "Driver info sent: %s",
                driverInfo);

            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(driverInfo));
            return;
        }
        case US4OEM_WIN32_IOCTL_MMAP:
        {
            if (InputBufferLength < sizeof(us4oem_mmap_argument)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "InputBufferLength %d is too small for us4oem_mmap_argument",
                    (int)InputBufferLength);
                WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
                return;
            }

            us4oem_mmap_argument arg = *(us4oem_mmap_argument*)InputBuffer;

            if (arg.area > MMAP_AREA_MAX) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "Area %d is out of range (0-%d)",
                    arg.area,
                    MMAP_AREA_MAX);
                WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
                return;
			}

			// Check if the bar index is valid and map the corresponding BAR to user-mode memory.
			PUS4OEM_CONTEXT deviceContext = us4oemGetContext(WdfIoQueueGetDevice(Queue));

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
                    break;
            }

			// Check if there's enough space in the output buffer
            if (OutputBufferLength < sizeof(us4oem_mmap_response)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "OutputBufferLength %d is too small for us4oem_mmap_response",
                    (int)OutputBufferLength);
                WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
                return;
			}

			// To map the BAR to user-mode we need to:
            // - Build an MDL (MmBuildMdlForNonPagedPool)
			// - Map the MDL to user-mode memory (MmMapLockedPagesSpecifyCache)

            ULONG length = (arg.length_limit != 0 && bar->Length > arg.length_limit) ? 
                arg.length_limit : bar->Length;

			PMDL mdl = IoAllocateMdl(
				bar->MappedAddress,
                length,
				FALSE,
				FALSE,
				NULL
			);

            if (!mdl) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "IoAllocateMdl failed to allocate MDL for area %d",
                    arg.area);
                WdfRequestComplete(Request, STATUS_INSUFFICIENT_RESOURCES);
                return;
            }
            __try {
                MmBuildMdlForNonPagedPool(mdl);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                IoFreeMdl(mdl);
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
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
                    TRACE_QUEUE,
                    "MmMapLockedPagesSpecifyCache failed for area %d",
                    arg.area);
                WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
                return;
			}

            // Copy the mapped address to the output buffer
            ((us4oem_mmap_response*)OutputBuffer)->address = mappedAddress;
            ((us4oem_mmap_response*)OutputBuffer)->length_mapped = length;

            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_QUEUE,
                "area %d mapped to user-mode memory at address %p",
                arg.area, mappedAddress);

			// TODO: (probably) a bunch of memory leaks that need to be handled properly

            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(us4oem_mmap_response));
            return;
		}
        case US4OEM_WIN32_IOCTL_READ_STATS:
        {
            // Check if there's enough space in the output buffer
            if (OutputBufferLength < sizeof(us4oem_stats)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "OutputBufferLength %d is too small for us4oem_stats",
                    (int)OutputBufferLength);
                WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
                return;
            }

			PUS4OEM_CONTEXT deviceContext = us4oemGetContext(WdfIoQueueGetDevice(Queue));

            // Copy the stats to the output buffer
            RtlCopyMemory(OutputBuffer, &deviceContext->Stats, sizeof(us4oem_stats));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(us4oem_stats));
			return;
        }
        default:
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "Unsupported IoControlCode %d",
                IoControlCode);
            WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
            return;
        }
    }

	// Should be unreachable
    return;
}


VOID
us4oemEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", 
                Queue, Request, ActionFlags);

    return;
}
