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
	UNREFERENCED_PARAMETER(FileObject);

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION,
		TRACE_QUEUE,
		"File closed, device unlocked.");
}