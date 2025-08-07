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
        }
    }

    return status;
}
