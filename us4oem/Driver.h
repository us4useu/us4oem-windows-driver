#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "us4oem.h"
#include "queue.h"
#include "trace.h"

EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD us4oemEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP us4oemEvtDriverContextCleanup;

EXTERN_C_END
