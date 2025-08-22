#include "driver.h"
#include "us4oem.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, us4oemCreateDevice)
#endif

NTSTATUS
us4oemCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
	WDF_OBJECT_ATTRIBUTES fileAttributes;
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    PUS4OEM_CONTEXT deviceContext;
    WDFDEVICE device;
    NTSTATUS status;

    PAGED_CODE();

    // Create file
    WDF_FILEOBJECT_CONFIG_INIT(
        &fileConfig,
        us4oemEvtDeviceFileCreate,
        us4oemEvtFileClose,
        WDF_NO_EVENT_CALLBACK
        );

    WDF_OBJECT_ATTRIBUTES_INIT(&fileAttributes);

    WdfDeviceInitSetFileObjectConfig(
        DeviceInit,
        &fileConfig,
        &fileAttributes
    );

    // Create device
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, US4OEM_CONTEXT);

	// We want unbuffered I/O for this device (for performance)
    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoDirect);

	// Initialize PnP callbacks, as we need them to map HW resources
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
	pnpPowerCallbacks.EvtDevicePrepareHardware = us4oemEvtDevicePrepareHardware;
	pnpPowerCallbacks.EvtDeviceReleaseHardware = us4oemEvtDeviceReleaseHardware;

    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) {
        deviceContext = us4oemGetContext(device);

		// Initialize the device context
		RtlZeroMemory(deviceContext, sizeof(US4OEM_CONTEXT));

        status = WdfDeviceCreateDeviceInterface(
            device,
            &GUID_DEVINTERFACE_us4oem,
            NULL // ReferenceString
        );

        if (NT_SUCCESS(status)) {
            status = us4oemQueueInitialize(device);

            if (NT_SUCCESS(status)) {
                // Setup DMA
                WdfDeviceSetAlignmentRequirement(
                    device,
                    0xFFF // 4096-byte alignment for DMA transfers
                );

                WDF_DMA_ENABLER_CONFIG dmaConfig;
                WDF_DMA_ENABLER_CONFIG_INIT(
                    &dmaConfig,
                    WdfDmaProfileScatterGather64,
                    0xFFFFFFFFFFFFFFFF // We don't really care about this anyway, but it must be set
                );
				dmaConfig.WdmDmaVersionOverride = 3; // We need more than 2 GiB for DMA transfers

                if (!&dmaConfig) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WDF_DMA_ENABLER_CONFIG_INIT failed");
                    return STATUS_INSUFFICIENT_RESOURCES;
				}

                status = WdfDmaEnablerCreate(
                    device,
                    &dmaConfig,
                    WDF_NO_OBJECT_ATTRIBUTES,
                    &deviceContext->DmaEnabler
                );
            }
        }
    }

    return status;
}