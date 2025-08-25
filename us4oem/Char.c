#include "char.h"
#include "char.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemEvtDeviceFileCreate)
#pragma alloc_text (PAGE, us4oemEvtFileClose)
#endif

VOID
us4oemEvtDeviceFileCreate(
	_In_ WDFDEVICE Device,
	_In_ WDFREQUEST Request,
	_In_ WDFFILEOBJECT FileObject
	)
{
	UNREFERENCED_PARAMETER(FileObject);

	PAGED_CODE();

	PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);
	deviceContext->Stats.file_open_count++;

	WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID
us4oemEvtFileClose(
	_In_ WDFFILEOBJECT FileObject
	)
{
	PAGED_CODE();

	WDFDEVICE device = WdfFileObjectGetDevice(FileObject);
	PUS4OEM_CONTEXT deviceContext = us4oemGetContext(device);

	if (deviceContext->StickyMode) {
		// Sticky mode enabled - clean buffers as soon as the file is closed
		LINKED_LIST_FOR_EACH(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers, commonBuffer) {
			if (commonBuffer->Item != NULL) {
				WdfObjectDelete(*commonBuffer->Item);
				deviceContext->Stats.dma_contig_free_count++;
			}
		}
		LINKED_LIST_FOR_EACH(MEMORY_ALLOCATION, deviceContext->DmaScatterGatherMemory, commonBuffer) {
			if (commonBuffer->Item != NULL) {
				WdfObjectDelete(commonBuffer->Item->transaction);
				MmUnlockPages(commonBuffer->Item->mdl);
				WdfObjectDelete(commonBuffer->Item->memory);
				IoFreeMdl(commonBuffer->Item->mdl);
				deviceContext->Stats.dma_sg_free_count++;
			}
		}

		deviceContext->Stats.dma_sg_alloc_count = 0;
		deviceContext->Stats.dma_contig_alloc_count = 0;

		LINKED_LIST_CLEAR(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers);
		LINKED_LIST_CLEAR(MEMORY_ALLOCATION, deviceContext->DmaScatterGatherMemory);
	}

	TraceEvents(TRACE_LEVEL_INFORMATION,
		TRACE_QUEUE,
		"File closed, device unlocked.");
}