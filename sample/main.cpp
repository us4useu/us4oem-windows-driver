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

		//printf("Device Path: %ls\n", deviceInterfaceDetailMap[index]->DevicePath);

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
    us4oem_driver_info outBuffer;
    DWORD bytesReceived = 0;

	memset(&outBuffer, 0, sizeof(outBuffer));

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
        sizeof(outBuffer),
        &bytesReceived,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
        return status;
    }

	// Print name from the c-style string
	std::cout << "Driver Name: " << std::string(outBuffer.name) << std::endl;

	// Print version
	std::cout << "Driver Version: " << std::endl;
	std::cout << "  Reply:" << std::endl;
	std::cout << "      Magic: 0x" << std::hex << ((outBuffer.version & 0xFF000000) >> 24) << std::dec << std::endl;
	std::cout << "      Major: 0x" << std::hex << ((outBuffer.version & 0x00FF0000) >> 16) << std::dec << std::endl;
	std::cout << "      Minor: 0x" << std::hex << ((outBuffer.version & 0x0000FF00) >> 8) << std::dec << std::endl;
	std::cout << "      Patch: 0x" << std::hex << (outBuffer.version & 0x000000FF) << std::dec << std::endl;
    std::cout << "  Built against: " << std::endl;
	std::cout << "      Magic: 0x" << std::hex << ((US4OEM_DRIVER_VERSION & 0xFF000000) >> 24) << std::dec << std::endl;
	std::cout << "      Major: 0x" << std::hex << ((US4OEM_DRIVER_VERSION & 0x00FF0000) >> 16) << std::dec << std::endl;
	std::cout << "      Minor: 0x" << std::hex << ((US4OEM_DRIVER_VERSION & 0x0000FF00) >> 8) << std::dec << std::endl;
	std::cout << "      Patch: 0x" << std::hex << (US4OEM_DRIVER_VERSION & 0x000000FF) << std::dec << std::endl;

    if (US4OEM_DRIVER_VERSION == outBuffer.version) {
        std::cout << "(match)" << std::endl;
    }
    else {
        std::cout << "(!!!MISMATCH!!!)" << std::endl;
    }

    if (deviceHandleMap.count(index) != 0 || deviceHandleMap[index] != INVALID_HANDLE_VALUE && deviceHandleMap[index] != 0) {
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
    }

    return status;
}

PVOID 
Us4OemMapBar(int deviceIndex, uint8_t bar, unsigned long size_limit) {
    BOOL status = TRUE;
    us4oem_mmap_response outBuffer;
    us4oem_mmap_argument inBuffer;
    DWORD bytesReceived = 0;

    memset(&inBuffer, 0, sizeof(inBuffer));
    memset(&outBuffer, 0, sizeof(outBuffer));

    inBuffer.area = bar == 0 ? MMAP_AREA_BAR_0 : MMAP_AREA_BAR_4;
    inBuffer.length_limit = size_limit;

    if (deviceHandleMap.count(deviceIndex) == 0 || deviceHandleMap[deviceIndex] == INVALID_HANDLE_VALUE) {
        status = GetDeviceHandle(deviceIndex);
        if (status == FALSE) {
            return NULL;
        }
    }

    if (deviceHandleMap[deviceIndex] == 0) {
        printf("Invalid device handle.\n");
        return NULL;
    }

    status = DeviceIoControl(deviceHandleMap[deviceIndex],
        US4OEM_WIN32_IOCTL_MMAP,
        &inBuffer,
        sizeof(inBuffer),
        &outBuffer,
        sizeof(outBuffer),
        &bytesReceived,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[deviceIndex]);
        deviceHandleMap[deviceIndex] = INVALID_HANDLE_VALUE;
        return NULL;
    }

    std::cout << "Bytes received: " << bytesReceived << std::endl;
    std::cout << "Address received: 0x" << std::hex << outBuffer.address << std::dec << std::endl;
    std::cout << "Length mapped: 0x" << std::hex << outBuffer.length_mapped << std::dec << std::endl;

    // Read offset 0
    std::cout << "@ offset 0: 0x" << std::hex << *(int*)(outBuffer.address) << std::dec << std::endl;

    if (deviceHandleMap.count(deviceIndex) != 0 || deviceHandleMap[deviceIndex] != INVALID_HANDLE_VALUE && deviceHandleMap[deviceIndex] != 0) {
        CloseHandle(deviceHandleMap[deviceIndex]);
        deviceHandleMap[deviceIndex] = INVALID_HANDLE_VALUE;
    }

    return outBuffer.address;
}

PVOID
Us4OemMapDmaBuf(int deviceIndex, void* va, unsigned long length) {
    BOOL status = TRUE;
    us4oem_mmap_response outBuffer;
    us4oem_mmap_argument inBuffer;
    DWORD bytesReceived = 0;

    memset(&inBuffer, 0, sizeof(inBuffer));
    memset(&outBuffer, 0, sizeof(outBuffer));

    inBuffer.area = MMAP_AREA_DMA;
	inBuffer.va = va; // Virtual address for DMA allocations
    inBuffer.length_limit = length;

    if (deviceHandleMap.count(deviceIndex) == 0 || deviceHandleMap[deviceIndex] == INVALID_HANDLE_VALUE) {
        status = GetDeviceHandle(deviceIndex);
        if (status == FALSE) {
            return NULL;
        }
    }

    if (deviceHandleMap[deviceIndex] == 0) {
        printf("Invalid device handle.\n");
        return NULL;
    }

    status = DeviceIoControl(deviceHandleMap[deviceIndex],
        US4OEM_WIN32_IOCTL_MMAP,
        &inBuffer,
        sizeof(inBuffer),
        &outBuffer,
        sizeof(outBuffer),
        &bytesReceived,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[deviceIndex]);
        deviceHandleMap[deviceIndex] = INVALID_HANDLE_VALUE;
        return NULL;
    }

    std::cout << "Address received: 0x" << std::hex << outBuffer.address << std::dec << std::endl;
    std::cout << "Length mapped: 0x" << std::hex << outBuffer.length_mapped << std::dec << std::endl;

    // Read offset 0
    std::cout << "@ offset 0: 0x" << std::hex << *(int*)(outBuffer.address) << std::dec << std::endl;
	*(int*)outBuffer.address = 0x12345678; // Example write to the mapped area
	std::cout << "Wrote 0x12345678 to the mapped area." << std::endl;
	std::cout << "@ offset 0 after write: 0x" << std::hex << *(int*)(outBuffer.address) << std::dec << std::endl;

    if (deviceHandleMap.count(deviceIndex) != 0 || deviceHandleMap[deviceIndex] != INVALID_HANDLE_VALUE && deviceHandleMap[deviceIndex] != 0) {
        CloseHandle(deviceHandleMap[deviceIndex]);
        deviceHandleMap[deviceIndex] = INVALID_HANDLE_VALUE;
    }

    return outBuffer.address;
}

BOOL
Us4OemReadStats(int index)
{
    BOOL status = TRUE;
    us4oem_stats outBuffer;
    DWORD bytesReceived = 0;

    memset(&outBuffer, 0, sizeof(outBuffer));

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
        US4OEM_WIN32_IOCTL_READ_STATS,
        NULL,
        0,
        &outBuffer,
        sizeof(outBuffer),
        &bytesReceived,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
        return status;
    }

    std::cout << "Stats: " << std::endl;
	std::cout << "  IRQ count: " << outBuffer.irq_count << std::endl;
	std::cout << "  IRQ pending count: " << outBuffer.irq_pending_count << std::endl;
	std::cout << "  DMA contiguous alloc count: " << outBuffer.dma_contig_alloc_count << std::endl;
    std::cout << "  DMA contiguous free count: " << outBuffer.dma_contig_free_count << std::endl;
    std::cout << std::endl;

    if (deviceHandleMap.count(index) != 0 || deviceHandleMap[index] != INVALID_HANDLE_VALUE && deviceHandleMap[index] != 0) {
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
    }

    return status;
}

std::pair<PVOID, unsigned long long>
Us4OemAllocDmaContig(int deviceIndex, unsigned long length, bool quiet = false) {
    BOOL status = TRUE;
    us4oem_dma_contiguous_buffer_response outBuffer;
    us4oem_dma_allocation_argument inBuffer;
    DWORD bytesReceived = 0;

    memset(&inBuffer, 0, sizeof(inBuffer));
    memset(&outBuffer, 0, sizeof(outBuffer));

	inBuffer.length = length; // Length of the DMA buffer to allocate

    if (deviceHandleMap.count(deviceIndex) == 0 || deviceHandleMap[deviceIndex] == INVALID_HANDLE_VALUE) {
        status = GetDeviceHandle(deviceIndex);
        if (status == FALSE) {
            return { NULL, NULL };
        }
    }

    if (deviceHandleMap[deviceIndex] == 0) {
        printf("Invalid device handle.\n");
        return { NULL, NULL };
    }

    status = DeviceIoControl(deviceHandleMap[deviceIndex],
        US4OEM_WIN32_IOCTL_ALLOCATE_DMA_CONTIGIOUS_BUFFER,
        &inBuffer,
        sizeof(inBuffer),
        &outBuffer,
        sizeof(outBuffer),
        &bytesReceived,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[deviceIndex]);
        deviceHandleMap[deviceIndex] = INVALID_HANDLE_VALUE;
        return { NULL, NULL };
    }

    if (!quiet) {
	std::cout << "VA: 0x" << std::hex << outBuffer.va << std::dec << std::endl;
	std::cout << "PA: 0x" << std::hex << outBuffer.pa << std::dec << std::endl;
    }

    if (deviceHandleMap.count(deviceIndex) != 0 || deviceHandleMap[deviceIndex] != INVALID_HANDLE_VALUE && deviceHandleMap[deviceIndex] != 0) {
        CloseHandle(deviceHandleMap[deviceIndex]);
        deviceHandleMap[deviceIndex] = INVALID_HANDLE_VALUE;
    }

	return { (PVOID)outBuffer.va, outBuffer.pa };
}

BOOL
Us4OemDeallocDmaContig(int deviceIndex, unsigned long long pa) {
    BOOL status = TRUE;
    unsigned long long inBuffer;
    DWORD bytesReceived = 0;

	memset(&inBuffer, 0, sizeof(inBuffer));

	inBuffer = pa; // Physical address of the allocated buffer

    if (deviceHandleMap.count(deviceIndex) == 0 || deviceHandleMap[deviceIndex] == INVALID_HANDLE_VALUE) {
        status = GetDeviceHandle(deviceIndex);
        if (status == FALSE) {
            return FALSE;
        }
    }

    if (deviceHandleMap[deviceIndex] == 0) {
        printf("Invalid device handle.\n");
        return FALSE;
    }

    status = DeviceIoControl(deviceHandleMap[deviceIndex],
        US4OEM_WIN32_IOCTL_DEALLOCATE_DMA_CONTIGIOUS_BUFFER,
        &inBuffer,
        sizeof(inBuffer),
        NULL,
        0,
        &bytesReceived,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[deviceIndex]);
        deviceHandleMap[deviceIndex] = INVALID_HANDLE_VALUE;
        return FALSE;
    }

    if (deviceHandleMap.count(deviceIndex) != 0 || deviceHandleMap[deviceIndex] != INVALID_HANDLE_VALUE && deviceHandleMap[deviceIndex] != 0) {
        CloseHandle(deviceHandleMap[deviceIndex]);
        deviceHandleMap[deviceIndex] = INVALID_HANDLE_VALUE;
    }

    return TRUE;
}

BOOL
Us4OemPoll(int index)
{
    BOOL status = TRUE;

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

	printf("Polling device %d...\n", index);
    status = DeviceIoControl(deviceHandleMap[index],
        US4OEM_WIN32_IOCTL_POLL,
        NULL,
        0,
        NULL,
        0,
        NULL,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
        return status;
    }
	printf("Device %d polled successfully.\n", index);

    if (deviceHandleMap.count(index) != 0 || deviceHandleMap[index] != INVALID_HANDLE_VALUE && deviceHandleMap[index] != 0) {
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
    }

    return status;
}

BOOL
Us4OemPollNonBlocking(int index)
{
    BOOL status = TRUE;

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

    printf("Polling device %d...\n", index);
    status = DeviceIoControl(deviceHandleMap[index],
        US4OEM_WIN32_IOCTL_POLL_NONBLOCKING,
        NULL,
        0,
        NULL,
        0,
        NULL,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
        return status;
    }
    printf("Device %d polled successfully.\n", index);

    if (deviceHandleMap.count(index) != 0 || deviceHandleMap[index] != INVALID_HANDLE_VALUE && deviceHandleMap[index] != 0) {
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
    }

    return status;
}

BOOL
Us4OemPollClearPending(int index)
{
    BOOL status = TRUE;

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

    printf("Clearing pending IRQs on device %d...\n", index);
    status = DeviceIoControl(deviceHandleMap[index],
        US4OEM_WIN32_IOCTL_CLEAR_PENDING,
        NULL,
        0,
        NULL,
        0,
        NULL,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
        return status;
    }

    if (deviceHandleMap.count(index) != 0 || deviceHandleMap[index] != INVALID_HANDLE_VALUE && deviceHandleMap[index] != 0) {
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
    }

    return status;
}

BOOL
Us4OemDeallocAll(int index)
{
    BOOL status = TRUE;

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

    printf("Deallocating all DMA buffers on device %d...\n", index);
    status = DeviceIoControl(deviceHandleMap[index],
        US4OEM_WIN32_IOCTL_DEALLOCATE_ALL_DMA_BUFFERS,
        NULL,
        0,
        NULL,
        0,
        NULL,
        NULL);
    if (status == FALSE) {
        printf("DeviceIoControl failed 0x%x\n", GetLastError());
        CloseHandle(deviceHandleMap[index]);
        deviceHandleMap[index] = INVALID_HANDLE_VALUE;
        return status;
    }

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

	std::cout << std::endl << "====== Memory Mapping Test ======" << std::endl;
   
    void* dev0bar4 = NULL;

    for (int i = 0; i < count; i++) {
        std::cout << "Mapping BAR 4 for device " << i << "..." << std::endl;
        void* bar4addr = NULL;
        if ((bar4addr = Us4OemMapBar(i, 4, 0))) {
            if (i == 0) {
                dev0bar4 = bar4addr; // Save the address of BAR 4 for device 0
			}
            std::cout << "success" << std::endl;
        }
        else {
            std::cout << "fail" << std::endl;
        }

        std::cout << "Mapping BAR 0 for device " << i << "..." << std::endl;
        if (Us4OemMapBar(i, 0, 0)) {
            std::cout << "success" << std::endl;
        }
        else {
            std::cout << "fail" << std::endl;
        }
    }

    if (dev0bar4 == NULL) {
        std::cout << "Failed to map BAR 4 for device 0." << std::endl;
        return 1;
	}

    std::cout << std::endl << "====== IRQ Test ======" << std::endl;

    std::cout << "Testing device 0..." << std::endl;
       
	// Read the IRQ count before the test
    Us4OemReadStats(0);
    std::cout << "Asserting IRQ..." << std::endl;

	/* 
    Simulate an IRQ by writing to the mapped BAR 4 for the device
    
    From the QEMU model's mem_write handler:

	switch (offset) {
    [...]
    case 0x60:
        us4oem_raise_irq(us4oem, val);
        break;
    case 0x64:
        us4oem_lower_irq(us4oem, val);
        break;
    [...]
    }
    */

    ((int*)dev0bar4)[0x60/sizeof(int)] = 0x01;
	std::cout << "IRQ asserted." << std::endl;
	((int*)dev0bar4)[0x64/sizeof(int)] = 0x01;
	std::cout << "IRQ cleared." << std::endl;

	// Sleep for a short while to allow the DPC to execute
	Sleep(100);

    // Read the IRQ count after the test
    Us4OemReadStats(0);

    // Poll Test
    std::cout << std::endl << "====== Poll Test ======" << std::endl;
	std::cout << "Polling device 0 for IRQ..." << std::endl;

    Us4OemPoll(0);
    Us4OemReadStats(0);

	// Raise another IRQ to test the non-blocking poll
    ((int*)dev0bar4)[0x60 / sizeof(int)] = 0x01;
    std::cout << "IRQ asserted." << std::endl;
    ((int*)dev0bar4)[0x64 / sizeof(int)] = 0x01;
    std::cout << "IRQ cleared." << std::endl;
    Sleep(100);

	std::cout << "Polling device 0 non-blocking for IRQ (should be successful)..." << std::endl;
    Us4OemReadStats(0);
    Us4OemPollNonBlocking(0);
	Us4OemReadStats(0);

	std::cout << "Polling device 0 non-blocking for IRQ (should be busy)..." << std::endl;
	Us4OemPollNonBlocking(0);
    Us4OemReadStats(0);

	// Pending clear test
	std::cout << std::endl << "====== Clear Pending Test ======" << std::endl;
    
    // Generate some pending IRQs
	((int*)dev0bar4)[0x60 / sizeof(int)] = 0x01;
	std::cout << "IRQ asserted." << std::endl;
	((int*)dev0bar4)[0x64 / sizeof(int)] = 0x01;
	std::cout << "IRQ cleared." << std::endl;
    Sleep(100);

    Us4OemReadStats(0);
	std::cout << "Clearing pending IRQs on device 0..." << std::endl;
    Us4OemPollClearPending(0);
	Us4OemReadStats(0);

	std::cout << std::endl << "====== DMA Contig Alloc Test ======" << std::endl;
    std::cout << "Allocating DMA contiguous buffer length=0x1000 for device 0..." << std::endl;
    std::pair<PVOID, unsigned long long> dmaBuffer = Us4OemAllocDmaContig(0, 0x1000);
    if (dmaBuffer.first) {
        std::cout << "DMA contiguous buffer allocated at VA: 0x" << std::hex << dmaBuffer.first << std::dec << std::endl;
        // Map the allocated DMA buffer
        std::cout << "Mapping DMA buffer for device 0..." << std::endl;
        void* m = Us4OemMapDmaBuf(0, dmaBuffer.first, 0x1000);
        Us4OemReadStats(0);

		ZeroMemory(m, 0x1000); // Zero out the allocated buffer
        std::cout << "Zeroed out the allocated buffer." << std::endl;
        // Read back to verify
        std::cout << "Reading back from the mapped DMA buffer..." << std::endl;
        for (int i = 0; i < 4; i++) {
            std::cout << "Offset " << i * 4 << ": 0x" << std::hex << *(int*)((char*)m + i * 4) << std::dec << std::endl;
		}

        // Allocate second
		std::cout << "Allocating another DMA contiguous buffer length=0x1004 for device 0..." << std::endl;
        std::pair<PVOID, unsigned long long> dmaBuffer2 = Us4OemAllocDmaContig(0, 0x1004);

		// Deallocate the DMA contiguous buffer
        std::cout << "Deallocating DMA contiguous buffer for device 0..." << std::endl;
        if (Us4OemDeallocDmaContig(0, dmaBuffer.second)) {
            std::cout << "[1] DMA contiguous buffer deallocated successfully." << std::endl;
        } else {
            std::cout << "[1] Failed to deallocate DMA contiguous buffer." << std::endl;
		}

        Us4OemReadStats(0);

        // Deallocate the DMA contiguous buffer
        std::cout << "Deallocating DMA contiguous buffer 2 for device 0..." << std::endl;
        if (Us4OemDeallocDmaContig(0, dmaBuffer2.second)) {
            std::cout << "[2] DMA contiguous buffer deallocated successfully." << std::endl;
        }
        else {
            std::cout << "[2] Failed to deallocate DMA contiguous buffer." << std::endl;
        }

        Us4OemReadStats(0);

		// Try deallocating again to see if it fails gracefully
        std::cout << "Trying to deallocate the same DMA contiguous buffer again (should fail)..." << std::endl;
        if (Us4OemDeallocDmaContig(0, dmaBuffer.second)) {
            std::cout << "[1] Deallocated DMA contiguous buffer again, which should not happen." << std::endl;
        }
        else {
            std::cout << "[1] Failed to deallocate DMA contiguous buffer again, as expected." << std::endl;
        }
    } else {
        std::cout << "Failed to allocate DMA contiguous buffer." << std::endl;
	}

	// Big buffer test
	std::cout << std::endl << "====== Big DMA Contig Alloc Test ======" << std::endl;
	// 10 MiB buffer allocation test
	// The spec only asks for 1 MiB, but we can try larger sizes
	unsigned long size = 10 * 1024 * 1024; // 10 MiB
	std::cout << "Allocating large DMA contiguous buffer length=0x" << std::hex << size << std::dec << " for device 0..." << std::endl;
	std::pair<PVOID, unsigned long long> bigDmaBuffer = Us4OemAllocDmaContig(0, size);
    if (bigDmaBuffer.first) {
        std::cout << "Large DMA contiguous buffer allocated at VA: 0x" << std::hex << bigDmaBuffer.first << std::dec << std::endl;
        
        Us4OemReadStats(0);

		// Map the allocated large DMA buffer
		void* m = Us4OemMapDmaBuf(0, bigDmaBuffer.first, size);

		// Try writing to all of the buffer
        for (unsigned long long i = 0; i < size / sizeof(int); i++) {
            ((int*)m)[i] = (int)i; // Fill with sequential values

			// Read back to verify
            if (i % 0x10000 == 0) { // Print every 0x10000th value to avoid flooding the output
                std::cout << "Offset " << std::hex << i * sizeof(int) << ": 0x" << ((int*)m)[i] << std::dec << std::endl;
			}
		}

		// Deallocate the large DMA contiguous buffer
        std::cout << "Deallocating large DMA contiguous buffer for device 0..." << std::endl;
        if (Us4OemDeallocDmaContig(0, bigDmaBuffer.second)) {
            std::cout << "Large DMA contiguous buffer deallocated successfully." << std::endl;
        }
        else {
            std::cout << "Failed to deallocate large DMA contiguous buffer." << std::endl;
        }
    } else {
		std::cout << "Failed to allocate large DMA contiguous buffer." << std::endl;
	}

	// Many allocations test
	std::cout << std::endl << "====== Many DMA Contig Alloc Test ======" << std::endl;
	std::cout << "Allocating many small DMA contiguous buffers for device 0..." << std::endl;
	int numAllocations = 100;
	std::vector<std::pair<PVOID, unsigned long long>> allocations;
    for (int i = 0; i < numAllocations; i++) {
        std::pair<PVOID, unsigned long long> currentBuf = Us4OemAllocDmaContig(0, 0xA0, /*quiet*/ true);

        if (!currentBuf.first) {
            std::cout << "Failed to allocate DMA contiguous buffer " << i << "." << std::endl;
            break; // Stop if allocation fails
        }
        else {
            allocations.push_back(currentBuf);
        }
    }
	// Cleanup allocated buffers
    for (const auto& alloc : allocations) {
        if (!Us4OemDeallocDmaContig(0, alloc.second)) {
            std::cout << "Failed to deallocate DMA contiguous buffer." << std::endl;
        }
	}

	std::cout << "All allocated DMA contiguous buffers deallocated." << std::endl;

	// Alloc a few buffers to test deallocation
	std::cout << std::endl << "====== Deallocate All Test ======" << std::endl;
	std::cout << "Allocating a few DMA contiguous buffers for device 0..." << std::endl;
	std::vector<std::pair<PVOID, unsigned long long>> testAllocations;

    for (int i = 0; i < 5; i++) {
        std::pair<PVOID, unsigned long long> currentBuf = Us4OemAllocDmaContig(0, 0x100, /*quiet*/ true);
        if (!currentBuf.first) {
            std::cout << "Failed to allocate DMA contiguous buffer " << i << "." << std::endl;
            break; // Stop if allocation fails
        }
        else {
            testAllocations.push_back(currentBuf);
        }
	}
	Us4OemReadStats(0);
    Us4OemDeallocAll(0);
    Us4OemReadStats(0);

    // Clean up
    if (dev0bar4) {
        VirtualFree(dev0bar4, 0, MEM_RELEASE);
        dev0bar4 = NULL;
	}
    for (auto& pair : deviceInterfaceDetailMap) {
        free(pair.second);
    }
    deviceInterfaceDetailMap.clear();
    deviceHandleMap.clear();
	SetupDiDestroyDeviceInfoList(hDevInfo);

	return 0;
}