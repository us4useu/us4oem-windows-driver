#pragma once

#include <ntddk.h>
#include <wdf.h>

#include "trace.h"
#include "us4oem.h"

EXTERN_C_START

EVT_WDF_INTERRUPT_ISR Us4OemInterruptIsr;
EVT_WDF_INTERRUPT_DPC Us4OemInterruptDpc;

EXTERN_C_END
