// pdaccounts.c - local accounts stored the macOS way, one dslocal plist per user
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "pdaccounts.h"

#include <CommonCrypto/CommonKeyDerivation.h>
#include <CommonCrypto/CommonRandom.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>

#define PDA_HASH_KEY "SALTED-SHA512-PBKDF2"
#define PDA_ENTROPY_LEN 128
#define PDA_SALT_LEN 32
#define PDA_ITERATIONS 40000

static int
name_ok(const char *name)
{
	// a record name becomes a file name, so no path tricks
	return name != NULL && name[0] != '\0' && name[0] != '.' && strchr(name, '/') == NULL && strlen(name) < 200;
}

static void
user_path(const char *name, char *buf, size_t len)
{
	snprintf(buf, len, PDA_USERS "/%s.plist", name);
}

static CFPropertyListRef
read_plist(const char *path, CFOptionFlags opts)
{
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, strlen(path), false);
	CFReadStreamRef stream = url ? CFReadStreamCreateWithFile(NULL, url) : NULL;
	CFPropertyListRef plist = NULL;

	if (stream != NULL && CFReadStreamOpen(stream)) {
		plist = CFPropertyListCreateWithStream(NULL, stream, 0, opts, NULL, NULL);
		CFReadStreamClose(stream);
	}
	if (stream)
		CFRelease(stream);
	if (url)
		CFRelease(url);
	return plist;
}

// whole-file replace through a temporary, so a crash never leaves half a record
static int
write_file(const char *path, const UInt8 *bytes, CFIndex len, mode_t mode)
{
	char tmp[1100];
	int fd;

	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (fd < 0)
		return -1;
	if (fchmod(fd, mode) != 0 || write(fd, bytes, (size_t)len) != len || fsync(fd) != 0) {
		close(fd);
		unlink(tmp);
		return -1;
	}
	close(fd);
	return rename(tmp, path);
}

static int
write_plist(const char *path, CFPropertyListRef plist, mode_t mode)
{
	CFDataRef data = CFPropertyListCreateData(NULL, plist, kCFPropertyListXMLFormat_v1_0, 0, NULL);
	int ret;

	if (data == NULL)
		return -1;
	ret = write_file(path, CFDataGetBytePtr(data), CFDataGetLength(data), mode);
	CFRelease(data);
	return ret;
}

static void
make_dirs(void)
{
	mkdir("/var/db/dslocal", 0755);
	mkdir("/var/db/dslocal/nodes", 0755);
	mkdir(PDA_NODE, 0755);
	mkdir(PDA_USERS, 0700);
	mkdir(PDA_GROUPS, 0755);
}

CFMutableDictionaryRef
pda_copy_user(const char *name)
{
	char path[1024];
	CFPropertyListRef plist;

	if (!name_ok(name))
		return NULL;
	user_path(name, path, sizeof(path));
	plist = read_plist(path, kCFPropertyListMutableContainersAndLeaves);
	if (plist != NULL && CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
		CFRelease(plist);
		return NULL;
	}
	return (CFMutableDictionaryRef)plist;
}

int
pda_attr(CFDictionaryRef rec, const char *key, char *buf, size_t len)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFTypeRef v = k ? CFDictionaryGetValue(rec, k) : NULL;

	if (k)
		CFRelease(k);
	if (v != NULL && CFGetTypeID(v) == CFArrayGetTypeID() && CFArrayGetCount(v) > 0)
		v = CFArrayGetValueAtIndex(v, 0);
	if (v == NULL || CFGetTypeID(v) != CFStringGetTypeID())
		return 0;
	return CFStringGetCString(v, buf, (CFIndex)len, kCFStringEncodingUTF8);
}

void
pda_set_attr(CFMutableDictionaryRef rec, const char *key, const char *value)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFStringRef v = CFStringCreateWithCString(NULL, value, kCFStringEncodingUTF8);
	CFArrayRef arr = v ? CFArrayCreate(NULL, (const void **)&v, 1, &kCFTypeArrayCallBacks) : NULL;

	if (k && arr)
		CFDictionarySetValue(rec, k, arr);
	if (arr)
		CFRelease(arr);
	if (v)
		CFRelease(v);
	if (k)
		CFRelease(k);
}

int
pda_write_user(CFDictionaryRef rec)
{
	char name[256], path[1024];

	if (!pda_attr(rec, "name", name, sizeof(name)) || !name_ok(name))
		return -1;
	make_dirs();
	user_path(name, path, sizeof(path));
	if (write_plist(path, rec, 0600) != 0)
		return -1;
	return pda_write_index();
}

int
pda_delete_user(const char *name)
{
	char path[1024];

	if (!name_ok(name))
		return -1;
	user_path(name, path, sizeof(path));
	if (unlink(path) != 0)
		return -1;
	return pda_write_index();
}

static CFComparisonResult
compare_names(const void *a, const void *b, void *ctx)
{
	(void)ctx;
	return CFStringCompare((CFStringRef)a, (CFStringRef)b, 0);
}

CFArrayRef
pda_copy_user_names(void)
{
	CFMutableArrayRef names = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	struct dirent *dp;
	DIR *dir = opendir(PDA_USERS);

	while (dir != NULL && (dp = readdir(dir)) != NULL) {
		size_t len = strlen(dp->d_name);
		CFStringRef name;

		if (dp->d_name[0] == '.' || len < 7 || strcmp(dp->d_name + len - 6, ".plist") != 0)
			continue;
		name = CFStringCreateWithBytes(NULL, (const UInt8 *)dp->d_name, (CFIndex)(len - 6), kCFStringEncodingUTF8, false);
		if (name) {
			CFArrayAppendValue(names, name);
			CFRelease(name);
		}
	}
	if (dir)
		closedir(dir);
	CFArraySortValues(names, CFRangeMake(0, CFArrayGetCount(names)), compare_names, NULL);
	return names;
}

uid_t
pda_next_uid(void)
{
	CFArrayRef names = pda_copy_user_names();
	uid_t next = PDA_FIRST_UID;

	for (CFIndex i = 0; i < CFArrayGetCount(names); i++) {
		char name[256], uid[32];
		CFMutableDictionaryRef rec;

		if (!CFStringGetCString(CFArrayGetValueAtIndex(names, i), name, sizeof(name), kCFStringEncodingUTF8))
			continue;
		rec = pda_copy_user(name);
		if (rec && pda_attr(rec, "uid", uid, sizeof(uid)) && (uid_t)strtol(uid, NULL, 10) >= next)
			next = (uid_t)strtol(uid, NULL, 10) + 1;
		if (rec)
			CFRelease(rec);
	}
	CFRelease(names);
	return next;
}

static CFDataRef
derive(const char *password, const UInt8 *salt, size_t saltlen, unsigned iterations)
{
	UInt8 out[PDA_ENTROPY_LEN];

	if (CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), salt, saltlen, kCCPRFHmacAlgSHA512,
	                         iterations, out, sizeof(out)) != kCCSuccess)
		return NULL;
	return CFDataCreate(NULL, out, sizeof(out));
}

int
pda_set_password(CFMutableDictionaryRef rec, const char *password)
{
	UInt8 salt[PDA_SALT_LEN];
	int iterations = PDA_ITERATIONS;
	CFDataRef entropy, saltData, blob;
	CFNumberRef iter;
	CFMutableDictionaryRef inner, outer;
	CFArrayRef shadow;

	if (CCRandomGenerateBytes(salt, sizeof(salt)) != kCCSuccess)
		return -1;
	entropy = derive(password, salt, sizeof(salt), (unsigned)iterations);
	if (entropy == NULL)
		return -1;
	saltData = CFDataCreate(NULL, salt, sizeof(salt));
	iter = CFNumberCreate(NULL, kCFNumberIntType, &iterations);
	inner = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(inner, CFSTR("entropy"), entropy);
	CFDictionarySetValue(inner, CFSTR("salt"), saltData);
	CFDictionarySetValue(inner, CFSTR("iterations"), iter);
	outer = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(outer, CFSTR(PDA_HASH_KEY), inner);

	// ShadowHashData is an array holding one binary plist, as on macOS
	blob = CFPropertyListCreateData(NULL, outer, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	shadow = blob ? CFArrayCreate(NULL, (const void **)&blob, 1, &kCFTypeArrayCallBacks) : NULL;
	if (shadow)
		CFDictionarySetValue(rec, CFSTR("ShadowHashData"), shadow);

	if (shadow)
		CFRelease(shadow);
	if (blob)
		CFRelease(blob);
	CFRelease(outer);
	CFRelease(inner);
	CFRelease(iter);
	CFRelease(saltData);
	CFRelease(entropy);
	return shadow ? 0 : -1;
}

int
pda_check_password(CFDictionaryRef rec, const char *password)
{
	CFTypeRef shadow = CFDictionaryGetValue(rec, CFSTR("ShadowHashData"));
	CFPropertyListRef hashes = NULL;
	CFDictionaryRef entry;
	CFDataRef entropy, salt, derived = NULL;
	CFNumberRef iter;
	int iterations = 0, ok = 0;

	if (shadow != NULL && CFGetTypeID(shadow) == CFArrayGetTypeID() && CFArrayGetCount(shadow) > 0)
		shadow = CFArrayGetValueAtIndex(shadow, 0);
	if (shadow == NULL || CFGetTypeID(shadow) != CFDataGetTypeID())
		return 0;
	hashes = CFPropertyListCreateWithData(NULL, shadow, kCFPropertyListImmutable, NULL, NULL);
	if (hashes == NULL || CFGetTypeID(hashes) != CFDictionaryGetTypeID())
		goto out;
	entry = CFDictionaryGetValue(hashes, CFSTR(PDA_HASH_KEY));
	if (entry == NULL || CFGetTypeID(entry) != CFDictionaryGetTypeID())
		goto out;
	entropy = CFDictionaryGetValue(entry, CFSTR("entropy"));
	salt = CFDictionaryGetValue(entry, CFSTR("salt"));
	iter = CFDictionaryGetValue(entry, CFSTR("iterations"));
	if (entropy == NULL || salt == NULL || iter == NULL || CFGetTypeID(entropy) != CFDataGetTypeID() ||
	    CFGetTypeID(salt) != CFDataGetTypeID() || CFGetTypeID(iter) != CFNumberGetTypeID() ||
	    !CFNumberGetValue(iter, kCFNumberIntType, &iterations) || iterations <= 0 ||
	    CFDataGetLength(entropy) != PDA_ENTROPY_LEN)
		goto out;
	derived = derive(password, CFDataGetBytePtr(salt), (size_t)CFDataGetLength(salt), (unsigned)iterations);
	if (derived != NULL) {
		// constant time, a wrong password takes as long as a right one
		const UInt8 *a = CFDataGetBytePtr(derived), *b = CFDataGetBytePtr(entropy);
		UInt8 diff = 0;

		for (CFIndex i = 0; i < PDA_ENTROPY_LEN; i++)
			diff |= a[i] ^ b[i];
		ok = diff == 0;
	}
out:
	if (derived)
		CFRelease(derived);
	if (hashes)
		CFRelease(hashes);
	return ok;
}

int
pda_write_index(void)
{
	CFArrayRef names = pda_copy_user_names();
	CFMutableDataRef out = CFDataCreateMutable(NULL, 0);
	int ret;

	for (CFIndex i = 0; i < CFArrayGetCount(names); i++) {
		char name[256], uid[32], gid[32], real[512], home[1024], shell[1024], line[3000];
		CFMutableDictionaryRef rec;

		if (!CFStringGetCString(CFArrayGetValueAtIndex(names, i), name, sizeof(name), kCFStringEncodingUTF8))
			continue;
		rec = pda_copy_user(name);
		if (rec == NULL)
			continue;
		if (pda_attr(rec, "uid", uid, sizeof(uid))) {
			if (!pda_attr(rec, "gid", gid, sizeof(gid)))
				snprintf(gid, sizeof(gid), "%d", PDA_STAFF_GID);
			if (!pda_attr(rec, "realname", real, sizeof(real)))
				real[0] = '\0';
			if (!pda_attr(rec, "home", home, sizeof(home)))
				snprintf(home, sizeof(home), "/Users/%s", name);
			if (!pda_attr(rec, "shell", shell, sizeof(shell)))
				snprintf(shell, sizeof(shell), "/bin/zsh");
			// ':' and newlines would split a passwd line, so they never reach the index
			for (char *c = real; *c; c++)
				if (*c == ':' || *c == '\n')
					*c = ' ';
			snprintf(line, sizeof(line), "%s:*:%s:%s:%s:%s:%s\n", name, uid, gid, real, home, shell);
			CFDataAppendBytes(out, (const UInt8 *)line, (CFIndex)strlen(line));
		}
		CFRelease(rec);
	}
	make_dirs();
	ret = write_file(PDA_INDEX, CFDataGetBytePtr(out), CFDataGetLength(out), 0644);
	CFRelease(out);
	CFRelease(names);
	return ret;
}

CFMutableDictionaryRef
pda_copy_user_by_uid(uid_t uid)
{
	CFArrayRef names = pda_copy_user_names();
	CFMutableDictionaryRef found = NULL;

	for (CFIndex i = 0; i < CFArrayGetCount(names) && found == NULL; i++) {
		char name[256], v[32];
		CFMutableDictionaryRef rec;

		if (!CFStringGetCString(CFArrayGetValueAtIndex(names, i), name, sizeof(name), kCFStringEncodingUTF8))
			continue;
		rec = pda_copy_user(name);
		if (rec && pda_attr(rec, "uid", v, sizeof(v)) && (uid_t)strtol(v, NULL, 10) == uid)
			found = rec;
		else if (rec)
			CFRelease(rec);
	}
	CFRelease(names);
	return found;
}

int
pda_valid_name(const char *name)
{
	size_t n = name ? strlen(name) : 0;

	if (n == 0 || n > 64 || name[0] == '-' || name[0] == '.')
		return 0;
	for (const char *c = name; *c; c++) {
		if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-' || *c == '.'))
			return 0;
	}
	return 1;
}

static void
group_path(const char *name, char *buf, size_t len)
{
	snprintf(buf, len, PDA_GROUPS "/%s.plist", name);
}

CFMutableDictionaryRef
pda_copy_group(const char *name)
{
	char path[1024];
	CFPropertyListRef plist;

	if (!name_ok(name))
		return NULL;
	group_path(name, path, sizeof(path));
	plist = read_plist(path, kCFPropertyListMutableContainersAndLeaves);
	if (plist != NULL && CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
		CFRelease(plist);
		return NULL;
	}
	return (CFMutableDictionaryRef)plist;
}

int
pda_write_group(CFDictionaryRef rec)
{
	char name[256], path[1024];

	if (!pda_attr(rec, "name", name, sizeof(name)) || !name_ok(name))
		return -1;
	make_dirs();
	group_path(name, path, sizeof(path));
	return write_plist(path, rec, 0644);
}

int
pda_delete_group(const char *name)
{
	char path[1024];

	if (!name_ok(name))
		return -1;
	group_path(name, path, sizeof(path));
	return unlink(path);
}

CFArrayRef
pda_copy_group_names(void)
{
	CFMutableArrayRef names = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	struct dirent *dp;
	DIR *dir = opendir(PDA_GROUPS);

	while (dir != NULL && (dp = readdir(dir)) != NULL) {
		size_t len = strlen(dp->d_name);
		CFStringRef name;

		if (dp->d_name[0] == '.' || len < 7 || strcmp(dp->d_name + len - 6, ".plist") != 0)
			continue;
		name = CFStringCreateWithBytes(NULL, (const UInt8 *)dp->d_name, (CFIndex)(len - 6), kCFStringEncodingUTF8, false);
		if (name) {
			CFArrayAppendValue(names, name);
			CFRelease(name);
		}
	}
	if (dir)
		closedir(dir);
	CFArraySortValues(names, CFRangeMake(0, CFArrayGetCount(names)), compare_names, NULL);
	return names;
}

gid_t
pda_next_gid(void)
{
	CFArrayRef names = pda_copy_group_names();
	gid_t next = PDA_FIRST_GID;

	for (CFIndex i = 0; i < CFArrayGetCount(names); i++) {
		char name[256], gid[32];
		CFMutableDictionaryRef rec;

		if (!CFStringGetCString(CFArrayGetValueAtIndex(names, i), name, sizeof(name), kCFStringEncodingUTF8))
			continue;
		rec = pda_copy_group(name);
		if (rec && pda_attr(rec, "gid", gid, sizeof(gid)) && (gid_t)strtol(gid, NULL, 10) >= next)
			next = (gid_t)strtol(gid, NULL, 10) + 1;
		if (rec)
			CFRelease(rec);
	}
	CFRelease(names);
	return next;
}

CFMutableDictionaryRef
pda_new_group(const char *name, gid_t gid)
{
	CFMutableDictionaryRef rec = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayRef none = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
	char v[32];

	snprintf(v, sizeof(v), "%u", (unsigned)gid);
	pda_set_attr(rec, "name", name);
	pda_set_attr(rec, "gid", v);
	pda_set_attr(rec, "passwd", "*");
	CFDictionarySetValue(rec, CFSTR("users"), none);
	CFRelease(none);
	return rec;
}

// the users attribute as a mutable array kept in the record
static CFMutableArrayRef
members(CFMutableDictionaryRef rec)
{
	CFTypeRef v = CFDictionaryGetValue(rec, CFSTR("users"));
	CFMutableArrayRef m;

	if (v != NULL && CFGetTypeID(v) == CFArrayGetTypeID()) {
		m = CFArrayCreateMutableCopy(NULL, 0, v);
	} else {
		m = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	CFDictionarySetValue(rec, CFSTR("users"), m);
	CFRelease(m);
	return m;
}

int
pda_group_add_member(const char *group, const char *user)
{
	CFMutableDictionaryRef rec = pda_copy_group(group);
	CFStringRef u = CFStringCreateWithCString(NULL, user, kCFStringEncodingUTF8);
	CFMutableArrayRef m;
	int ret;

	if (rec == NULL && strcmp(group, "admin") == 0)
		rec = pda_new_group("admin", PDA_ADMIN_GID);
	if (rec == NULL || u == NULL) {
		if (rec)
			CFRelease(rec);
		if (u)
			CFRelease(u);
		return -1;
	}
	m = members(rec);
	if (!CFArrayContainsValue(m, CFRangeMake(0, CFArrayGetCount(m)), u))
		CFArrayAppendValue(m, u);
	ret = pda_write_group(rec);
	CFRelease(u);
	CFRelease(rec);
	return ret;
}

int
pda_group_remove_member(const char *group, const char *user)
{
	CFMutableDictionaryRef rec = pda_copy_group(group);
	CFStringRef u = CFStringCreateWithCString(NULL, user, kCFStringEncodingUTF8);
	CFMutableArrayRef m;
	CFIndex at;
	int ret = 0;

	if (rec == NULL || u == NULL) {
		if (rec)
			CFRelease(rec);
		if (u)
			CFRelease(u);
		return -1;
	}
	m = members(rec);
	at = CFArrayGetFirstIndexOfValue(m, CFRangeMake(0, CFArrayGetCount(m)), u);
	if (at >= 0) {
		CFArrayRemoveValueAtIndex(m, at);
		ret = pda_write_group(rec);
	}
	CFRelease(u);
	CFRelease(rec);
	return ret;
}

int
pda_group_has_member(const char *group, const char *user)
{
	CFMutableDictionaryRef rec = pda_copy_group(group);
	CFStringRef u = CFStringCreateWithCString(NULL, user, kCFStringEncodingUTF8);
	CFTypeRef v = rec ? CFDictionaryGetValue(rec, CFSTR("users")) : NULL;
	int found = 0;

	if (v != NULL && u != NULL && CFGetTypeID(v) == CFArrayGetTypeID())
		found = CFArrayContainsValue(v, CFRangeMake(0, CFArrayGetCount(v)), u);
	if (u)
		CFRelease(u);
	if (rec)
		CFRelease(rec);
	return found;
}

void
pda_remove_from_all_groups(const char *user)
{
	CFArrayRef names = pda_copy_group_names();

	for (CFIndex i = 0; i < CFArrayGetCount(names); i++) {
		char name[256];

		if (CFStringGetCString(CFArrayGetValueAtIndex(names, i), name, sizeof(name), kCFStringEncodingUTF8) &&
		    pda_group_has_member(name, user))
			pda_group_remove_member(name, user);
	}
	CFRelease(names);
}

// the folders a new home gets, owned by the user
static int
make_home(const char *home, uid_t uid, gid_t gid)
{
	static const char *dirs[] = { "", "/Desktop", "/Documents", "/Downloads", "/Library", "/Library/Preferences", "/Pictures", NULL };
	char path[1100];

	mkdir("/Users", 0755);
	for (int i = 0; dirs[i] != NULL; i++) {
		snprintf(path, sizeof(path), "%s%s", home, dirs[i]);
		if (mkdir(path, i == 0 ? 0750 : 0700) != 0 && errno != EEXIST)
			return -1;
		if (chown(path, uid, gid) != 0)
			return -1;
	}
	return 0;
}

// physical walk, a symlink is removed and never followed out of the home
static void
remove_tree(const char *path)
{
	char *paths[] = { (char *)path, NULL };
	FTS *fts = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
	FTSENT *e;

	while (fts != NULL && (e = fts_read(fts)) != NULL) {
		if (e->fts_info == FTS_DP)
			rmdir(e->fts_path);
		else if (e->fts_info != FTS_D)
			unlink(e->fts_path);
	}
	if (fts)
		fts_close(fts);
}

int
pda_create_user(const char *name, const char *real, uid_t uid, const char *shell, const char *password, int admin)
{
	char home[1024], num[32], guid[37];
	CFMutableDictionaryRef rec;
	uuid_t u;
	int ret;

	if (!pda_valid_name(name)) {
		errno = EINVAL;
		return -1;
	}
	if ((rec = pda_copy_user(name)) != NULL || getpwnam(name) != NULL) {
		if (rec)
			CFRelease(rec);
		errno = EEXIST;
		return -1;
	}
	if (uid == 0)
		uid = pda_next_uid();
	if (uid < PDA_FIRST_UID) {
		errno = EINVAL;
		return -1;
	}
	rec = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	pda_set_attr(rec, "name", name);
	snprintf(num, sizeof(num), "%u", (unsigned)uid);
	pda_set_attr(rec, "uid", num);
	snprintf(num, sizeof(num), "%d", PDA_STAFF_GID);
	pda_set_attr(rec, "gid", num);
	pda_set_attr(rec, "realname", real && *real ? real : name);
	snprintf(home, sizeof(home), "/Users/%s", name);
	pda_set_attr(rec, "home", home);
	pda_set_attr(rec, "shell", shell ? shell : "/bin/zsh");
	uuid_generate_random(u);
	uuid_unparse_upper(u, guid);
	pda_set_attr(rec, "generateduid", guid);
	ret = pda_set_password(rec, password);
	if (ret == 0)
		ret = make_home(home, uid, PDA_STAFF_GID);
	if (ret == 0)
		ret = pda_write_user(rec);
	if (ret == 0 && admin)
		ret = pda_group_add_member("admin", name);
	CFRelease(rec);
	return ret;
}

int
pda_remove_user(const char *name, int remove_home)
{
	char home[1024];
	CFMutableDictionaryRef rec = pda_copy_user(name);

	if (rec == NULL) {
		errno = ENOENT;
		return -1;
	}
	if (!pda_attr(rec, "home", home, sizeof(home)))
		home[0] = '\0';
	CFRelease(rec);
	if (pda_delete_user(name) != 0)
		return -1;
	pda_remove_from_all_groups(name);
	// only a home under /Users is ever removed
	if (remove_home && strncmp(home, "/Users/", 7) == 0 && strstr(home, "..") == NULL && strchr(home + 7, '/') == NULL)
		remove_tree(home);
	return 0;
}
