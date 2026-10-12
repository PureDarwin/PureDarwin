// pdkext: manages kexts outside the boot KC, for now loading an auxiliary KC after boot
#include <mach/mach.h>
#include <mach/host_priv.h>
#include <mach/host_special_ports.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>

// where the auxiliary KC lives, as on macOS
#define AUX_KC_PATH "/Library/KernelCollections/AuxiliaryKernelExtensions.kc"

// xnu's kext_request_keys.h
#define kKextRequestPredicateKey "Kext Request Predicate"
#define kKextRequestArgumentsKey "Kext Request Arguments"
#define kKextRequestPredicateLoadFileSetKC "loadfilesetkc"
#define kKextRequestArgumentAuxKCFilename "AuxKCName"
#define kKextRequestPredicateLoadFromKC "LoadFromKC"
#define kKextRequestArgumentBundleIdentifierKey "CFBundleIdentifier"

// bundle ids started after the aux KC loads, one per line
#define BOOT_LOAD_LIST "/etc/pdkext.load"

extern kern_return_t kext_request(host_priv_t host_priv, uint32_t user_log_flags,
    vm_offset_t request_data, mach_msg_type_number_t request_dataCnt,
    vm_offset_t *response_data, mach_msg_type_number_t *response_dataCnt,
    vm_offset_t *log_data, mach_msg_type_number_t *log_dataCnt, kern_return_t *op_result);

static void usage(void)
{
	fprintf(stderr, "usage: pdkext load-aux <collection>\n       pdkext load <bundle-id>\n       pdkext boot\n");
	exit(2);
}

// the path goes into an XML plist, so escape what XML needs escaped
static void put_escaped(char *out, size_t cap, const char *s)
{
	size_t n = strlen(out);

	for (; *s && n + 7 < cap; s++) {
		const char *rep = *s == '&' ? "&amp;" : *s == '<' ? "&lt;" : *s == '>' ? "&gt;" : NULL;
		if (rep) {
			strcpy(out + n, rep);
			n += strlen(rep);
		} else {
			out[n++] = *s;
		}
	}
	out[n] = 0;
}

// one request with one string argument, the kernel's log and result printed
static int send_request(const char *predicate, const char *key, const char *value, const char *what)
{
	char request[4096];
	mach_port_t host_priv = MACH_PORT_NULL;
	vm_offset_t response = 0, log = 0;
	mach_msg_type_number_t response_len = 0, log_len = 0;
	kern_return_t op_result = KERN_FAILURE, kr;

	snprintf(request, sizeof(request),
	    "<plist version=\"1.0\"><dict><key>%s</key><string>%s</string><key>%s</key><dict><key>%s</key><string>",
	    kKextRequestPredicateKey, predicate, kKextRequestArgumentsKey, key);
	put_escaped(request, sizeof(request), value);
	strncat(request, "</string></dict></dict></plist>", sizeof(request) - strlen(request) - 1);

	kr = host_get_host_priv_port(mach_host_self(), &host_priv);
	if (kr != KERN_SUCCESS || host_priv == MACH_PORT_NULL) {
		fprintf(stderr, "pdkext: no host privilege port (%s), run as root\n", mach_error_string(kr));
		return 1;
	}

	kr = kext_request(host_priv, 0, (vm_offset_t)request, (mach_msg_type_number_t)strlen(request) + 1,
	    &response, &response_len, &log, &log_len, &op_result);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "pdkext: kext_request failed: %s\n", mach_error_string(kr));
		return 1;
	}
	if (log && log_len)
		fprintf(stderr, "%.*s\n", (int)log_len, (const char *)log);
	if (response && response_len)
		printf("%.*s\n", (int)response_len, (const char *)response);
	if (response)
		vm_deallocate(mach_task_self(), response, response_len);
	if (log)
		vm_deallocate(mach_task_self(), log, log_len);

	if (op_result != KERN_SUCCESS) {
		fprintf(stderr, "pdkext: the kernel refused %s: 0x%x\n", what, op_result);
		return 1;
	}
	printf("pdkext: loaded %s\n", what);
	return 0;
}

static int load_aux(const char *path)
{
	if (path[0] != '/') {
		fprintf(stderr, "pdkext: the kernel opens the collection itself, give an absolute path\n");
		return 1;
	}
	if (access(path, R_OK) != 0) {
		perror(path);
		return 1;
	}
	return send_request(kKextRequestPredicateLoadFileSetKC, kKextRequestArgumentAuxKCFilename, path, path);
}

// starts a kext that a loaded collection already registered
static int load_kext(const char *bundle_id)
{
	return send_request(kKextRequestPredicateLoadFromKC, kKextRequestArgumentBundleIdentifierKey, bundle_id, bundle_id);
}

// at boot: load the aux KC if there is one, unless the boot args ask for a start without extensions
static int boot_load(void)
{
	char args[1024] = "";
	size_t len = sizeof(args) - 1;

	if (sysctlbyname("kern.bootargs", args, &len, NULL, 0) == 0 && strstr(args, "-noaux")) {
		printf("pdkext: -noaux, starting without the auxiliary KC\n");
		return 0;
	}
	if (access(AUX_KC_PATH, R_OK) != 0)
		return 0;
	if (load_aux(AUX_KC_PATH) != 0)
		return 1;

	FILE *list = fopen(BOOT_LOAD_LIST, "r");
	char line[256];
	int failed = 0;

	if (!list)
		return 0;
	while (fgets(line, sizeof(line), list)) {
		line[strcspn(line, " \t\r\n#")] = 0;
		if (line[0])
			failed |= load_kext(line);
	}
	fclose(list);
	return failed;
}

int main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "boot"))
		return boot_load();
	if (argc == 3 && !strcmp(argv[1], "load"))
		return load_kext(argv[2]);
	if (argc == 3 && !strcmp(argv[1], "load-aux"))
		return load_aux(argv[2]);
	usage();
	return 2;
}
