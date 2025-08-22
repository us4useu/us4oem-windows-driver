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
    LINKED_LIST_FOR_EACH(MEMORY_ALLOCATION, deviceContext->DmaScatterGatherMemory, commonBuffer) {
        if (commonBuffer->Item != NULL) {
            MmUnlockPages(commonBuffer->Item->mdl);
            WdfObjectDelete(commonBuffer->Item->memory);
            IoFreeMdl(commonBuffer->Item->mdl);
            deviceContext->Stats.dma_sg_free_count++;
        }
	}

    deviceContext->Stats.dma_sg_alloc_count = 0;
	deviceContext->Stats.dma_contig_alloc_count = 0;

    // Clear the linked list
#pragma warning(disable: 6387, justification: "I promise the NULL check is there and also I'd just like to say that MSVC sucks")
    LINKED_LIST_CLEAR(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers);
	LINKED_LIST_CLEAR(MEMORY_ALLOCATION, deviceContext->DmaScatterGatherMemory);
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

    void* va = (*(void**)(InputBuffer));
    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

    // Iterate over the contiguous buffers until we find the one with the matching PA
    LINKED_LIST_FOR_EACH(MEMORY_ALLOCATION, deviceContext->DmaScatterGatherMemory, commonBuffer) {
        if (commonBuffer->Item != NULL &&
            WdfMemoryGetBuffer(commonBuffer->Item->memory, NULL) == va) {

            // Found the buffer, delete it
            MmUnlockPages(commonBuffer->Item->mdl);
            WdfObjectDelete(commonBuffer->Item->memory);
            IoFreeMdl(commonBuffer->Item->mdl);
            deviceContext->Stats.dma_sg_free_count++;
            deviceContext->Stats.dma_sg_alloc_count--;
            LINKED_LIST_REMOVE(MEMORY_ALLOCATION, deviceContext->DmaScatterGatherMemory, commonBuffer);
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

typedef struct _us4oem_dma_program_context {
    WDFREQUEST Request; // The request to complete
    PVOID OutputBuffer; // The output buffer for the response
    PVOID VA; // Virtual address of the DMA buffer
} us4oem_dma_program_context;

BOOLEAN us4oemProgramDma(
    WDFDMATRANSACTION Transaction,
    WDFDEVICE Device,
    WDFCONTEXT Context,
    WDF_DMA_DIRECTION Direction,
    PSCATTER_GATHER_LIST SgList
) {
    UNREFERENCED_PARAMETER(Direction);
    UNREFERENCED_PARAMETER(Transaction);
    UNREFERENCED_PARAMETER(Device);

    us4oem_dma_program_context* context = (us4oem_dma_program_context*)Context;

    WDFREQUEST request = context->Request;
    us4oem_dma_scatter_gather_buffer_response* outputBuffer = (us4oem_dma_scatter_gather_buffer_response*)context->OutputBuffer;

    // Get the input and output buffers from the request
    if (request == NULL) {
        WdfRequestComplete(request, STATUS_INVALID_PARAMETER);
        return FALSE;
    }

    // Check if the SgList is valid
    if (SgList == NULL || SgList->NumberOfElements == 0) {
        WdfRequestComplete(request, STATUS_INVALID_PARAMETER);
        return FALSE;
    }

    // Process the scatter-gather list
    for (ULONG i = 0; i < SgList->NumberOfElements; i++) {
        PSCATTER_GATHER_ELEMENT element = &SgList->Elements[i];
        us4oem_dma_scatter_gather_buffer_chunk* chunk = &((outputBuffer)->chunks[i]);
        chunk->pa = element->Address.QuadPart;
        chunk->length = element->Length;
    }

    // Set the response fields
    outputBuffer->chunk_count = SgList->NumberOfElements;
    outputBuffer->length_used = US4OEM_DMA_SG_RESPONSE_NEEDED_SIZE(SgList->NumberOfElements);
	outputBuffer->va = context->VA; // The VA of the buffer is the one we allocated earlier

    // Complete the request successfully

    WdfRequestCompleteWithInformation(
        request,
        STATUS_SUCCESS,
        outputBuffer->length_used
    );
    return TRUE;
}

VOID us4oemIoctlAllocateDmaScatterGatherBuffer(
    WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer, size_t OutputBufferLength, size_t InputBufferLength
) {
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    PAGED_CODE();

    us4oem_dma_allocation_argument* arg = (us4oem_dma_allocation_argument*)InputBuffer;
    if (arg == NULL || arg->length == 0) {
        WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
        return;
    }

    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

	/*WdfDmaEnablerSetMaximumScatterGatherElements(
        deviceContext->DmaEnabler,
        arg->max_chunks
	);*/

    MEMORY_ALLOCATION* allocation = MmAllocateNonCachedMemory(sizeof(MEMORY_ALLOCATION));
    PVOID pBuffer;

    NTSTATUS status = WdfMemoryCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        PagedPool,
        'r4su',
        arg->length,
        &allocation->memory,
		&pBuffer
	);

    if (!NT_SUCCESS(status)) {
		MmFreeNonCachedMemory(allocation, sizeof(MEMORY_ALLOCATION));
        WdfRequestComplete(Request, status);
        return;
	}

	// Lock pages, allocate an MDL, and map the buffer
	allocation->mdl = IoAllocateMdl(pBuffer, (ULONG)arg->length, FALSE, FALSE, NULL);

    if (allocation->mdl == NULL) {
		WdfObjectDelete(allocation->memory);
        MmFreeNonCachedMemory(allocation, sizeof(MEMORY_ALLOCATION));
        WdfRequestComplete(Request, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    __try {
        MmProbeAndLockPages(allocation->mdl, KernelMode, IoWriteAccess);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_IOCTL,
            "MmProbeAndLockPages failed");
        IoFreeMdl(allocation->mdl);
        WdfObjectDelete(allocation->memory);
        MmFreeNonCachedMemory(allocation, sizeof(MEMORY_ALLOCATION));
        WdfRequestComplete(Request, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }
    
	// Create a DMA transaction
	WDFDMATRANSACTION transaction;

    status = WdfDmaTransactionCreate(deviceContext->DmaEnabler, WDF_NO_OBJECT_ATTRIBUTES, &transaction);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_IOCTL,
            "WdfDmaTransactionCreate failed");
        WdfRequestComplete(Request, status);
		MmUnlockPages(allocation->mdl);
        IoFreeMdl(allocation->mdl);
        WdfObjectDelete(allocation->memory);
        MmFreeNonCachedMemory(allocation, sizeof(MEMORY_ALLOCATION));
        return;
	}

	us4oem_dma_program_context* context = MmAllocateNonCachedMemory(sizeof(us4oem_dma_program_context));
	context->Request = Request;
	context->OutputBuffer = OutputBuffer;
	context->VA = WdfMemoryGetBuffer(allocation->memory, NULL);

    status = WdfDmaTransactionInitialize(
        transaction,
        &us4oemProgramDma,
        WdfDmaDirectionReadFromDevice,
        allocation->mdl,
        MmGetMdlVirtualAddress(allocation->mdl),
        MmGetMdlByteCount(allocation->mdl)
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_IOCTL,
            "WdfDmaTransactionInitialize failed");
        WdfObjectDelete(transaction);
        MmUnlockPages(allocation->mdl);
        IoFreeMdl(allocation->mdl);
        WdfObjectDelete(allocation->memory);
        MmFreeNonCachedMemory(allocation, sizeof(MEMORY_ALLOCATION));
        WdfRequestComplete(Request, status);
        return;
    }

    WdfDmaTransactionSetImmediateExecution(transaction, TRUE);

    status = WdfDmaTransactionExecute(transaction, context);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_IOCTL,
            "WdfDmaTransactionExecute failed");
        WdfObjectDelete(transaction);
        MmUnlockPages(allocation->mdl);
        IoFreeMdl(allocation->mdl);
        WdfObjectDelete(allocation->memory);
        MmFreeNonCachedMemory(allocation, sizeof(MEMORY_ALLOCATION));
        WdfRequestComplete(Request, status);
        return;
    }

	// Push the memory into the linked list of scatter-gather buffers
	LINKED_LIST_PUSH(MEMORY_ALLOCATION, deviceContext->DmaScatterGatherMemory, allocation);

	// Increment the allocation count
	deviceContext->Stats.dma_sg_alloc_count++;
    return;
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