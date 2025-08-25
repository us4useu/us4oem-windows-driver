#include "sdk.hpp"

#include <iostream>
#include <thread>

const bool QEMU_TEST = false;

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

void torture(const Us4OemDeviceLocation& location) {
	std::cout << "=====> Torture round starting on " << location.toString() << std::endl;

	unsigned long contigSize = 5 * 1024 * 1024; // 5 MiB
	size_t sgAllocSize1 = GiB * size_t(10); // 10 GiB
	size_t sgAllocSize2 = MiB * 64; // 64 MiB

	std::vector<Us4OemDmaSgDescription> desc1 = {};
	std::vector<Us4OemDmaSgDescription> desc2 = {};

	Us4OemDevice d(location);

	if (!d.open()) {
		std::cerr << "Failed to open device for contiguous allocation." << std::endl;
		return;
	}

	d.setStickyMode(false);

	auto desc = d.allocDmaContig(contigSize);
	
	d.allocDmaScatterGather(sgAllocSize1, desc1);
	d.allocDmaScatterGather(sgAllocSize2, desc2);

	if (desc1.size() == 0 || desc2.size() == 0) {
		std::cerr << "Failed to allocate scatter-gather DMA buffer." << std::endl;
		throw std::runtime_error("Failed to allocate scatter-gather DMA buffer.");
	}

	if (!d.deallocDmaContig(desc.pa)) {
		std::cerr << "Failed to deallocate contiguous DMA buffer." << std::endl;
		throw std::runtime_error("Failed to deallocate contiguous DMA buffer.");
	}

	if (!d.deallocDmaScatterGather(desc1)) {
		std::cerr << "Failed to deallocate scatter-gather DMA buffer." << std::endl;
		throw std::runtime_error("Failed to deallocate scatter-gather DMA buffer.");
	}

	// Map the small SG allocation
	d.mapDmaBuf(desc2[0].va, 0);

	/*if (!d.deallocDmaScatterGather(desc2)) {
		std::cerr << "Failed to deallocate scatter-gather DMA buffer." << std::endl;
		throw std::runtime_error("Failed to deallocate scatter-gather DMA buffer.");
	}*/

	if (!d.deallocAll()) {
		std::cerr << "Failed to deallocate all DMA buffers." << std::endl;
		throw std::runtime_error("Failed to deallocate all DMA buffers.");
	}

	// Check stats for allocation counts
	Us4OemDeviceStats stats = d.readStats();
	if (stats.dmaContigAllocCount != 0 || stats.dmaSgAllocCount != 0) {
		std::cerr << "DMA allocation counts are not zero after deallocation!" << std::endl;
		std::cerr << "Stats: " << std::endl << stats.toString() << std::endl;
		throw std::runtime_error("DMA allocation counts are not zero after deallocation!");
	}

	std::cout << "<===== Round OK on " << location.toString() << std::endl;

	// Close is handled by destructor
}

void test(const Us4OemDeviceLocation& location) {
	std::cin.get();

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
	std::cout << "BAR 0 mapped at: 0x" << std::hex << bar0.address << " length: 0x" << bar0.lengthMapped << std::dec << std::endl;
	std::cout << "  @ offset 0x0: 0x" << std::hex << *(unsigned int*)((char*)bar0.address + 0x0) << std::dec << std::endl;

	// Bar 4 mapping test
	auto bar4 = d.mapBar(4);
	std::cout << "BAR 4 mapped at: 0x" << std::hex << bar4.address << " length: 0x" << bar4.lengthMapped << std::dec << std::endl;
	std::cout << "  @ offset 0x0: 0x" << std::hex << *(unsigned int*)((char*)bar4.address + 0x0) << std::dec << std::endl;
	if (QEMU_TEST) {
		// We are expecting 0x0 to be 0x0100_00ED on QEMU
		if (*(unsigned int*)((char*)bar4.address + 0x0) != 0x010000ED) {
			std::cerr << "Unexpected value at BAR 4 offset 0x0: 0x" << std::hex << *(unsigned int*)((char*)bar4.address + 0x0) << std::dec << std::endl;
			return;
		}
	}

	// Read stats
	std::cout << std::endl << "====== Read Stats Test ======" << std::endl;
	std::cout << "Stats: " << std::endl << d.readStats().toString() << std::endl;

	// IRQ Polling Test
	std::cout << std::endl << "====== IRQ Polling Test ======" << std::endl;
	// We can only test this in QEMU, as it simulates the IRQs.
	if (QEMU_TEST) {
		// Assert an IRQ
		qemuTriggerIrq(bar4.address);

		std::cout << "Polling for IRQ..." << std::endl;
		d.poll();
		std::cout << "IRQ received." << std::endl;

		// Trigger another IRQ
		qemuTriggerIrq(bar4.address);

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
		qemuTriggerIrq(bar4.address);

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

	// DMA Contiguous Buffer Allocation Test
	std::cout << std::endl << "====== DMA Contiguous Buffer Alloc Test ======" << std::endl;

	// Let's alloc 1 MiB as that's the max size, per the spec.
	unsigned long size = 1024 * 1024; // 1 MiB
	std::cout << "Allocating DMA contiguous buffer length=0x" << std::hex << size << std::dec << " for device..." << std::endl;

	auto dmaBuffer = d.allocDmaContig(size);
	if (dmaBuffer.va) {
		std::cout << "DMA contiguous buffer allocated at VA: 0x" << std::hex << dmaBuffer.va << " PA: 0x" << dmaBuffer.pa << std::dec << std::endl;
		
		// Map the allocated DMA buffer
		std::cout << "Mapping DMA buffer..." << std::endl;
		auto mappedBuffer = d.mapDmaBuf(dmaBuffer.va);

		if (mappedBuffer.address) {
			std::cout << "Mapped DMA buffer at: 0x" << std::hex << mappedBuffer.address << std::dec << std::endl;
			

			// Write anything to check if the mapping works
			std::cout << "Writing to the mapped DMA buffer..." << std::endl;
			static_cast<int*>(mappedBuffer.address)[0] = 0xAA; // Write a byte to the first byte of the buffer
			// Read back to verify
			std::cout << "Reading back from the mapped DMA buffer..." << std::endl;
			if (static_cast<int*>(mappedBuffer.address)[0] == 0xAA) {
				std::cout << "Mapped DMA buffer read back successfully." << std::endl;
			} else {
				std::cerr << "Mapped DMA buffer read back failed." << std::endl;
				return;
			}

		} else {
			std::cerr << "Failed to map DMA buffer." << std::endl;
			return;
		}

		// Deallocate the DMA contiguous buffer
		std::cout << "Deallocating DMA contiguous buffer..." << std::endl;
		if (d.deallocDmaContig(dmaBuffer.pa)) {
			std::cout << "DMA contiguous buffer deallocated successfully." << std::endl;
		} else {
			std::cerr << "Failed to deallocate DMA contiguous buffer." << std::endl;
			return;
		}
		
	} else {
		std::cerr << "Failed to allocate DMA contiguous buffer." << std::endl;
	}

	// DMA Scatter-Gather Buffer Allocation Test
	std::cout << std::endl << "====== DMA Scatter-Gather Buffer Alloc Test ======" << std::endl;

	// Sanity check
	d.deallocAll();

	// Let's allocate a large scatter-gather buffer
	size_t sgAllocSize = QEMU_TEST ? 
		(GiB * 2) : // 2 GiB on QEMU
		(GiB * (size_t)22); // 22 GiB on hardware (the test laptop has 32 GiB of RAM)

	std::cout << "Allocating DMA scatter-gather buffer length=0x" << std::hex << sgAllocSize  
		<< " for device..." << std::dec << std::endl;

	std::vector<Us4OemDmaSgDescription> desc = {};

	try {
		d.allocDmaScatterGather(sgAllocSize, desc);
	} catch (const std::runtime_error& e) {
		std::cerr << "Failed to allocate DMA scatter-gather buffer: " << e.what() << std::endl;
		return;
	}

	std::cout << "DMA scatter-gather buffer allocated successfully." << std::endl;

	size_t actualAllocLength = 0;
	size_t numChunks = 0;

	for (const auto& dsc : desc) {
		actualAllocLength += dsc.length;
		numChunks += dsc.chunks.size();
		std::cout << "  SG buffer at VA: 0x" << std::hex << dsc.va << std::dec 
			<< " length: 0x" << std::hex << dsc.length << std::dec
			<< " in " << dsc.chunks.size() << " chunks." << std::endl;
	}
	std::cout << "Total allocated length: 0x" << std::hex << actualAllocLength 
		<< std::dec << " in " << numChunks << " chunks." << std::endl;

	// Map the first chunk of the first descriptor to test
	auto mappedBuffer = d.mapDmaBuf(desc.front().va);
	if (mappedBuffer.address) {
		std::cout << "Mapped first chunk of first SG DMA buffer at: 0x" << std::hex << mappedBuffer.address << std::dec << std::endl;
		// Write anything to check if the mapping works
		std::cout << "Writing to the mapped SG DMA buffer..." << std::endl;
		static_cast<int*>(mappedBuffer.address)[0] = 0xBB; // Write a byte to the first byte of the buffer
		// Read back to verify
		std::cout << "Reading back from the mapped SG DMA buffer..." << std::endl;
		if (static_cast<int*>(mappedBuffer.address)[0] == 0xBB) {
			std::cout << "Mapped SG DMA buffer read back successfully." << std::endl;
		}
		else {
			std::cerr << "Mapped SG DMA buffer read back failed." << std::endl;
			return;
		}
	}
	else {
		std::cerr << "Failed to map SG DMA buffer." << std::endl;
		return;
	}

	std::cin.get();

	// Deallocate the DMA scatter-gather buffer
	std::cout << "Deallocating DMA scatter-gather buffer..." << std::endl;
	if (d.deallocDmaScatterGather(desc)) {
		std::cout << "DMA scatter-gather buffer deallocated successfully." << std::endl;
	} else {
		std::cerr << "Failed to deallocate DMA scatter-gather buffer." << std::endl;
		return;
	}

	// Print stats
	Us4OemDeviceStats stats = d.readStats();
	std::cout << "Stats after deallocating: " << std::endl << stats.toString() << std::endl;

	// Allocate 100 MiB and 10 MiB
	std::cout << std::endl << "====== DMA Deallocate All Test ======" << std::endl;

	d.allocDmaScatterGather(MiB * 100, desc);
	d.allocDmaScatterGather(MiB * 10, desc);

	stats = d.readStats();
	std::cout << "Stats before deallocating: " << std::endl << stats.toString() << std::endl;

	std::cout << "Deallocating all DMA buffers..." << std::endl;
	if (d.deallocAll()) {
		std::cout << "All DMA buffers deallocated successfully." << std::endl;
	} else {
		std::cerr << "Failed to deallocate all DMA buffers." << std::endl;
		return;
	}

	stats = d.readStats();
	std::cout << "Stats after deallocating: " << std::endl << stats.toString() << std::endl;

	// Set sticky mode
	std::cout << std::endl << "====== Sticky Mode Test ======" << std::endl;
	if (d.setStickyMode(true)) {
		std::cout << "Sticky mode set successfully." << std::endl;
	} else {
		std::cerr << "Failed to set sticky mode." << std::endl;
		return;
	}

	// Allocate a DMA contiguous buffer to test sticky mode
	auto stickyBuffer = d.allocDmaContig(1024 * 1024); // 1 MiB

	// Now close the device handle
	d.close();
	d.open();

	// Check if there's still a buffer allocated
	stats = d.readStats();
	if (stats.dmaContigAllocCount > 0) {
		std::cerr << 
			"Sticky mode failed: DMA contiguous buffer is still allocated after reopening the device." << std::endl;
		return;
	} else {
		std::cerr << 
			"Sticky mode works: no DMA contiguous buffer allocated after reopening the device." << std::endl;
	}
}

int main(int argc, char* argv[]) {
	Us4OemDriverSdk sdk = Us4OemDriverSdk();

	if (QEMU_TEST) {
		std::cout << "Running in QEMU." << std::endl;
	}
	else {
		std::cout << "Running on a real device." << std::endl;
	}

	size_t deviceCount = sdk.getDeviceCount();
	std::cout << "Found " << deviceCount << " us4oem devices." << std::endl;

	if (argc == 1) {
		// Help
		std::cout << "Usage: " << std::endl;
		std::cout << "  " << argv[0] << " list" << std::endl << "    List all devices and exit" << std::endl;
		std::cout << "  " << argv[0] << " test" << std::endl << "    Basic test of all the functions" << std::endl;
		std::cout << "  " << argv[0] << " torture" << std::endl << "    Torture test for bugcheck hunting and looking for memory leaks (press enter to stop)" << std::endl;

		return 0;
	}

	std::string command = argv[1];

	if (command == "list") {
		for (int i = 0; i < deviceCount; ++i) {
			const Us4OemDeviceLocation loc = sdk.getDeviceLocation(i);
			std::cout << "Device " << i << ": " << loc.toString() << std::endl;
			std::cout << "  System path: " << loc.getSystemPath() << std::endl;
		}
		return 0;
	} else if (command == "torture") {

		if (deviceCount == 0) {
			std::cerr << "No devices found; cannot torture test." << std::endl;
			return 1;
		}

		std::cout << "Torture test running. Press Enter to stop." << std::endl;
		
		// Start threads for each device
		std::vector<std::jthread> threads;
		std::stop_source stop_source;

		// Execute until ctrl-c
		for (int i = 0; i < deviceCount; ++i) {
			const Us4OemDeviceLocation loc = sdk.getDeviceLocation(i);
			threads.emplace_back([loc, &stop_source]() {
				while (!stop_source.get_token().stop_requested()) {
					try {
						torture(loc);
					} catch (const std::exception& e) {
						std::cerr << "Exception in torture test thread: " << e.what() << std::endl;
						while (true);
					}
				}
				});
		}

		std::cin.get();
		stop_source.request_stop();

		return 0;

	} else if (command == "test") {

		for (int i = 0; i < deviceCount; ++i) {
			const Us4OemDeviceLocation loc = sdk.getDeviceLocation(i);

			test(loc);
		}
	}
	else {
		// Unknown command
		std::cerr << "Unknown command: " << command << std::endl;
		return 1;
	}

	return 0;
}