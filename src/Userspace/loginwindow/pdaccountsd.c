// pdaccountsd - makes the account changes Settings and passwd ask for, after checking who asks
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// root may do anything, a person may change their own password and picture, the rest needs an administrator
// requests are served one at a time and a wrong password costs two seconds

#include "pdaccounts.h"
#include "pdaccounts_client.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#define PICTURES "/Library/User Pictures"
#define ACCOUNT_PICTURES PICTURES "/Accounts"
#define MIN_PASSWORD 4

typedef struct {
	int fd;
	uid_t uid;
	char name[256];
	pdac_message *m;
} request;

static void
reply(request *r, int ok, const char *message)
{
	pdac_item items[2] = { { "status", ok ? "ok" : "error", 0 }, { "message", message ? message : "", 0 } };

	pdac_write(r->fd, items, 2);
}

static int
fail(request *r, const char *message)
{
	reply(r, 0, message);
	return -1;
}

static int
is_admin(const char *name)
{
	return name[0] != '\0' && pda_group_has_member("admin", name);
}

static int
password_ok(const char *name, const char *password)
{
	CFMutableDictionaryRef rec = name ? pda_copy_user(name) : NULL;
	int ok = rec != NULL && password != NULL && pda_check_password(rec, password);

	if (rec)
		CFRelease(rec);
	if (!ok)
		sleep(2);
	return ok;
}

// root, or an administrator's name and password sent along with the request
static int
authorized(request *r)
{
	const char *user = pdac_get(r->m, "auth_user"), *password = pdac_get(r->m, "auth_password");

	if (r->uid == 0)
		return 1;
	if (user == NULL || password == NULL)
		return 0;
	return is_admin(user) && password_ok(user, password);
}

static int
admin_count(void)
{
	CFMutableDictionaryRef g = pda_copy_group("admin");
	CFTypeRef v = g ? CFDictionaryGetValue(g, CFSTR("users")) : NULL;
	int n = 0;

	if (v != NULL && CFGetTypeID(v) == CFArrayGetTypeID()) {
		for (CFIndex i = 0; i < CFArrayGetCount(v); i++) {
			char name[256];
			CFMutableDictionaryRef rec;

			if (!CFStringGetCString(CFArrayGetValueAtIndex(v, i), name, sizeof(name), kCFStringEncodingUTF8))
				continue;
			if ((rec = pda_copy_user(name)) != NULL) {
				n++;
				CFRelease(rec);
			}
		}
	}
	if (g)
		CFRelease(g);
	return n;
}

// what Settings shows of everyone, nothing secret: names, ids, pictures and who administers
static int
op_list(request *r)
{
	CFArrayRef names = pda_copy_user_names();
	pdac_item items[PDAC_MAX_ITEMS];
	char *lines[PDAC_MAX_ITEMS];
	int n = 0;

	items[n++] = (pdac_item){ "status", "ok", 0 };
	for (CFIndex i = 0; i < CFArrayGetCount(names) && n < PDAC_MAX_ITEMS; i++) {
		char name[256], uid[32], real[512], picture[1024];
		CFMutableDictionaryRef rec;

		if (!CFStringGetCString(CFArrayGetValueAtIndex(names, i), name, sizeof(name), kCFStringEncodingUTF8) ||
		    (rec = pda_copy_user(name)) == NULL)
			continue;
		if (!pda_attr(rec, "uid", uid, sizeof(uid)))
			strcpy(uid, "0");
		if (!pda_attr(rec, "realname", real, sizeof(real)))
			strlcpy(real, name, sizeof(real));
		if (!pda_attr(rec, "picture", picture, sizeof(picture)))
			picture[0] = '\0';
		CFRelease(rec);
		if (asprintf(&lines[n], "%s%c%s%c%s%c%s%c%d", name, PDAC_FIELD_SEP, uid, PDAC_FIELD_SEP, real, PDAC_FIELD_SEP,
		             picture, PDAC_FIELD_SEP, is_admin(name)) < 0)
			break;
		items[n] = (pdac_item){ "user", lines[n], 0 };
		n++;
	}
	CFRelease(names);
	pdac_write(r->fd, items, n);
	for (int i = 1; i < n; i++)
		free(lines[i]);
	return 0;
}

static int
op_check(request *r)
{
	if (!authorized(r))
		return fail(r, "The administrator name or password is incorrect.");
	reply(r, 1, NULL);
	return 0;
}

static int
op_passwd(request *r)
{
	const char *user = pdac_get(r->m, "user"), *new = pdac_get(r->m, "new"), *old = pdac_get(r->m, "old");
	CFMutableDictionaryRef rec;
	int own = user != NULL && strcmp(user, r->name) == 0;

	if (user == NULL || new == NULL)
		return fail(r, "The request is missing a value.");
	if (strlen(new) < MIN_PASSWORD)
		return fail(r, "The new password is too short.");
	// a person changing their own needs the old one, anyone else needs an administrator
	if (own && old != NULL) {
		if (!password_ok(user, old))
			return fail(r, "The old password is incorrect.");
	} else if (!authorized(r)) {
		return fail(r, "The administrator name or password is incorrect.");
	}
	if ((rec = pda_copy_user(user)) == NULL)
		return fail(r, "There is no such user.");
	if (pda_set_password(rec, new) != 0 || pda_write_user(rec) != 0) {
		CFRelease(rec);
		return fail(r, "The password could not be saved.");
	}
	CFRelease(rec);
	fprintf(stderr, "pdaccountsd: uid %u changed %s's password\n", (unsigned)r->uid, user);
	reply(r, 1, NULL);
	return 0;
}

static int
op_add(request *r)
{
	const char *user = pdac_get(r->m, "user"), *real = pdac_get(r->m, "realname"), *password = pdac_get(r->m, "password");
	const char *admin = pdac_get(r->m, "admin");

	if (!authorized(r))
		return fail(r, "The administrator name or password is incorrect.");
	if (user == NULL || password == NULL)
		return fail(r, "The request is missing a value.");
	if (!pda_valid_name(user))
		return fail(r, "An account name uses letters, digits, '.', '_' and '-' only.");
	if (strlen(password) < MIN_PASSWORD)
		return fail(r, "The password is too short.");
	if (real != NULL && (strchr(real, ':') || strchr(real, '\n')))
		return fail(r, "A full name cannot hold ':' or line breaks.");
	if (pda_create_user(user, real, 0, NULL, password, admin != NULL && strcmp(admin, "1") == 0) != 0)
		return fail(r, errno == EEXIST ? "That account name is already taken." : "The account could not be created.");
	fprintf(stderr, "pdaccountsd: uid %u added %s\n", (unsigned)r->uid, user);
	reply(r, 1, NULL);
	return 0;
}

static void
remove_account_picture(const char *user)
{
	static const char *exts[] = { "png", "jpg", "tiff", NULL };
	char path[PATH_MAX];

	for (int i = 0; exts[i]; i++) {
		snprintf(path, sizeof(path), ACCOUNT_PICTURES "/%s.%s", user, exts[i]);
		unlink(path);
	}
}

static int
op_delete(request *r)
{
	const char *user = pdac_get(r->m, "user"), *home = pdac_get(r->m, "removehome");

	if (!authorized(r))
		return fail(r, "The administrator name or password is incorrect.");
	if (user == NULL)
		return fail(r, "The request is missing a value.");
	if (strcmp(user, r->name) == 0)
		return fail(r, "You can't delete the account you are logged in with.");
	if (is_admin(user) && admin_count() <= 1)
		return fail(r, "The last administrator can't be deleted.");
	if (pda_remove_user(user, home != NULL && strcmp(home, "1") == 0) != 0)
		return fail(r, "The account could not be deleted.");
	remove_account_picture(user);
	fprintf(stderr, "pdaccountsd: uid %u deleted %s\n", (unsigned)r->uid, user);
	reply(r, 1, NULL);
	return 0;
}

static int
op_admin(request *r)
{
	const char *user = pdac_get(r->m, "user"), *on = pdac_get(r->m, "on");
	CFMutableDictionaryRef rec;

	if (!authorized(r))
		return fail(r, "The administrator name or password is incorrect.");
	if (user == NULL || on == NULL || (rec = pda_copy_user(user)) == NULL)
		return fail(r, "There is no such user.");
	CFRelease(rec);
	if (strcmp(on, "1") == 0) {
		if (pda_group_add_member("admin", user) != 0)
			return fail(r, "The admin group could not be changed.");
	} else {
		if (is_admin(user) && admin_count() <= 1)
			return fail(r, "At least one administrator has to stay.");
		if (pda_group_remove_member("admin", user) != 0)
			return fail(r, "The admin group could not be changed.");
	}
	reply(r, 1, NULL);
	return 0;
}

// the extension a picture's own bytes call for, NULL when it is no picture we draw
static const char *
picture_type(const unsigned char *b, size_t len)
{
	if (len > 8 && memcmp(b, "\x89PNG\r\n\x1a\n", 8) == 0)
		return "png";
	if (len > 3 && b[0] == 0xff && b[1] == 0xd8 && b[2] == 0xff)
		return "jpg";
	if (len > 4 && (memcmp(b, "II*\0", 4) == 0 || memcmp(b, "MM\0*", 4) == 0))
		return "tiff";
	return NULL;
}

static int
op_picture(request *r)
{
	const char *user = pdac_get(r->m, "user"), *chosen = pdac_get(r->m, "picture"), *data, *ext;
	char path[PATH_MAX], real[PATH_MAX], tmp[PATH_MAX];
	CFMutableDictionaryRef rec;
	struct stat sb;
	size_t len = 0;
	int fd;

	if (user == NULL || (rec = pda_copy_user(user)) == NULL)
		return fail(r, "There is no such user.");
	if (strcmp(user, r->name) != 0 && !authorized(r)) {
		CFRelease(rec);
		return fail(r, "The administrator name or password is incorrect.");
	}
	data = pdac_get_bytes(r->m, "data", &len);
	if (chosen != NULL) {
		// a built-in picture is named by path, the daemon never reads a file it was pointed at
		if (realpath(chosen, real) == NULL || strncmp(real, PICTURES "/", strlen(PICTURES) + 1) != 0 ||
		    stat(real, &sb) != 0 || !S_ISREG(sb.st_mode)) {
			CFRelease(rec);
			return fail(r, "That is not one of the system's pictures.");
		}
		strlcpy(path, real, sizeof(path));
	} else if (data != NULL && (ext = picture_type((const unsigned char *)data, len)) != NULL) {
		// a picture of the person's own comes as bytes and is kept where loginwindow can read it
		mkdir(PICTURES, 0755);
		mkdir(ACCOUNT_PICTURES, 0755);
		remove_account_picture(user);
		snprintf(path, sizeof(path), ACCOUNT_PICTURES "/%s.%s", user, ext);
		snprintf(tmp, sizeof(tmp), "%s.tmp", path);
		fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0644);
		if (fd < 0 || write(fd, data, len) != (ssize_t)len || close(fd) != 0 || rename(tmp, path) != 0) {
			unlink(tmp);
			CFRelease(rec);
			return fail(r, "The picture could not be saved.");
		}
	} else {
		CFRelease(rec);
		return fail(r, "Pictures can be PNG, JPEG or TIFF files.");
	}
	pda_set_attr(rec, "picture", path);
	if (pda_write_user(rec) != 0) {
		CFRelease(rec);
		return fail(r, "The picture could not be saved.");
	}
	CFRelease(rec);
	reply(r, 1, NULL);
	return 0;
}

static void
serve(int fd)
{
	pdac_message m;
	request r = { .fd = fd, .m = &m };
	gid_t gid;
	CFMutableDictionaryRef self;
	struct timeval tv = { 10, 0 };
	const char *op;

	// a client that stalls can't hold up everyone else
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (getpeereid(fd, &r.uid, &gid) != 0 || pdac_read(fd, &m) != 0)
		return;
	if ((self = pda_copy_user_by_uid(r.uid)) != NULL) {
		pda_attr(self, "name", r.name, sizeof(r.name));
		CFRelease(self);
	}
	op = pdac_get(&m, "op");
	if (op == NULL)
		fail(&r, "The request names no operation.");
	else if (strcmp(op, "list") == 0)
		op_list(&r);
	else if (strcmp(op, "check") == 0)
		op_check(&r);
	else if (strcmp(op, "passwd") == 0)
		op_passwd(&r);
	else if (strcmp(op, "add") == 0)
		op_add(&r);
	else if (strcmp(op, "delete") == 0)
		op_delete(&r);
	else if (strcmp(op, "admin") == 0)
		op_admin(&r);
	else if (strcmp(op, "picture") == 0)
		op_picture(&r);
	else
		fail(&r, "The account service doesn't know that operation.");
	pdac_free(&m);
}

int
main(void)
{
	struct sockaddr_un sun = { .sun_family = AF_UNIX };
	int s;

	signal(SIGPIPE, SIG_IGN);
	umask(022);
	strlcpy(sun.sun_path, PDAC_SOCKET, sizeof(sun.sun_path));
	unlink(PDAC_SOCKET);
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0 || bind(s, (struct sockaddr *)&sun, sizeof(sun)) != 0 ||
	    chmod(PDAC_SOCKET, 0666) != 0 || listen(s, 8) != 0) {
		fprintf(stderr, "pdaccountsd: can't listen on %s, errno %d\n", PDAC_SOCKET, errno);
		return 1;
	}
	fprintf(stderr, "pdaccountsd: listening on %s\n", PDAC_SOCKET);
	for (;;) {
		int fd = accept(s, NULL, NULL);

		if (fd < 0)
			continue;
		serve(fd);
		close(fd);
	}
}
