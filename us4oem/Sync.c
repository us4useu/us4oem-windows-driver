#include "ioctl.h"
#include "sync.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemIoctlPoll)
#pragma alloc_text (PAGE, us4oemIoctlPollNonBlocking)
#pragma alloc_text (PAGE, us4oemIoctlClearPending)
#endif

VOID us4oemIoctlPoll(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(OutputBuffer);

    PAGED_CODE();

    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

    // If there are IRQs left to be processed, we can complete the request immediately
    if (deviceContext->Stats.irq_pending_count > 0) {
        WdfRequestComplete(Request, STATUS_SUCCESS);
        deviceContext->Stats.irq_pending_count--;
        return;
    }

    // Wait for the interrupt to be signaled
    if (deviceContext->PendingRequest) {
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