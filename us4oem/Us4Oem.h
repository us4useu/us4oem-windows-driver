#pragma once

#include "us4oemapi.h"

EXTERN_C_START

typedef struct _US4OEM_CONTEXT
{
    ULONG PrivateDeviceData;  // placeholder

} US4OEM_CONTEXT, *PUS4OEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(US4OEM_CONTEXT, us4oemGetContext)

//
// Function to initialize the device and its callbacks
//
NTSTATUS
us4oemCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

EXTERN_C_END
