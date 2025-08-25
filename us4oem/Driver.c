#include "driver.h"
#include "driver.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, us4oemEvtDeviceAdd)
#pragma alloc_text (PAGE, us4oemEvtDriverContextCleanup)
#pragma alloc_text (PAGE, us4oemEvtDevicePrepareHardware)
#pragma alloc_text (PAGE, us4oemEvtDeviceReleaseHardware)
#endif

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    //
    // Initialize WPP Tracing
    //
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = us4oemEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config,
                           us4oemEvtDeviceAdd
                           );

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &attributes,
                             &config,
                             WDF_NO_HANDLE
                             );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

NTSTATUS
us4oemEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    status = us4oemCreateDevice(DeviceInit);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

VOID
us4oemEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}

NTSTATUS
us4oemEvtDevicePrepareHardware(
    WDFDEVICE      Device,
    WDFCMRESLIST   Resources,
    WDFCMRESLIST   ResourcesTranslated
) {
    UNREFERENCED_PARAMETER(Resources);

    PAGED_CODE();

    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
    PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

    // We expect to find three resources: IRQ, PCIDMA memory (512 KiB) @ BAR 0 and us4oem memory (64 MiB) @ BAR 4

    for (ULONG i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {

        descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);

        if (!descriptor) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "%!FUNC! WdfCmResourceListGetDescriptor failed");
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

        PBAR_INFO bar = NULL;
        WDF_INTERRUPT_CONFIG interruptConfig;

        switch (descriptor->Type) {
            case CmResourceTypeMemory:
                if (descriptor->u.Memory.Length == PCIDMA_REGION_LENGTH) {
                    bar = &deviceContext->BarPciDma;
                }
                else if (descriptor->u.Memory.Length == US4OEM_REGION_LENGTH) {
                    bar = &deviceContext->BarUs4Oem;
                }
                else {
                    break;
                }

                // Save physical address and length
                bar->BaseAddr = descriptor->u.Memory.Start;
                bar->Length = descriptor->u.Memory.Length;

				// Map the memory to the device context
                bar->MappedAddress = MmMapIoSpace(
                    bar->BaseAddr,
                    bar->Length,
                    MmNonCached
				);

                bar = NULL;
					
                break;
            case CmResourceTypeInterrupt:
                WDF_INTERRUPT_CONFIG_INIT(&interruptConfig, Us4OemInterruptIsr, Us4OemInterruptDpc);

                interruptConfig.InterruptTranslated = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
                interruptConfig.InterruptRaw = WdfCmResourceListGetDescriptor(Resources, i);

                NTSTATUS status = WdfInterruptCreate(
                    Device,
                    &interruptConfig,
                    WDF_NO_OBJECT_ATTRIBUTES,
                    &deviceContext->Interrupt);
                if (!NT_SUCCESS(status))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                        "WdfInterruptCreate failed. status: %!STATUS!", status);
                    return STATUS_DEVICE_CONFIGURATION_ERROR;
                }

                break;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
us4oemEvtDeviceReleaseHardware(
    IN  WDFDEVICE    Device,
    IN  WDFCMRESLIST ResourcesTranslated
) {
	UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

	// Unmap every mapped BAR
	PUS4OEM_CONTEXT deviceContext = us4oemGetContext(Device);

    if (deviceContext->BarPciDma.MappedAddress) {
        MmUnmapIoSpace(deviceContext->BarPciDma.MappedAddress, deviceContext->BarPciDma.Length);
    }

    if (deviceContext->BarUs4Oem.MappedAddress) {
        MmUnmapIoSpace(deviceContext->BarUs4Oem.MappedAddress, deviceContext->BarUs4Oem.Length);
    }

	// Deallocate all DMA buffers
    LINKED_LIST_FOR_EACH(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers, commonBuffer) {
        if (commonBuffer->Item != NULL) {
            WdfObjectDelete(*commonBuffer->Item);
            deviceContext->Stats.dma_contig_free_count++;
        }
    }
    LINKED_LIST_FOR_EACH(MEMORY_ALLOCATION, deviceContext->DmaScatterGatherMemory, commonBuffer) {
        if (commonBuffer->Item != NULL) {
            WdfObjectDelete(commonBuffer->Item->transaction);
            MmUnlockPages(commonBuffer->Item->mdl);
            WdfObjectDelete(commonBuffer->Item->memory);
            IoFreeMdl(commonBuffer->Item->mdl);
            deviceContext->Stats.dma_sg_free_count++;
        }
	}

    deviceContext->Stats.dma_sg_alloc_count = 0;
    deviceContext->Stats.dma_contig_alloc_count = 0;

    LINKED_LIST_CLEAR(WDFCOMMONBUFFER, deviceContext->DmaContiguousBuffers);
	LINKED_LIST_CLEAR(MEMORY_ALLOCATION, deviceContext->DmaScatterGatherMemory);

    // Clear the pending request
    if (deviceContext->PendingRequest) {
        WdfRequestComplete(deviceContext->PendingRequest, STATUS_DEVICE_REMOVED);
        deviceContext->PendingRequest = NULL;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Device hardware released");

    return STATUS_SUCCESS;
}