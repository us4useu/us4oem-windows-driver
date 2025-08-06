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
    PUS4OEM_CONTEXT deviceContext;
    WDFDEVICE device;
    NTSTATUS status;

    PAGED_CODE();

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, US4OEM_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) {
        deviceContext = us4oemGetContext(device);

        deviceContext->PrivateDeviceData = 0;

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
