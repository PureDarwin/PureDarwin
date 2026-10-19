/*
 * Copyright 2009, Colin Günther. All Rights Reserved.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_HAIKU_MODULE_H_
#define _FBSD_COMPAT_SYS_HAIKU_MODULE_H_


#include <Drivers.h>
#include <KernelExport.h>

#include <kernel/lock.h>
#include <sys/method-ids.h>

#undef __unused
#define __unused

#undef ASSERT
	/* private/kernel/debug.h sets it */

typedef struct device *device_t;
typedef struct devclass *devclass_t;

typedef int (*device_method_signature_t)(device_t dev);

typedef int device_probe_t(device_t dev);
typedef int device_attach_t(device_t dev);
typedef int device_detach_t(device_t dev);
typedef int device_resume_t(device_t dev);
typedef int device_shutdown_t(device_t dev);
typedef int device_suspend_t(device_t dev);

typedef int bus_child_location_str_t(device_t dev __unused, device_t child,
	char *buf, size_t buflen);
typedef int bus_child_pnpinfo_str_t(device_t dev __unused, device_t child,
	char *buf, size_t buflen);
typedef void bus_hinted_child_t(device_t dev, const char *name, int unit);
typedef int bus_print_child_t(device_t dev, device_t child);
typedef int bus_read_ivar_t(device_t dev, device_t child __unused, int which,
    uintptr_t *result);

typedef int miibus_readreg_t(device_t dev, int phy, int reg);
typedef int miibus_writereg_t(device_t dev, int phy, int reg, int data);
typedef void miibus_statchg_t(device_t dev);
typedef void miibus_linkchg_t(device_t dev);
typedef void miibus_mediainit_t(device_t dev);


struct device_method {
	const char* name;
	const int32 id;
	device_method_signature_t method;
};

typedef struct device_method device_method_t;

#define DEVMETHOD(name, func) { #name, ID_##name, (device_method_signature_t)&func }
#define DEVMETHOD_END 		  { 0, 0 }


typedef struct {
	const char* name;
	device_method_t* methods;
	size_t size; /* softc size */
} driver_t;

#define DEFINE_CLASS_0(name, driver, methods, size) \
	driver_t driver = { #name, methods, size }

#define DRIVER_MODULE(name, busname, driver, evh, arg) \
	driver_t *DRIVER_MODULE_NAME(name, busname) = &(driver)

#define DRIVER_MODULE_ORDERED(name, busname, driver, evh, arg, order) \
	DRIVER_MODULE(name, busname, driver, evh, arg)

#define DRIVER_MODULE_NAME(name, busname) \
	__fbsd_ ## name ## _ ## busname


void __haiku_init_hardware(void);
status_t _fbsd_init_hardware();
status_t _fbsd_init_hardware_pci(driver_t* drivers[]);
status_t _fbsd_init_hardware_uhub(driver_t* drivers[]);
status_t _fbsd_init_drivers();
status_t _fbsd_uninit_drivers();

extern const char *gDriverName;
driver_t *__haiku_select_miibus_driver(device_t dev);
driver_t *__haiku_probe_drivers(device_t dev, driver_t *drivers[]);

status_t init_wlan_stack(void);
void uninit_wlan_stack(void);
status_t start_wlan(device_t);
status_t stop_wlan(device_t);
status_t wlan_control(void*, uint32, void*, size_t);
status_t wlan_close(void*);

/* we define the driver methods with HAIKU_FBSD_DRIVERS_GLUE to
 * force the rest of the stuff to be linked back with the driver.
 * While gcc 2.95 packs everything from the static library onto
 * the final binary, gcc 4.x rightfuly doesn't. */

#ifdef __PUREDARWIN__
/* on puredarwin an iokit class attaches each device, only the name is needed */
#define HAIKU_FBSD_DRIVERS_CORE_GLUE(publicname)						\
	const char *gDriverName = #publicname;
#else
#define HAIKU_FBSD_DRIVERS_CORE_GLUE(publicname)						\
	extern const char *gDeviceNameList[];								\
	extern device_hooks gDeviceHooks;									\
	const char *gDriverName = #publicname;								\
	int32 api_version = B_CUR_DRIVER_API_VERSION;						\
	status_t init_hardware()											\
	{																	\
		return _fbsd_init_hardware();									\
	}																	\
	status_t init_driver()												\
	{																	\
		return _fbsd_init_drivers();									\
	}																	\
	void uninit_driver()												\
	{																	\
		_fbsd_uninit_drivers();											\
	}																	\
	const char **publish_devices()										\
		{ return gDeviceNameList; }										\
	device_hooks *find_device(const char *name)							\
		{ return &gDeviceHooks; }
#endif

#define HAIKU_FBSD_DRIVERS_GLUE(publicname)								\
	HAIKU_FBSD_DRIVERS_CORE_GLUE(publicname)							\
	status_t init_wlan_stack(void)										\
		{ return B_OK; } 												\
	void uninit_wlan_stack(void) {}										\
	status_t start_wlan(device_t dev)									\
		{ return B_OK; }												\
	status_t stop_wlan(device_t dev)									\
		{ return B_OK; }												\
	status_t wlan_control(void *cookie, uint32 op, void *arg, 			\
		size_t length)													\
		{ return B_BAD_VALUE; }											\
	status_t wlan_close(void* cookie)									\
		{ return B_OK; }

#define HAIKU_FBSD_DRIVER_GLUE(publicname, name, busname)				\
	extern driver_t* DRIVER_MODULE_NAME(name, busname);					\
	void __haiku_init_hardware()										\
	{																	\
		driver_t *drivers[] = {											\
			DRIVER_MODULE_NAME(name, busname),							\
			NULL														\
		};																\
		_fbsd_init_hardware_pci(drivers);								\
	}																	\
	HAIKU_FBSD_DRIVERS_GLUE(publicname);

#define HAIKU_FBSD_WLAN_DRIVERS_GLUE(publicname)						\
	HAIKU_FBSD_DRIVERS_CORE_GLUE(publicname)

#define HAIKU_FBSD_WLAN_DRIVER_GLUE(publicname, name, busname)			\
	extern driver_t *DRIVER_MODULE_NAME(name, busname);					\
	void __haiku_init_hardware()										\
	{																	\
		driver_t *drivers[] = {											\
			DRIVER_MODULE_NAME(name, busname),							\
			NULL														\
		};																\
		_fbsd_init_hardware_pci(drivers);								\
	}																	\
	HAIKU_FBSD_WLAN_DRIVERS_GLUE(publicname);

#define HAIKU_FBSD_MII_DRIVER(name)								\
	extern driver_t *DRIVER_MODULE_NAME(name, miibus);			\
	driver_t *__haiku_select_miibus_driver(device_t dev)		\
	{															\
		driver_t *drivers[] = {									\
			DRIVER_MODULE_NAME(name, miibus),					\
			NULL												\
		};														\
		return __haiku_probe_drivers(dev, drivers);				\
	}

#define NO_HAIKU_FBSD_MII_DRIVER()								\
	driver_t *__haiku_select_miibus_driver(device_t dev)		\
	{															\
		return NULL;											\
	}

extern spinlock __haiku_intr_spinlock;
extern int __haiku_disable_interrupts(device_t dev);
extern void __haiku_reenable_interrupts(device_t dev);

#define HAIKU_CHECK_DISABLE_INTERRUPTS		__haiku_disable_interrupts
#define HAIKU_REENABLE_INTERRUPTS			__haiku_reenable_interrupts

#define NO_HAIKU_CHECK_DISABLE_INTERRUPTS()				\
	int HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev) {	\
		panic("should never be called.");				\
		return -1; \
	}

#define NO_HAIKU_REENABLE_INTERRUPTS() \
	void HAIKU_REENABLE_INTERRUPTS(device_t dev) {}

extern int __haiku_driver_requirements;

enum {
	FBSD_FAST_TASKQUEUE		= 1 << 0,
	FBSD_SWI_TASKQUEUE		= 1 << 1,
	FBSD_THREAD_TASKQUEUE	= 1 << 2,
	FBSD_WLAN_FEATURE		= 1 << 3,

	FBSD_WLAN				= FBSD_WLAN_FEATURE | FBSD_THREAD_TASKQUEUE,
	OBSD_WLAN				= FBSD_WLAN_FEATURE | FBSD_FAST_TASKQUEUE,

	FBSD_TASKQUEUES = FBSD_FAST_TASKQUEUE | FBSD_SWI_TASKQUEUE | FBSD_THREAD_TASKQUEUE,
};

#define HAIKU_DRIVER_REQUIREMENTS(flags) \
	int __haiku_driver_requirements = (flags)

#define HAIKU_DRIVER_REQUIRES(flag) (__haiku_driver_requirements & (flag))


/* #pragma mark - firmware loading */


/*
 * Only needed to be specified in the glue code of drivers which actually need
 * to load firmware. See iprowifi2100 for an example.
 */

extern const uint __haiku_firmware_version;

/* Use 0 if driver doesn't care about firmware version. */
#define HAIKU_FIRMWARE_VERSION(version) \
	const uint __haiku_firmware_version = (version)

extern const uint __haiku_firmware_parts_count;
extern const char* __haiku_firmware_name_map[][2];

/*
 * Provide a firmware name mapping as a multi-dimentional const char* array.
 *
 * HAIKU_FIRMWARE_NAME_MAP({
 *   {"name-used-by-driver", "actual-name-of-firmware-file-on-disk"},
 *   ...
 * });
 */
#define HAIKU_FIRMWARE_NAME_MAP(...) \
	const char* __haiku_firmware_name_map[][2] = __VA_ARGS__; \
	const uint __haiku_firmware_parts_count = B_COUNT_OF(__haiku_firmware_name_map)

#define NO_HAIKU_FIRMWARE_NAME_MAP() \
	const uint __haiku_firmware_parts_count = 0; \
	const char* __haiku_firmware_name_map[0][2] = {}


/* #pragma mark - synchronization */


#define HAIKU_INTR_REGISTER_STATE \
	cpu_status __haiku_cpu_state = 0

#define HAIKU_INTR_REGISTER_ENTER() do {		\
	__haiku_cpu_state = disable_interrupts();	\
	acquire_spinlock(&__haiku_intr_spinlock);	\
} while (0)

#define HAIKU_INTR_REGISTER_LEAVE() do {		\
	release_spinlock(&__haiku_intr_spinlock);	\
	restore_interrupts(__haiku_cpu_state);		\
} while (0)

#define HAIKU_PROTECT_INTR_REGISTER(x) do {		\
	HAIKU_INTR_REGISTER_STATE;					\
	HAIKU_INTR_REGISTER_ENTER();				\
	x;											\
	HAIKU_INTR_REGISTER_LEAVE();				\
} while (0)

#define nitems(_a)     (sizeof((_a)) / sizeof((_a)[0]))

#endif	/* _FBSD_COMPAT_SYS_HAIKU_MODULE_H_ */
