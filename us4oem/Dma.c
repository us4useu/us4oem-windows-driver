#include "ioctl.h"
#include "dma.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemIoctlAllocateDmaContiguousBuffer)
#pragma alloc_text (PAGE, us4oemIoctlDeallocateContigousDmaBuffer)
#pragma alloc_text (PAGE, us4oemIoctlDeallocateAllDmaBuffers)
#pragma alloc_text (PAGE, us4oemIoctlAllocateDmaScatterGatherBuffer)
#pragma alloc_text (PAGE, us4oemIoctlDeallocateScatterGatherDmaBuffer)
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
        }
    }

	// Iterate over the linked list of scatter-gather buffers and delete each one
    LINKED_LIST_FOR_EACH(WDFCOMMONBUFFER, deviceContext->DmaScatterGatherBuffers, commonBuffer) {
        if (commonBuffer->Item != NULL) {
            WdfObjectDelete(*commonBuffer->Item);
            deviceContext->Stats.dma_sg_free_count++;
        }
	}

    deviceContext->Stats.dma_sg_alloc_count = 0;
	deviceContext->Stats.dma_contig_alloc_count = 0;

    // Clear the linked list
#pragma warning(disable: 6387, justification: "I promise the NULL check is there and also I'd just like to say that MSVC sucks")
    LINKED_LIST_CLEAR(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers);
	LINKED_LIST_CLEAR(WDFCOMMONBUFFER, deviceContext->DmaScatterGatherBuffers);
#pragma warning(default: 6387)
    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_IOCTL,
        "Deallocated all DMA buffers");
    WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID us4oemIoctlDeallocateScatterGatherDmaBuffer(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer
) {
    PAGED_CODE();

    UNREFERENCED_PARAMETER(OutputBuffer);

    void* va = InputBuffer;
    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

    // Iterate over the contiguous buffers until we find the one with the matching PA
    LINKED_LIST_FOR_EACH(WDFCOMMONBUFFER, deviceContext->DmaScatterGatherBuffers, commonBuffer) {
        if (commonBuffer->Item != NULL &&
            WdfCommonBufferGetAlignedVirtualAddress(*(commonBuffer->Item)) == va) {
            // Found the buffer, delete it
            WdfObjectDelete(*commonBuffer->Item);
            deviceContext->Stats.dma_sg_free_count++;
            deviceContext->Stats.dma_sg_alloc_count--;
            LINKED_LIST_REMOVE(WDFCOMMONBUFFER, deviceContext->DmaScatterGatherBuffers, commonBuffer);
            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_IOCTL,
                "Deallocated SG DMA buffer with VA: 0x%p",
                va);
            WdfRequestComplete(Request, STATUS_SUCCESS);
            return;
        }
    }

    // Not found, complete with an error
    TraceEvents(TRACE_LEVEL_ERROR,
        TRACE_IOCTL,
        "Failed to find SG DMA buffer with VA: 0x%p",
        va);
    WdfRequestComplete(Request, STATUS_NOT_FOUND);
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

VOID us4oemIoctlAllocateDmaScatterGatherBuffer(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer, size_t OutputBufferLength, size_t InputBufferLength
) {
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBuffer);

    PAGED_CODE();

    us4oem_dma_allocation_argument* arg = (us4oem_dma_allocation_argument*)InputBuffer;

    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

	// Calculate the amount of chunks needed based on the input length and the maximum chunk size
	unsigned long chunkCount = US4OEM_DMA_SG_CHUNK_COUNT(arg->length, arg->chunk_size);

	// Check if there's enough space in the output buffer
	if (OutputBufferLength < US4OEM_DMA_SG_RESPONSE_NEEDED_SIZE(chunkCount)) {
        WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
        return;
	}

    us4oem_dma_scatter_gather_buffer_response* response = (us4oem_dma_scatter_gather_buffer_response*)OutputBuffer;

	// Allocate chunks for the scatter-gather buffer
    for (unsigned long i = 0; i < chunkCount; i++) {
        // Allocate memory for the common buffer struct
        WDFCOMMONBUFFER* commonBuffer = MmAllocateNonCachedMemory(sizeof(WDFCOMMONBUFFER));

        unsigned long chunkSize;
        if (arg->length % arg->chunk_size == 0) {
            chunkSize = arg->chunk_size;
        } else {
			chunkSize = i == chunkCount - 1 ? arg->length % arg->chunk_size : arg->chunk_size;
		}

        NTSTATUS status = WdfCommonBufferCreate(deviceContext->DmaEnabler,
            chunkSize,
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

        us4oem_dma_scatter_gather_buffer_chunk* chunk = &(((us4oem_dma_scatter_gather_buffer_response*)OutputBuffer)->chunks[i]);
        chunk->va = WdfCommonBufferGetAlignedVirtualAddress(*commonBuffer);
        chunk->pa = (WdfCommonBufferGetAlignedLogicalAddress(*commonBuffer)).QuadPart;
        chunk->length = chunkSize;

        // Store information about the allocated buffer in the device context
        LINKED_LIST_PUSH(WDFCOMMONBUFFER, deviceContext->DmaScatterGatherBuffers, commonBuffer);
	}

    response->chunk_count = chunkCount;
	response->length_used = US4OEM_DMA_SG_RESPONSE_NEEDED_SIZE(chunkCount);
    deviceContext->Stats.dma_sg_alloc_count += chunkCount;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_IOCTL,
        "Allocated %d chunks for scatter-gather DMA buffer, total size: %d bytes",
        chunkCount,
        US4OEM_DMA_SG_RESPONSE_NEEDED_SIZE(chunkCount));
	// Complete the request with the response

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, US4OEM_DMA_SG_RESPONSE_NEEDED_SIZE(chunkCount));
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