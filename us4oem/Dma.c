#include "ioctl.h"
#include "dma.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemIoctlAllocateDmaContiguousBuffer)
#pragma alloc_text (PAGE, us4oemIoctlDeallocateContigousDmaBuffer)
#pragma alloc_text (PAGE, us4oemIoctlDeallocateAllDmaBuffers)
#endif

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
#pragma warning(disable: 6387, justification: "I promise the NULL check is there and also I'd just like to say that MSVC sucks")
    LINKED_LIST_CLEAR(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers);
#pragma warning(default: 6387)
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