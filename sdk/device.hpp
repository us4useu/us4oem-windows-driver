#pragma once

#include "devicelocation.hpp"
#include "common.hpp"

class Us4OemDevice {
public:
	Us4OemDevice(const Us4OemDeviceLocation& loc) :
		location(loc),
		deviceHandle(INVALID_HANDLE_VALUE), 
		isHandleOpen(false) {}

	// Opens the device
	bool open() {
		if (isHandleOpen) {
			return true; // Already open
		}

		deviceHandle = CreateFileA(location.getSystemPath().c_str(),
			GENERIC_READ | GENERIC_WRITE,
			NULL,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		
		return isHandleOpen = deviceHandle != INVALID_HANDLE_VALUE;
	}

	bool isOpen() {
		return isHandleOpen;
	}

	// Closes the device handle.
	void close() {
		if (isHandleOpen) {
			CloseHandle(deviceHandle);
			deviceHandle = INVALID_HANDLE_VALUE;
			isHandleOpen = false;
		}
	}

	// Returns the device location.
	const Us4OemDeviceLocation& getLocation() const {
		return location;
	}

	// Returns true if major and minor parts of the driver version match the SDK version we're built against.
	bool isKmdCompatible() {
		// Version is 4 bytes: magic (1 byte) | major (1 byte) | minor (1 byte) | patch (1 byte)
		// We care about the major/minor only, as there shouldn't be any breaking changes in the patch version.
		const unsigned long UPPER_24_BITS = 0xFFFFFF00;

		return ((getDriverVersion() & UPPER_24_BITS) == (US4OEM_DRIVER_VERSION & UPPER_24_BITS));
	}

	// Retrieves the driver version.
	unsigned long getDriverVersion() {
		us4oem_driver_info driverInfo;

		ioctl(US4OEM_WIN32_IOCTL_GET_DRIVER_INFO, NULL, 0, &driverInfo, sizeof(driverInfo));

		return driverInfo.version;
	}

	// Retrieves the driver version as a string.
	std::string getDriverVersionString() {
		us4oem_driver_info driverInfo;

		ioctl(US4OEM_WIN32_IOCTL_GET_DRIVER_INFO, NULL, 0, &driverInfo, sizeof(driverInfo));

		return std::format("{} {}.{}.{}",
			std::string(driverInfo.name),
			(driverInfo.version & 0x00FF0000) >> 16,
			(driverInfo.version & 0x0000FF00) >> 8,
			driverInfo.version & 0x000000FF);
	}

private:
	// Wrapper for DeviceIoControl to reduce boilerplate.
	bool ioctl(unsigned long ioctlCode, void* inputBuffer, unsigned long inputSize, void* outputBuffer, unsigned long outputSize) {
		if (!isHandleOpen) {
			throw std::runtime_error("Device handle is not open");
		}

		bool status = DeviceIoControl(deviceHandle,
			ioctlCode,
			inputBuffer, inputSize,
			outputBuffer, outputSize,
			NULL, NULL);

		if (!status) {
			throw std::runtime_error("DeviceIoControl failed: " + std::to_string(GetLastError()));
		}

		return true;
	}

	Us4OemDeviceLocation location;
	HANDLE deviceHandle; // Win32 handle to the device
	bool isHandleOpen; // Whether the device is open
};