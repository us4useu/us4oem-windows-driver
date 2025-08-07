#pragma once

#include "us4oemapi.h"

EXTERN_C_START

// Used for identification of BAR regions
#define PCIDMA_REGION_LENGTH 0x200
#define US4OEM_REGION_LENGTH 0x4000000

typedef struct _BAR_INFO
{
    PHYSICAL_ADDRESS BaseAddr;
    ULONG Length;

	PVOID MappedAddress; // This is the virtual address after mapping the BAR
} BAR_INFO, *PBAR_INFO;

typedef struct _US4OEM_CONTEXT
{
    BAR_INFO BarPciDma;
    BAR_INFO BarUs4Oem;

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
