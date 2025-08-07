#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "us4oem.h"
#include "queue.h"
#include "trace.h"

EXTERN_C_START

EVT_WDF_DEVICE_FILE_CREATE us4oemEvtDeviceFileCreate;
EVT_WDF_FILE_CLOSE us4oemEvtFileClose;

EXTERN_C_END
