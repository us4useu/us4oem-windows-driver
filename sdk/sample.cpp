#include "sdk.hpp"

#include <iostream>

void test(const Us4OemDeviceLocation& location) {
	std::cout << std::endl << "========== Testing on " << location.toString() << "==========" << std::endl;
	std::cout << "System path: " << location.getSystemPath() << std::endl;

	// Open device
	std::cout << std::endl << "====== Open Device Test ======" << std::endl;
	Us4OemDevice d(location);
	if (d.open()) {
		std::cout << "Opened successfully!" << std::endl;
	}
	else {
		std::cerr << "Failed to open." << std::endl;
		return;
	}

	// Read driver version
	std::cout << std::endl << "====== Driver Version Test ======" << std::endl;
	std::cout << "Is KMD compatible: " << (d.isKmdCompatible() ? "yes" : "no") << std::endl;
	std::cout << "KMD version: 0x" << std::hex << d.getDriverVersion();
	std::cout << "; API version: 0x" << US4OEM_DRIVER_VERSION << std::dec << std::endl;
	std::cout << "KMD ID string: " << d.getDriverVersionString() << std::endl;
	if (!d.isKmdCompatible()) {
		std::cerr << "KMD not compatible; not continuing." << std::endl;
		return;
	}

	// Bar 0 mapping test
	std::cout << std::endl << "====== BAR Mapping Test ======" << std::endl;
	auto bar0 = d.mapBar(0);
	std::cout << "BAR 0 mapped at: 0x" << std::hex << bar0.first << " length: 0x" << bar0.second << std::dec << std::endl;
	std::cout << "  @ offset 0x0: 0x" << std::hex << *(unsigned int*)((char*)bar0.first + 0x0) << std::dec << std::endl;

	// Bar 4 mapping test
	auto bar4 = d.mapBar(4);
	std::cout << "BAR 4 mapped at: 0x" << std::hex << bar4.first << " length: 0x" << bar4.second << std::dec << std::endl;
	std::cout << "  @ offset 0x0: 0x" << std::hex << *(unsigned int*)((char*)bar4.first + 0x0) << std::dec << std::endl;
	// We are expecting 0x0 to be 0x0100_00ED
	if (*(unsigned int*)((char*)bar4.first + 0x0) != 0x010000ED) {
		std::cerr << "Unexpected value at BAR 4 offset 0x0: 0x" << std::hex << *(unsigned int*)((char*)bar4.first + 0x0) << std::dec << std::endl;
		return;
	}

	// Read stats
	std::cout << std::endl << "====== Read Stats Test ======" << std::endl;
	std::cout << "Stats: " << std::endl << d.readStats().toString() << std::endl;
}

int main() {
	Us4OemDriverSdk sdk = Us4OemDriverSdk();

	size_t deviceCount = sdk.getDeviceCount();
	std::cout << "Found " << deviceCount << " us4oem devices." << std::endl;
	for (int i = 0; i < deviceCount; ++i) {
		const Us4OemDeviceLocation loc = sdk.getDeviceLocation(i);
			
		test(loc);
	}

	return 0;
}