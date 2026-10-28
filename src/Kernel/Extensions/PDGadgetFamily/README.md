PDGadgetFamily, the USB device framework


What this is

PDGadgetFamily lets a PureDarwin board act as a USB device. It holds the parts of USB device mode that do not depend on the controller hardware, and a separate backend kext drives the controller. The first backend is PDQcomUSB, the ChipIdea controller of the MSM8916 stick. A Tegra XUSB device controller for the TX1 would be the next one, and it would sit under the same framework without changes to it.

The framework kext provides three things. PDGadgetController is the base class a backend subclasses. PDGadgetPipe is a bulk pipe function that publishes a character device, used for /dev/adb (pdadbd talks to it) and /dev/pdfb (the display stream). PDGadgetACM is a CDC-ACM serial function that carries the console.


What the framework does

The framework owns the configuration and everything on endpoint 0. It builds the device, configuration, string and qualifier descriptors from the functions it carries. It answers the standard requests, which are GET_STATUS, CLEAR_FEATURE, SET_FEATURE, SET_ADDRESS, GET_DESCRIPTOR, GET_CONFIGURATION, SET_CONFIGURATION, GET_INTERFACE and SET_INTERFACE. It routes class requests to the function that owns the interface. It runs the setup, data and status stages of every control transfer, including the zero length packet a short answer on a packet boundary needs. On SET_CONFIGURATION it assigns endpoint numbers to the functions, asks the backend to allocate and enable each endpoint, and tells the functions they are configured. Functions queue transfers through buffer, busy, queue and queueAt, and the framework checks them before it calls the backend.

The functions are chosen by boot arguments in addStandardFunctions. The serial function is always there. The adb pipe is there unless pdusbadb=0. The pdfb stream is there when pdusbfb=1, or by default when the backend asks for it. pdusbtrace=N logs the first N control requests.


What a backend implements

A backend is an IOService subclass of PDGadgetController that matches its controller in the device tree. It implements these methods.

hwAllocEndpoint(ep, size) gives the endpoint a buffer of size bytes in ep->buf, together with whatever the hardware needs alongside it, such as transfer descriptors. It is called each time the host configures the device, so it should allocate once and keep the memory. The framework sets ep->in before the call, so the backend can lay out memory by direction. Backend state belongs in an array indexed by ep->slot, where slots 0 to 15 are the function endpoints, slot 16 is endpoint 0 out and slot 17 is endpoint 0 in.

hwEnableEndpoint(ep) and hwDisableEndpoint(ep) turn an endpoint on and off in the hardware with its number, direction, type and maximum packet size, which the framework fills in before the call. Disabling must also drop any transfer that is still queued.

hwQueue(ep, offset, length) starts one transfer of length bytes at offset into the endpoint's buffer and returns false if it cannot. Endpoint 0 uses it too, with ep0 buffers the backend handed to initGadget. hwQueueAlign returns the alignment the backend needs for offset. The ChipIdea backend needs 16 KB, the default is 1.

hwEp0Stall stalls endpoint 0 in both directions until the next setup packet. hwSetAddress(address) sets the device address so that it takes effect after the status stage of the SET_ADDRESS request, which is when USB requires it. hwClearHalt(num, in) clears a stall on a function endpoint and resets its data toggle.

rebootToMaskrom resets the board into whatever mode flashes it. The serial function calls it when the host sends a break. kick is optional and wakes the backend's thread when a function has queued work, which a polled backend uses to stop sleeping early.


What a backend calls

At start, after mapping the controller and allocating endpoint 0's buffer of kEp0Buffer bytes, the backend calls initGadget(buffer, product) with the product string to report. It then calls addStandardFunctions or addFunction and registerService. initGadget also reads pdusbtrace.

When the controller reports a bus reset, the backend clears its own address register and endpoint state, then calls busReset so the framework drops the configuration. When it reports the port speed, the backend calls setHighSpeed. When a setup packet arrives on endpoint 0, the backend reads the 8 bytes, drops anything still queued on endpoint 0, and calls setupReceived. When a transfer on endpoint 0 finishes, it calls ep0Done(in, actual). When a function endpoint transfer finishes, it calls transferDone(ep, actual) with the bytes actually moved, which the framework passes on to the function.

The backend also calls pollFunctions regularly from its own thread or interrupt work, because the functions move bytes between their character devices and the endpoints there. inFlight tells a polling backend whether an IN transfer is outstanding, so it can keep looking for completions instead of sleeping.


Notes from the ChipIdea backend

PDQcomUSB is polled, because the stick's interrupt routing for USB was not set up when it was written. A backend with working interrupts can call the same framework methods from its interrupt handler work loop instead. The ChipIdea core reads setup packets under a tripwire bit and acknowledges completions only for descriptors that are retired, because a completion bit stays set while a chained descriptor is still active. Stream mode stays disabled, since enabling it lowered throughput to about 10 MB/s on this core. Throughput with two 64 KB IN slots is about 32 MB/s.
