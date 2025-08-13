#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "us4oem.h"
#include "queue.h"
#include "trace.h"

EXTERN_C_START

// ioctl handler function prototype
typedef VOID(IOCTL_HANDLER_FUNC)(WDFDEVICE Device, WDFREQUEST Request, PVOID OutputBuffer, PVOID InputBuffer);

// Struct for IOCTL handling
typedef struct _IOCTL_HANDLER {
	ULONG IoControlCode;
	size_t InputBufferNeeded;
	size_t OutputBufferNeeded;
	IOCTL_HANDLER_FUNC* HandlerFunc; // Function to handle the IOCTL
} IOCTL_HANDLER, *PIOCTL_HANDLER;

IOCTL_HANDLER_FUNC us4oemIoctlGetDriverInfo;
IOCTL_HANDLER_FUNC us4oemIoctlMmap;
IOCTL_HANDLER_FUNC us4oemIoctlReadStats;
IOCTL_HANDLER_FUNC us4oemIoctlPoll;
IOCTL_HANDLER_FUNC us4oemIoctlPollNonBlocking;
IOCTL_HANDLER_FUNC us4oemIoctlClearPending;
IOCTL_HANDLER_FUNC us4oemIoctlAllocateDmaContiguousBuffer;
IOCTL_HANDLER_FUNC us4oemIoctlDeallocateContigousDmaBuffer;
IOCTL_HANDLER_FUNC us4oemIoctlDeallocateAllDmaBuffers;

PIOCTL_HANDLER us4oemGetIoctlHandler();
ULONG us4oemGetIoctlHandlerCount();

EXTERN_C_END