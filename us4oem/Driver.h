#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include <stdbool.h>

#include "us4oem.h"
#include "queue.h"
#include "trace.h"
#include "char.h"
#include "interrupt.h"

EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD us4oemEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP us4oemEvtDriverContextCleanup;
EVT_WDF_DEVICE_PREPARE_HARDWARE us4oemEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE us4oemEvtDeviceReleaseHardware;

EXTERN_C_END
