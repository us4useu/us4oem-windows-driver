#include "main.h"

// Based on https://github.com/microsoft/Windows-driver-samples/tree/main/general/PLX9x5x/test
// Note that this is very much not a well-made example, it is just a quick way to test the driver.

HDEVINFO hDevInfo;
std::map<int, PSP_DEVICE_INTERFACE_DETAIL_DATA> deviceInterfaceDetailMap;
std::map<int, HANDLE> deviceHandleMap;

int count = -1;

VOID
GetDeviceCount() {
    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;

    //
    //  Retreive the device information for all us4oem devices.
    //
    hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_us4oem,
        NULL,
        NULL,
        DIGCF_DEVICEINTERFACE |
        DIGCF_PRESENT);

    //
    //  Initialize the SP_DEVICE_INTERFACE_DATA Structure.
    //
    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    //
    //  Determine how many devices are present.
    //
    count = 0;
    while (SetupDiEnumDeviceInterfaces(hDevInfo,
        NULL,
        &GUID_DEVINTERFACE_us4oem,
        count++,  //Cycle through the available devices.
        &DeviceInterfaceData)
        );

    //
    // Since the last call fails when all devices have been enumerated,
    // decrement the count to get the true device count.
    //
    count--;

    printf("Found %d us4oem devices.\n", count);
}

BOOL
GetDevicePath(int index)
{
    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    SP_DEVINFO_DATA DeviceInfoData;

    ULONG size;
    BOOL status = TRUE;
    TCHAR* DeviceName = NULL;
    TCHAR* DeviceLocation = NULL;

    if (count == -1) {
        GetDeviceCount();
	}

    if (index > count) {
        printf("Requested index %d is out of range. Valid range is 0 to %d.\n", index, count);
		return FALSE;
    }

    //
    //  If the count is zero then there are no devices present.
    //
    if (count == 0) {
        printf("No us4oem devices are present and enabled in the system.\n");
        return FALSE;
    }

    //
    //  Initialize the appropriate data structures in preparation for
    //  the SetupDi calls.
    //
    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    SetupDiEnumDeviceInterfaces(hDevInfo,
        NULL,
        (LPGUID)&GUID_DEVINTERFACE_us4oem,
        index,
        &DeviceInterfaceData);

    //
    // Determine the size required for the DeviceInterfaceData
    //
    SetupDiGetDeviceInterfaceDetail(hDevInfo,
        &DeviceInterfaceData,
        NULL,
        0,
        &size,
        NULL);

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        printf("SetupDiGetDeviceInterfaceDetail failed, Error: %u", GetLastError());
        return FALSE;
    }

    deviceInterfaceDetailMap[index] = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(size);

    if (!deviceInterfaceDetailMap[index]) {
        printf("Insufficient memory.\n");
        return FALSE;
    }

    //
    // Initialize structure and retrieve data.
    //
    deviceInterfaceDetailMap[index]->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    status = SetupDiGetDeviceInterfaceDetail(hDevInfo,
        &DeviceInterfaceData,
        deviceInterfaceDetailMap[index],
        size,
        NULL,
        &DeviceInfoData);

    if (!status) {
        printf("SetupDiGetDeviceInterfaceDetail failed, Error: %u", GetLastError());
        return status;
    }

    //
    //  Get the Device Name
    //  Calls to SetupDiGetDeviceRegistryProperty require two consecutive
    //  calls, first to get required buffer size and second to get
    //  the data.
    //
    SetupDiGetDeviceRegistryProperty(hDevInfo,
        &DeviceInfoData,
        SPDRP_DEVICEDESC,
        NULL,
        (PBYTE)DeviceName,
        0,
        &size);

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        printf("SetupDiGetDeviceRegistryProperty failed, Error: %u", GetLastError());
        return FALSE;
    }

    DeviceName = (TCHAR*)malloc(size);
    if (!DeviceName) {
        printf("Insufficient memory.\n");
        return FALSE;
    }

    status = SetupDiGetDeviceRegistryProperty(hDevInfo,
        &DeviceInfoData,
        SPDRP_DEVICEDESC,
        NULL,
        (PBYTE)DeviceName,
        size,
        NULL);
    if (!status) {
        printf("SetupDiGetDeviceRegistryProperty failed, Error: %u",
            GetLastError());
        free(DeviceName);
        return status;
    }

    //
    //  Now retrieve the Device Location.
    //
    SetupDiGetDeviceRegistryProperty(hDevInfo,
        &DeviceInfoData,
        SPDRP_LOCATION_INFORMATION,
        NULL,
        (PBYTE)DeviceLocation,
        0,
        &size);

    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        DeviceLocation = (TCHAR*)malloc(size);

        if (DeviceLocation != NULL) {

            status = SetupDiGetDeviceRegistryProperty(hDevInfo,
                &DeviceInfoData,
                SPDRP_LOCATION_INFORMATION,
                NULL,
                (PBYTE)DeviceLocation,
                size,
                NULL);
            if (!status) {
                free(DeviceLocation);
                DeviceLocation = NULL;
            }
        }

    }
    else {
        DeviceLocation = NULL;
    }

    printf("%ls\n", DeviceName);

    if (DeviceLocation) {
        printf("@ %ls\n", DeviceLocation);
    }

    free(DeviceName);
    DeviceName = NULL;

    if (DeviceLocation) {
        free(DeviceLocation);
        DeviceLocation = NULL;
    }

    return status;
}

BOOL
GetDeviceHandle(int index)
{
    BOOL status = TRUE;

    if (deviceInterfaceDetailMap[index] == NULL) {
        status = GetDevicePath(index);
    }
    if (deviceInterfaceDetailMap[index] == NULL) {
        status = FALSE;
    }

    if (status) {

		printf("Device Path: %ls\n", deviceInterfaceDetailMap[index]->DevicePath);

        //
        //  Get handle to device.
        //
        deviceHandleMap[index] = CreateFile(deviceInterfaceDetailMap[index]->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            NULL,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (deviceHandleMap[index] == INVALID_HANDLE_VALUE) {
            status = FALSE;
            printf("CreateFile failed.  Error:%u", GetLastError());
        }

    }

    return status;
}

BOOL
Us4OemGetDriverInfo(int index)
{
    BOOL status = TRUE;
    char outBuffer[32];
    DWORD bytesReceived = 0;

	memset(outBuffer, 0, 32);

    if (deviceHandleMap.count(index) == 0 || deviceHandleMap[index] == INVALID_HANDLE_VALUE) {
        status = GetDeviceHandle(index);
        if (status == FALSE) {
            return status;
        }
    }

    if (deviceHandleMap[index] == 0) {
        printf("Invalid device handle.\n");
        return FALSE;
	}

    status = DeviceIoControl(deviceHandleMap[index],
        US4OEM_WIN32_IOCTL_GET_DRIVER_INFO,
        NULL,
        0,
		&outBuffer,
        32,
        &bytesReceived,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[index]);
        return status;
    }

    std::cout << "Bytes received: " << bytesReceived << std::endl;
	std::cout << "Data received:" << std::endl;
    std::cout << "hex: ";
	
    for (DWORD i = 0; i < bytesReceived; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)(outBuffer)[i] << " ";
	}
	std::cout << std::endl;

	std::cout << "ascii: ";
    for (DWORD i = 0; i < bytesReceived - 1; i++) {
        if (isprint(outBuffer[i])) {
            std::cout << outBuffer[i];
        } else {
            std::cout << '#';
        }
	}
	std::cout << std::endl;

    if (deviceHandleMap.count(index) != 0 || deviceHandleMap[index] != INVALID_HANDLE_VALUE && deviceHandleMap[index] != 0) {
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
    }

    return status;
}

int main() {
    GetDeviceCount();

	std::cout << "Found " << count << " us4oem devices." << std::endl << std::endl;
    
    if (count == 0) return 1;

	std::cout << std::endl << "====== IOCTL Test ======" << std::endl;
    for (int i = 0; i < count; i++) {
		std::cout << "=== Device " << i << " ===" << std::endl;
        Us4OemGetDriverInfo(i);
	}

    std::cout << std::endl << "====== Exclusive Access Test ======" << std::endl;
    std::cout << "Trying without closing handles (this should fail on the second attempt)" << std::endl;
    std::vector<HANDLE> handles;
    for (int i = 0; i < 2; i++) {
        std::cout << "Opening device 0 for acccess, i=" << i << "... ";
        handles.push_back(CreateFile(deviceInterfaceDetailMap[0]->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            // In a real client for this driver we should explicitly not share the file acccess,
            // however we shouldn't rely on the user to implement safeguards that should already be there,
            // so this tests the worst-case scenario.
            FILE_SHARE_READ | FILE_SHARE_WRITE, 
            NULL,
            OPEN_EXISTING,
            0,
            NULL));

        if (handles[i] == INVALID_HANDLE_VALUE) {
            std::cout << "fail" << std::endl;
        }
        else {
            std::cout << "success" << std::endl;
        }
    }
    for (int i = 0; i < 2; i++) {
        CloseHandle(handles[i]);
    }

    std::cout << "Trying with closing handles (this should NOT fail on the second attempt)" << std::endl;
    for (int i = 0; i < 2; i++) {
        std::cout << "Opening device 0 for acccess, i=" << i << "... ";
        HANDLE h = CreateFile(deviceInterfaceDetailMap[0]->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (h == INVALID_HANDLE_VALUE) {
            std::cout << "fail" << std::endl;
        }
        else {
            std::cout << "success" << std::endl;
            CloseHandle(h);
        }
    }
    for (int i = 0; i < 2; i++) {
        CloseHandle(handles[i]);
    }

	return 0;
}