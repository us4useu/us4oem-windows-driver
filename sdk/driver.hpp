#pragma once

#include "common.hpp"

class Us4OemDriverSdk {
public:
	Us4OemDriverSdk() : deviceCount(0) {}

	// Triggers a device scan and returns the number of devices found.
	size_t getDeviceCount() {
		_scanDevices();

		return deviceLocations.size();
	}

	// Retrieves the device location for a given index.
	// The index has to be less than the number of devices returned by GetDeviceCount().
	Us4OemDeviceLocation getDeviceLocation(int index) {
		if (index < 0 || index >= deviceLocations.size()) {
			throw std::out_of_range("Index out of range");
		}
		return deviceLocations[index];
	}

	// Return the KMD version this SDK was built against (aka which us4oemapi.h was included).
	unsigned long builtAgainstKmdVersion() const {
		return US4OEM_DRIVER_VERSION;
	}


private:
	bool _scanDevices() {
		deviceLocations.clear();

		// Retrieve the device information for all us4oem devices.
		HDEVINFO devInfo = SetupDiGetClassDevs(
			&GUID_DEVINTERFACE_us4oem,
			/* Enumerator */ NULL,
			/* Window of the installer (setupapi) */ NULL,
			DIGCF_DEVICEINTERFACE |
			DIGCF_PRESENT);

		if (devInfo == INVALID_HANDLE_VALUE) {
			return false; // Failed to get device info
		}

		SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
		// The existence of this field is not elaborated on in the documentation. Thanks Microsoft.
		deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

		// Iterate over devices until we fail to read one
		int count = 0;
		while (SetupDiEnumDeviceInterfaces(devInfo,
			/* DeviceInfoData */ NULL,
			&GUID_DEVINTERFACE_us4oem,
			count++,  //Cycle through the available devices.
			&deviceInterfaceData)
			) {
			// We found a device, now read its details

			SP_DEVINFO_DATA deviceInfoData;
			// Again, no elaboration. Surely there must be a better way to manage possible future changes?
			deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

			SetupDiGetDeviceInterfaceDetail(devInfo,
				&deviceInterfaceData,
				/* DeviceInterfaceDetailData */ NULL,
				/* DeviceInterfaceDetailDataSize */ 0,
				NULL,
				&deviceInfoData);


			// Now we can actually start reading the device properties.
			uint32_t address;
			uint32_t busNumber;

			SetupDiGetDeviceRegistryProperty(devInfo,
				&deviceInfoData,
				SPDRP_BUSNUMBER,
				/* PropertyRegDataType */ NULL,
				(unsigned char*)(&busNumber), // static_cast HATES this, so use a C-style cast
				sizeof(busNumber),
				/* [out] RequiredSize */ NULL);

			SetupDiGetDeviceRegistryProperty(devInfo,
				&deviceInfoData,
				SPDRP_ADDRESS,
				/* PropertyRegDataType */ NULL,
				(unsigned char*)(&address),
				sizeof(address),
				/* [out] RequiredSize */ NULL);

			// Get the system path of the device
			// As all functions dealing with "strings" in the Windows API, this is awful.

			// Realistically this is overkill, but better to reserve the space now than deal with a bug later
			char systemPathBuffer[MAX_PATH]; 

			SetupDiGetDeviceRegistryPropertyA(devInfo,
				&deviceInfoData,
				SPDRP_PHYSICAL_DEVICE_OBJECT_NAME,
				/* PropertyRegDataType */ NULL,
				(unsigned char*)(&systemPathBuffer),
				MAX_PATH,
				/* [out] RequiredSize */ NULL);

			// Create a device location object and add it to the list
			deviceLocations.emplace_back(Us4OemDeviceLocation(address, busNumber, std::string(systemPathBuffer)));
		}

		deviceCount = deviceLocations.size();

		return true;
	}

	size_t deviceCount;
	std::vector<Us4OemDeviceLocation> deviceLocations;
};