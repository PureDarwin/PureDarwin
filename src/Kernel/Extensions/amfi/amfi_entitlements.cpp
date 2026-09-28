// Entitlement queries answered from the process's own code signature:
// the embedded XML entitlements blob, validated against its special slot by xnu

#include <sys/types.h>
#include <sys/proc.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSUnserialize.h>

extern "C" {
struct cs_blob;
struct cs_blob *csproc_get_blob(struct proc *p);
int csblob_get_entitlements(struct cs_blob *csblob, void **out_start, size_t *out_length);

OSObject *pd_amfi_copy_entitlement(proc_t proc, const char *name);
bool pd_amfi_entitlement_is_true(proc_t proc, const char *name);
bool pd_amfi_entitlement_string_equals(proc_t proc, const char *name, const char *value);
}

// CS_GenericBlob header: big-endian magic and length, then the XML plist
static const size_t kBlobHeader = 8;

static OSDictionary *
pd_amfi_copy_entitlements(proc_t proc)
{
	struct cs_blob *blob = csproc_get_blob(proc);
	void *start = NULL;
	size_t length = 0;

	if (blob == NULL || csblob_get_entitlements(blob, &start, &length) != 0 ||
	    start == NULL || length <= kBlobHeader) {
		return NULL;
	}

	OSData *xml = OSData::withBytes((const char *)start + kBlobHeader,
	    (unsigned int)(length - kBlobHeader));
	if (xml == NULL) {
		return NULL;
	}
	const char nul = '\0';
	xml->appendBytes(&nul, 1);

	OSObject *parsed = OSUnserializeXML((const char *)xml->getBytesNoCopy(), xml->getLength());
	xml->release();
	OSDictionary *dict = OSDynamicCast(OSDictionary, parsed);
	if (dict == NULL) {
		OSSafeReleaseNULL(parsed);
	}
	return dict;
}

OSObject *
pd_amfi_copy_entitlement(proc_t proc, const char *name)
{
	OSDictionary *dict = pd_amfi_copy_entitlements(proc);
	OSObject *value = NULL;

	if (dict != NULL) {
		value = dict->getObject(name);
		if (value != NULL) {
			value->retain();
		}
		dict->release();
	}
	return value;
}

bool
pd_amfi_entitlement_is_true(proc_t proc, const char *name)
{
	OSObject *value = pd_amfi_copy_entitlement(proc, name);
	bool yes = (value == kOSBooleanTrue);

	OSSafeReleaseNULL(value);
	return yes;
}

bool
pd_amfi_entitlement_string_equals(proc_t proc, const char *name, const char *expected)
{
	OSObject *value = pd_amfi_copy_entitlement(proc, name);
	OSString *str = OSDynamicCast(OSString, value);
	bool yes = (str != NULL && expected != NULL && str->isEqualTo(expected));

	OSSafeReleaseNULL(value);
	return yes;
}
