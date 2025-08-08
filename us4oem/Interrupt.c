#include "interrupt.h"
#include "interrupt.tmh"

BOOLEAN Us4OemInterruptIsr(IN WDFINTERRUPT Interrupt, IN ULONG MessageID) {
	UNREFERENCED_PARAMETER(MessageID);

	// The IRQ basically functions as a signal for user-space to perform an action,
	// so we don't need to do anything here other than queueing a DPC.

	WdfInterruptQueueDpcForIsr(Interrupt);

	return TRUE; // Indicate that the interrupt was handled
}

VOID Us4OemInterruptDpc(IN WDFINTERRUPT Interrupt, IN WDFOBJECT AssociatedObject) {
	UNREFERENCED_PARAMETER(AssociatedObject);
	
	// TODO: Notify user-mode application about the interrupt, for now just increment 
	// the IRQ count in the device context, as we don't have a sync mechanism yet.
	PUS4OEM_CONTEXT deviceContext = us4oemGetContext(WdfInterruptGetDevice(Interrupt));
	deviceContext->Stats.irq_count++;
}