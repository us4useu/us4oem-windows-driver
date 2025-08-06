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
