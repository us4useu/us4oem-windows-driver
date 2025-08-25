#pragma once

#include "us4oemapi.h"

#include "linkedlist.h"

EXTERN_C_START

// Used for identification of BAR regions
#define PCIDMA_REGION_LENGTH 0x200
#define US4OEM_REGION_LENGTH 0x4000000

// Driver version
#ifndef US4OEM_DRIVER_INFO_STRING
#define US4OEM_DRIVER_INFO_STRING "us4oem win32 driver"
#endif

typedef struct _BAR_INFO
{
    PHYSICAL_ADDRESS BaseAddr;
    ULONG Length;

	PVOID MappedAddress; // This is the virtual address after mapping the BAR
} BAR_INFO, *PBAR_INFO;

// Stores information 
typedef struct _MEMORY_ALLOCATION 
{
	WDFMEMORY memory;
	PMDL mdl;
	WDFDMATRANSACTION transaction;
} MEMORY_ALLOCATION, *PMEMORY_ALLOCATION;

USE_IN_LINKED_LISTS(WDFCOMMONBUFFER);
USE_IN_LINKED_LISTS(MEMORY_ALLOCATION);

typedef struct _US4OEM_CONTEXT
{
    BAR_INFO BarPciDma;
    BAR_INFO BarUs4Oem;

	WDFINTERRUPT Interrupt; // Interrupt object for the device

    us4oem_stats Stats; // Statistics for the device

	WDFREQUEST PendingRequest; // Request that is waiting for an IRQ

	WDFDMAENABLER DmaEnabler; // DMA enabler for the device

	BOOLEAN StickyMode; // If TRUE, buffers will be released as soon as the device handle is closed

	LINKED_LIST_POINTERS(WDFCOMMONBUFFER, DmaContiguousBuffers) // Linked list of contiguous DMA buffers

	LINKED_LIST_POINTERS(MEMORY_ALLOCATION, DmaScatterGatherMemory) // Linked list of scatter-gather DMA buffers

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
