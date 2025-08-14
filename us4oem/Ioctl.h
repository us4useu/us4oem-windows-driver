#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "us4oem.h"
#include "queue.h"
#include "trace.h"

EXTERN_C_START

// IOCTL handler function prototype
typedef VOID(IOCTL_HANDLER_FUNC)(WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer);

// IOCTL handler function prototype with buffer sizes for dynamic response sizes; preferred over IOCTL_HANDLER_FUNC
typedef VOID(IOCTL_HANDLER_FUNC_WITH_BUFFER_SIZES)(WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer, size_t OutputBufferLength, size_t InputBufferLength);

// Struct for IOCTL handling
typedef struct _IOCTL_HANDLER {
	ULONG IoControlCode;
	size_t InputBufferNeeded;
	size_t OutputBufferNeeded;
	IOCTL_HANDLER_FUNC* HandlerFunc; // Function to handle the IOCTL
	IOCTL_HANDLER_FUNC_WITH_BUFFER_SIZES* HandlerFuncWithBufferSizes; // Function to handle the IOCTL with buffer sizes
} IOCTL_HANDLER, *PIOCTL_HANDLER;

// Defined in Ioctl.c
IOCTL_HANDLER_FUNC us4oemIoctlGetDriverInfo;
IOCTL_HANDLER_FUNC us4oemIoctlReadStats;

// Defined in Mem.c
IOCTL_HANDLER_FUNC us4oemIoctlMmap;

// Defined in Sync.c
IOCTL_HANDLER_FUNC us4oemIoctlPoll;
IOCTL_HANDLER_FUNC us4oemIoctlPollNonBlocking;
IOCTL_HANDLER_FUNC us4oemIoctlClearPending;

// Defined in Dma.c
IOCTL_HANDLER_FUNC us4oemIoctlAllocateDmaContiguousBuffer;
IOCTL_HANDLER_FUNC us4oemIoctlDeallocateContigousDmaBuffer;
IOCTL_HANDLER_FUNC us4oemIoctlDeallocateAllDmaBuffers;
IOCTL_HANDLER_FUNC_WITH_BUFFER_SIZES us4oemIoctlAllocateDmaScatterGatherBuffer;
IOCTL_HANDLER_FUNC us4oemIoctlDeallocateScatterGatherDmaBuffer;

PIOCTL_HANDLER us4oemGetIoctlHandler();
ULONG us4oemGetIoctlHandlerCount();

EXTERN_C_END