#pragma once

#include "devicelocation.hpp"
#include "sg.hpp"
#include "common.hpp"

// This is ~awful and unsafe~, but in the specific use below it's basically the only way to
// have a function template that can take nullptr or a pointer type.
template<class T>
concept NullablePtr = (std::is_pointer_v<T> || std::is_null_pointer_v<T>) && !std::is_const_v<T>;

class Us4OemDevice {
public:
	Us4OemDevice(const Us4OemDeviceLocation& loc) :
		location(loc),
		deviceHandle(INVALID_HANDLE_VALUE), 
		isHandleOpen(false) {}

	~Us4OemDevice() {
		if (isHandleOpen) {
			close(); // Ensure the handle is closed on destruction
		}
	}

	// A pair of virtual and physical addresses.
	struct VirtualAndPhysicalAddress {
		void* va; // Virtual address of the area; note: not accessible from user space - needs mapping
		size_t pa; // Physical address of the area
	};

	struct MemoryMapping {
		void* address; // Virtual address of the mapped area
		size_t lengthMapped; // Length of the mapped area
	};

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
		us4oem_driver_info driverInfo = {};

		ioctl(US4OEM_WIN32_IOCTL_GET_DRIVER_INFO, nullptr, &driverInfo);

		return driverInfo.version;
	}

	// Retrieves the driver version as a string.
	std::string getDriverVersionString() {
		us4oem_driver_info driverInfo = {};

		ioctl(US4OEM_WIN32_IOCTL_GET_DRIVER_INFO, nullptr, &driverInfo);

		return std::format("{} {}.{}.{}",
			std::string(driverInfo.name),
			(driverInfo.version & 0x00FF0000) >> 16,
			(driverInfo.version & 0x0000FF00) >> 8,
			driverInfo.version & 0x000000FF);
	}

	// Map BAR 0/4 to userspace
	MemoryMapping mapBar(int bar) {
		if (bar != 0 && bar != 4) {
			throw std::range_error("Only BAR 0 and 4 are supported!");
		}

		us4oem_mmap_argument arg = {};
		arg.area = (bar == 0) ? MMAP_AREA_BAR_0 : MMAP_AREA_BAR_4;
		arg.length_limit = 0; // Map the whole area
		// arg.va is ignored for BARs

		us4oem_mmap_response response = {};
		ioctl(US4OEM_WIN32_IOCTL_MMAP, &arg, &response);

		if (response.address == NULL) {
			throw std::runtime_error("Failed to map BAR " + std::to_string(bar));
		}

		return { response.address, response.length_mapped };
	}

	// Map DMA buffer to userspace.
	MemoryMapping mapDmaBuf(void* va, unsigned long length_limit = 0) {
		us4oem_mmap_argument arg = {};
		arg.area = MMAP_AREA_DMA;
		arg.va = va;
		arg.length_limit = length_limit;

		us4oem_mmap_response response = {};

		ioctl(US4OEM_WIN32_IOCTL_MMAP, &arg, &response);

		if (response.address == NULL) {
			throw std::runtime_error("Failed to map DMA buffer");
		}

		return { response.address, response.length_mapped };
	}

	// Read stats
	Us4OemDeviceStats readStats() {
		us4oem_stats stats = {};

		ioctl(US4OEM_WIN32_IOCTL_READ_STATS, nullptr, &stats);

		return Us4OemDeviceStats(stats);
	}

	// Polls the device for pending IRQs. Note: BLOCKS THREAD UNTIL AN IRQ IS RECEIVED, IF NONE ARE PENDING.
	bool poll() {
		return ioctl(US4OEM_WIN32_IOCTL_POLL, nullptr, nullptr);
	}

	// Non-blocking poll for pending IRQs. Returns true if an IRQ is pending, false otherwise.
	bool pollNonBlocking() {
		try {
			ioctl(US4OEM_WIN32_IOCTL_POLL_NONBLOCKING, nullptr, nullptr);
			return true; // IRQ is pending
		}
		catch (const std::runtime_error&) {
			// TODO: Handle other errors
			return false;
		}
	}

	// Clears all pending IRQs. Note: this does not complete any poll requests.
	bool pollClearPending() {
		return ioctl<nullptr_t,nullptr_t>(US4OEM_WIN32_IOCTL_CLEAR_PENDING, nullptr, nullptr);
	}

	// Alloc contiguous DMA buffer.
	VirtualAndPhysicalAddress allocDmaContig(unsigned long length) {
		us4oem_dma_contiguous_buffer_response response = {};
		us4oem_dma_allocation_argument arg = {};
		arg.length = length;

		ioctl(US4OEM_WIN32_IOCTL_ALLOCATE_DMA_CONTIGIOUS_BUFFER, &arg, &response);

		if (response.va == NULL) {
			throw std::runtime_error("Failed to allocate contiguous DMA buffer");
		}

		return { response.va, response.pa };
	}

	// Dealloc contiguous DMA buffer.
	bool deallocDmaContig(unsigned long long pa) {
		return ioctl(US4OEM_WIN32_IOCTL_DEALLOCATE_DMA_CONTIGIOUS_BUFFER, &pa, nullptr);
	}

	// Allocates a scatter-gather DMA buffer.
	// Note: due to how the driver works, we have to use ugly malloc to allocate the response buffer.
	bool allocDmaScatterGather(unsigned long length, unsigned long chunkSize, std::vector<Us4OemDmaSgChunk>& addresses) {

		unsigned long needed_size = US4OEM_DMA_SG_RESPONSE_NEEDED_SIZE(US4OEM_DMA_SG_CHUNK_COUNT(length, chunkSize));

		auto response = (us4oem_dma_scatter_gather_buffer_response*)malloc(needed_size);

		us4oem_dma_allocation_argument arg = {};
		arg.length = length;
		arg.chunk_size = chunkSize;

		// Use raw ioctl as the template can only deduce the size of const types.
		ioctlRaw(US4OEM_WIN32_IOCTL_ALLOCATE_DMA_SG_BUFFER, 
			&arg, 
			sizeof(us4oem_dma_allocation_argument), 
			response, 
			needed_size);

		if (response->chunk_count == 0) {
			free(response);
			return false;
		}

		addresses.reserve(response->chunk_count);

		for (unsigned int i = 0; i < response->chunk_count; i++) {
			addresses.push_back({
				response->chunks[i].va, // Virtual address of the chunk
				response->chunks[i].pa,  // Physical address of the chunk
				response->chunks[i].length // Length of the chunk
			});
		}

		free(response); 

		return true;
	}

	bool deallocDmaScatterGather(std::vector<Us4OemDmaSgChunk>& chunks) {
		if (chunks.empty()) {
			return true; // Nothing to deallocate
		}

		for (const auto& chunk : chunks) {
			void* va = chunk.va; // Get rid of constness
			ioctl(US4OEM_WIN32_IOCTL_DEALLOCATE_DMA_SG_BUFFER, 
				&va,
				nullptr);
		}

		chunks.clear(); // Clear the vector after deallocation

		return true;
	}

	bool deallocAll() {
		return ioctl(US4OEM_WIN32_IOCTL_DEALLOCATE_ALL_DMA_BUFFERS, nullptr, nullptr);
	}

private:
	// A wrapper^2 of the ioctl function
	// This function allows us to omit the input/output buffer sizes, as it's: a) inconvenient, and 
	// b) easy to mess up. 
	template<typename A = nullptr_t, typename B = nullptr_t> requires (NullablePtr<A> && NullablePtr<B>)
	bool ioctl(unsigned long ioctlCode, 
		A inputBuffer,
		B outputBuffer) {

		return ioctlRaw(ioctlCode,
			inputBuffer,
			std::is_null_pointer_v<A> ? 0 : sizeof(std::remove_pointer_t<A>),
			outputBuffer,
			std::is_null_pointer_v<B> ? 0 : sizeof(std::remove_pointer_t<B>)
		);
	}

	// A "raw" C-like wrapper for DeviceIoControl to reduce boilerplate.
	bool ioctlRaw(unsigned long ioctlCode, void* inputBuffer, unsigned long inputSize, void* outputBuffer, unsigned long outputSize) {
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