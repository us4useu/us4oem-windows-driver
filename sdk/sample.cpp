#include "sdk.hpp"

#include <iostream>

const bool QEMU_TEST = true;

bool qemuTriggerIrq(void* bar4) {
	if (!QEMU_TEST) {
		std::cerr << "QEMU test is disabled; cannot trigger IRQ." << std::endl;
		return false;
	}
	// Assert an IRQ
	((int*)bar4)[0x60 / sizeof(int)] = 0x01;
	((int*)bar4)[0x64 / sizeof(int)] = 0x01;
	Sleep(100); // Wait a bit for QEMU to trigger the IRQ, Windows to handle it, DPC to fire etc.
	return true;
}

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

	// IRQ Polling Test
	std::cout << std::endl << "====== IRQ Polling Test ======" << std::endl;
	// We can only test this in QEMU, as it simulates the IRQs.
	if (QEMU_TEST) {
		// Assert an IRQ
		qemuTriggerIrq(bar4.first);

		std::cout << "Polling for IRQ..." << std::endl;
		d.poll();
		std::cout << "IRQ received." << std::endl;

		// Trigger another IRQ
		qemuTriggerIrq(bar4.first);

		// Test non-blocking poll
		std::cout << "Polling for IRQ non-blocking..." << std::endl;
		if (d.pollNonBlocking()) {
			std::cout << "IRQ received (non-blocking)." << std::endl;
		} else {
			std::cerr << "No IRQ pending (non-blocking)." << std::endl;
			return;
		}

		// Try another time, this time we should get false
		std::cout << "Polling for IRQ non-blocking (another time)..." << std::endl;
		if (d.pollNonBlocking()) {
			std::cerr << "IRQ received (non-blocking). This is incorrect." << std::endl;
			return;
		}
		else {
			std::cout << "No IRQ pending (non-blocking). This is expected." << std::endl;
		}

		// Create another IRQ, and clear pending IRQs
		qemuTriggerIrq(bar4.first);

		std::cout << "Clearing pending IRQs..." << std::endl;
		d.pollClearPending();

		Us4OemDeviceStats stats = d.readStats();
		std::cout << "Stats after clearing pending IRQs: " << std::endl << stats.toString() << std::endl;
		if (stats.pendingIrqCount != 0) {
			std::cerr << "Pending IRQ count is not zero after clearing: " << stats.pendingIrqCount << std::endl;
			return;
		}

	} else {
		std::cout << "Skipping IRQ polling test, not running in QEMU." << std::endl;
	}
}

int main() {
	Us4OemDriverSdk sdk = Us4OemDriverSdk();

	if (QEMU_TEST) {
		std::cout << "Running in QEMU." << std::endl;
	}

	size_t deviceCount = sdk.getDeviceCount();
	std::cout << "Found " << deviceCount << " us4oem devices." << std::endl;
	for (int i = 0; i < deviceCount; ++i) {
		const Us4OemDeviceLocation loc = sdk.getDeviceLocation(i);
			
		test(loc);
	}

	return 0;
}