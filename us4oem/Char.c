#include "char.h"
#include "char.tmh"

VOID
us4oemEvtDeviceFileCreate(
	_In_ WDFDEVICE Device,
	_In_ WDFREQUEST Request,
	_In_ WDFFILEOBJECT FileObject
	)
{
	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(Device);

	WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID
us4oemEvtFileClose(
	_In_ WDFFILEOBJECT FileObject
	)
{
	UNREFERENCED_PARAMETER(FileObject);

	TraceEvents(TRACE_LEVEL_INFORMATION,
		TRACE_QUEUE,
		"File closed, device unlocked.");
}