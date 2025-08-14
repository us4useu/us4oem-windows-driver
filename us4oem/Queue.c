#include "driver.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemQueueInitialize)
#pragma alloc_text (PAGE, us4oemEvtIoDeviceControl)
#pragma alloc_text (PAGE, us4oemEvtIoStop)
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

    PAGED_CODE();

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

	PIOCTL_HANDLER handlers = us4oemGetIoctlHandler();

    for (size_t i = 0; i < us4oemGetIoctlHandlerCount(); i++) {
        if (handlers[i].IoControlCode == IoControlCode) {
            // Check if the input and output buffer sizes are sufficient
            if (InputBufferLength < handlers[i].InputBufferNeeded ||
                OutputBufferLength < handlers[i].OutputBufferNeeded) {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "Input/Output buffer size is insufficient for IoControlCode %d",
                    IoControlCode);
                WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
                return;
            }

            if (handlers[i].HandlerFuncWithBufferSizes) {
                // If the handler uses dynamic buffer sizes, call it with the actual sizes
                handlers[i].HandlerFuncWithBufferSizes(WdfIoQueueGetDevice(Queue), Request, OutputBuffer, InputBuffer, OutputBufferLength, InputBufferLength);
			} else {
                // Otherwise, just call the handler without buffer sizes
                handlers[i].HandlerFunc(WdfIoQueueGetDevice(Queue), Request, OutputBuffer, InputBuffer);
			}

            return;
        }
	}

    TraceEvents(TRACE_LEVEL_ERROR,
        TRACE_QUEUE,
        "Unsupported IoControlCode %d",
        IoControlCode);
    WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
    return;
}

VOID
us4oemEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", 
                Queue, Request, ActionFlags);

    return;
}
