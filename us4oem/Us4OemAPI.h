// This file is shared between user mode and kernel mode.
// No kernel mode specific code should be added here.
#pragma once

#ifndef DONT_INCLUDE_INITGUID
// This header is needed for GUIDs to work properly in win32 user mode applications,
// however it's possible that an app would want to avoid using the regular Windows SDK,
// so leave an option to not include it. Note that in that case the app must provide
// a macro for DEFINE_GUID.
#include <initguid.h> 
#endif

//
// Define an Interface Guid so that apps can find the device and talk to it.
//
DEFINE_GUID (GUID_DEVINTERFACE_us4oem,
    0x5d6d47d5,0x5cfa,0x48d0,0x9e,0x12,0xa5,0x10,0xed,0xe8,0x66,0xbd);

#define US4OEM_WIN32_IOCTL_BASE 0xA00 // Arbitrary base value for IOCTLs

// Read the driver information. Returns a null-terminated string.
#define US4OEM_WIN32_IOCTL_GET_DRIVER_INFO \
    CTL_CODE(FILE_DEVICE_UNKNOWN, US4OEM_WIN32_IOCTL_BASE + 0, METHOD_BUFFERED, FILE_ANY_ACCESS)