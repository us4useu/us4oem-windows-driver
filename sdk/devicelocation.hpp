#pragma once

#include "common.hpp"

// Retrieves an unique identifier for the device.
// Used to pick an appropriate device to communicate with.
const class Us4OemDeviceLocation {
public:
	Us4OemDeviceLocation(uint32_t address, uint32_t bus, std::string path) :
		Us4OemDeviceLocation(
			(address & 0xFFFF0000) >> 16, 
			address & 0x0000FFFF, 
			bus, 
			path) {}

	Us4OemDeviceLocation(uint16_t device, uint16_t function, uint32_t bus, std::string path) :
		device(device),
		function(function),
		bus(bus),
		path(path) {}

	bool operator==(const Us4OemDeviceLocation& other) const {
		return device == other.device && function == other.function && bus == other.bus;
	}

	std::string toString() const {
		return std::format("PCI Bus {} Device {} Function {}",
			bus, device, function);
	}

	std::string getSystemPath() const {
		// The path we get from Us4OemDriverSdk is actually not a system path, but an NT device manager path.
		// To access it we can use the \\.\GLOBALROOT prefix.
		// We could also just get the device symlink using other means, but this is cleaner.
		return "\\\\.\\GLOBALROOT" + path;
	}
private:
	// NOTE: Windows lacks any clear documentation on the format of DEVPKEY_Device_Address,
	// this assumes this one random KB article is correct: 
	// https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/obtaining-device-configuration-information-at-irql---dispatch-level
	const uint16_t device; // Device address; upper 16 bits of DEVPKEY_Device_Address
	const uint16_t function; // Function number; lower 16 bits of DEVPKEY_Device_Address

	const uint32_t bus; // Bus number; see DEVPKEY_Device_BusNumber

	const std::string path; // System path of the device; note: redundant for checking equality
};