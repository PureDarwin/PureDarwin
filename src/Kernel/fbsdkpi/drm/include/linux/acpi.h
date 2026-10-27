// SPDX-License-Identifier: MIT
// linux ACPI interface for drm drivers, with the ACPICA names drm-kmod's amdgpu uses
// tables and methods are reached through pd_drm_acpi.c on the xnu side
#ifndef _PD_DRM_LINUX_ACPI_H_
#define _PD_DRM_LINUX_ACPI_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/uuid.h>
#include <linux/notifier.h>

typedef void *acpi_handle;
typedef uint32_t acpi_status;
typedef char *acpi_string;
typedef uint32_t acpi_object_type;
typedef uint64_t acpi_size;
typedef uint64_t acpi_integer;

#define AE_OK 0x0000
#define AE_ERROR 0x0001
#define AE_NO_MEMORY 0x0004
#define AE_NOT_FOUND 0x0005
#define AE_NOT_EXIST 0x0006
#define AE_BAD_PARAMETER 0x1001
#define ACPI_FAILURE(s) ((s) != AE_OK)
#define ACPI_SUCCESS(s) ((s) == AE_OK)

#define ACPI_TYPE_ANY 0
#define ACPI_TYPE_INTEGER 1
#define ACPI_TYPE_STRING 2
#define ACPI_TYPE_BUFFER 3
#define ACPI_TYPE_PACKAGE 4

#define ACPI_ALLOCATE_BUFFER ((acpi_size)-1)
#define ACPI_FULL_PATHNAME 0
#define ACPI_ID_LEN 16
#define ACPI_NAMESEG_SIZE 4
#define ACPI_FADT_LOW_POWER_S0 (1 << 21)

#define ACPI_VIDEO_CLASS "video"
#define ACPI_AC_CLASS "ac_adapter"
#define ACPI_VIDEO_NOTIFY_PROBE 0x81
#define ACPI_VIDEO_DISPLAY_LCD 4

#define ACPI_HARDWARE 0x00000002
#define ACPI_MODULE_NAME(name)
#define ACPI_STATUS(s) (s)
#define return_ACPI_STATUS(s) return (s)

// both spellings name the same storage, amdgpu uses ACPICA's and nouveau linux's
union acpi_object {
	acpi_object_type Type;
	acpi_object_type type;
	struct {
		acpi_object_type Type;
		acpi_integer Value;
	} Integer;
	struct {
		acpi_object_type type;
		acpi_integer value;
	} integer;
	struct {
		acpi_object_type Type;
		uint32_t Length;
		char *Pointer;
	} String;
	struct {
		acpi_object_type type;
		uint32_t length;
		char *pointer;
	} string;
	struct {
		acpi_object_type Type;
		uint32_t Length;
		uint8_t *Pointer;
	} Buffer;
	struct {
		acpi_object_type type;
		uint32_t length;
		uint8_t *pointer;
	} buffer;
	struct {
		acpi_object_type Type;
		uint32_t Count;
		union acpi_object *Elements;
	} Package;
	struct {
		acpi_object_type type;
		uint32_t count;
		union acpi_object *elements;
	} package;
};

struct acpi_object_list {
	union { uint32_t Count; uint32_t count; };
	union { union acpi_object *Pointer; union acpi_object *pointer; };
};

struct acpi_buffer {
	union { acpi_size Length; acpi_size length; };
	union { void *Pointer; void *pointer; };
};

struct acpi_table_header {
	char Signature[ACPI_NAMESEG_SIZE];
	uint32_t Length;
	uint8_t Revision;
	uint8_t Checksum;
	char OemId[6];
	char OemTableId[8];
	uint32_t OemRevision;
	char AslCompilerId[ACPI_NAMESEG_SIZE];
	uint32_t AslCompilerRevision;
} __packed;

// as in FreeBSD, an acpi device is a newbus device and the glue keeps its handle
#define acpi_device _device
struct _device;

struct acpi_bus_event {
	char device_class[20];
	char bus_id[15];
	uint32_t type;
	uint32_t data;
};

#define ACPI_FREE(p) kfree(p)
#define ACPI_HANDLE(dev) pd_acpi_device_handle(dev)
#define ACPI_COMPANION(dev) ((struct acpi_device *)NULL)
#define acpi_device_handle(adev) pd_acpi_dev_handle(adev)
#define acpi_handle_debug(handle, fmt, ...)

acpi_handle pd_acpi_device_handle(struct device *dev);
acpi_handle pd_acpi_dev_handle(struct acpi_device *adev);
acpi_status acpi_get_table(const char *signature, uint32_t instance,
    struct acpi_table_header **table);
void acpi_put_table(struct acpi_table_header *table);
acpi_status acpi_get_handle(acpi_handle parent, const char *pathname,
    acpi_handle *ret);
acpi_status acpi_get_name(acpi_handle handle, uint32_t name_type,
    struct acpi_buffer *ret);
acpi_status acpi_evaluate_object(acpi_handle handle, const char *pathname,
    struct acpi_object_list *params, struct acpi_buffer *ret);
acpi_status acpi_evaluate_integer(acpi_handle handle, const char *pathname,
    struct acpi_object_list *params, unsigned long long *ret);
union acpi_object *acpi_evaluate_dsm_typed(acpi_handle handle,
    const guid_t *guid, uint64_t rev, uint64_t func, union acpi_object *argv4,
    acpi_object_type type);
const char *acpi_format_exception(acpi_status status);

static inline union acpi_object *
acpi_evaluate_dsm(acpi_handle handle, const guid_t *guid, uint64_t rev,
    uint64_t func, union acpi_object *argv4)
{
	return acpi_evaluate_dsm_typed(handle, guid, rev, func, argv4, ACPI_TYPE_ANY);
}

// function 0 of a _DSM returns a bitmap of the functions it implements
static inline bool
acpi_check_dsm(acpi_handle handle, const guid_t *guid, uint64_t rev, uint64_t funcs)
{
	union acpi_object *obj;
	uint64_t mask = 0;
	uint32_t i;

	if (funcs == 0)
		return false;
	obj = acpi_evaluate_dsm_typed(handle, guid, rev, 0, NULL, ACPI_TYPE_BUFFER);
	if (obj == NULL)
		return false;
	for (i = 0; i < obj->Buffer.Length && i < 8; i++)
		mask |= (uint64_t)obj->Buffer.Pointer[i] << (i * 8);
	ACPI_FREE(obj);
	return (mask & 1) && (mask & funcs) == funcs;
}

struct acpi_device *acpi_dev_get_first_match_dev(const char *hid,
    const char *uid, int64_t hrv);
void acpi_dev_put(struct acpi_device *adev);
int register_acpi_notifier(struct notifier_block *nb);
int unregister_acpi_notifier(struct notifier_block *nb);
bool acpi_amd_wbrf_supported_consumer(struct device *dev);

// ACPICA's spelling of the FADT, only its flags are read
struct pd_acpi_fadt {
	uint32_t Flags;
};
extern struct pd_acpi_fadt AcpiGbl_FADT;

#endif
