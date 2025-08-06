#include "main.h"

// Based on https://github.com/microsoft/Windows-driver-samples/tree/main/general/PLX9x5x/test
// Note that this is very much not a well-made example, it is just a quick way to test the driver.

HDEVINFO hDevInfo;
PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetail;
HANDLE hDevice = INVALID_HANDLE_VALUE;

BOOL
GetDevicePath()
{
    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    SP_DEVINFO_DATA DeviceInfoData;

    ULONG size;
    int count, i, index;
    BOOL status = TRUE;
    TCHAR* DeviceName = NULL;
    TCHAR* DeviceLocation = NULL;

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

    //
    //  Loop through the device list to allow user to choose
    //  a device.  If there is only one device, select it
    //  by default.
    //
    i = 0;
    while (SetupDiEnumDeviceInterfaces(hDevInfo,
        NULL,
        (LPGUID)&GUID_DEVINTERFACE_us4oem,
        i,
        &DeviceInterfaceData)) {

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

        pDeviceInterfaceDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(size);

        if (!pDeviceInterfaceDetail) {
            printf("Insufficient memory.\n");
            return FALSE;
        }

        //
        // Initialize structure and retrieve data.
        //
        pDeviceInterfaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        status = SetupDiGetDeviceInterfaceDetail(hDevInfo,
            &DeviceInterfaceData,
            pDeviceInterfaceDetail,
            size,
            NULL,
            &DeviceInfoData);

        free(pDeviceInterfaceDetail);

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

        //
        // If there is more than one device print description.
        //
        if (count > 1) {
            printf("%d- ", i);
        }

        printf("%ls\n", DeviceName);

        if (DeviceLocation) {
            printf("        %ls\n", DeviceLocation);
        }

        free(DeviceName);
        DeviceName = NULL;

        if (DeviceLocation) {
            free(DeviceLocation);
            DeviceLocation = NULL;
        }

        i++; // Cycle through the available devices.
    }

    //
    //  Select device.
    //
    index = 0;
    if (count > 1) {
        printf("\nSelect Device: ");

        if (scanf_s("%d", &index) == 0) {
            return ERROR_INVALID_DATA;
        }
    }

    //
    //  Get information for specific device.
    //
    status = SetupDiEnumDeviceInterfaces(hDevInfo,
        NULL,
        (LPGUID)&GUID_DEVINTERFACE_us4oem,
        index,
        &DeviceInterfaceData);

    if (!status) {
        printf("SetupDiEnumDeviceInterfaces failed, Error: %u", GetLastError());
        return status;
    }

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

    pDeviceInterfaceDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(size);

    if (!pDeviceInterfaceDetail) {
        printf("Insufficient memory.\n");
        return FALSE;
    }

    //
    // Initialize structure and retrieve data.
    //
    pDeviceInterfaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    status = SetupDiGetDeviceInterfaceDetail(hDevInfo,
        &DeviceInterfaceData,
        pDeviceInterfaceDetail,
        size,
        NULL,
        &DeviceInfoData);
    if (!status) {
        printf("SetupDiGetDeviceInterfaceDetail failed, Error: %u", GetLastError());
        return status;
    }

    return status;
}

BOOL
GetDeviceHandle()
{
    BOOL status = TRUE;

    if (pDeviceInterfaceDetail == NULL) {
        status = GetDevicePath();
    }
    if (pDeviceInterfaceDetail == NULL) {
        status = FALSE;
    }

    if (status) {

		printf("Device Path: %ls\n", pDeviceInterfaceDetail->DevicePath);

        //
        //  Get handle to device.
        //
        hDevice = CreateFile(pDeviceInterfaceDetail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (hDevice == INVALID_HANDLE_VALUE) {
            status = FALSE;
            printf("CreateFile failed.  Error:%u", GetLastError());
        }
    }

    return status;
}

BOOL
Us4OemGetDriverInfo()
{
    BOOL status = TRUE;
    char outBuffer[32];
    DWORD bytesReceived = 0;

	memset(outBuffer, 0, 32);

    if (hDevice == INVALID_HANDLE_VALUE) {
        status = GetDeviceHandle();
        if (status == FALSE) {
            return status;
        }
    }

    if (hDevice == 0) {
        printf("Invalid device handle.\n");
        return FALSE;
	}

    status = DeviceIoControl(hDevice,
        US4OEM_WIN32_IOCTL_GET_DRIVER_INFO,
        NULL,
        0,
		&outBuffer,
        32,
        &bytesReceived,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(hDevice);
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

    if (hDevice != INVALID_HANDLE_VALUE && hDevice != 0) {
        CloseHandle(hDevice);
        hDevice = INVALID_HANDLE_VALUE;
    }

    return status;
}

int main() {
	std::cout << "Hello, World!" << std::endl;

    Us4OemGetDriverInfo();

	return 0;
}