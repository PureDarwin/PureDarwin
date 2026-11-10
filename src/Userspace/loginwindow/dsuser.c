// dsuser - add, change and remove local users kept in dslocal
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// an administrator is a member of group admin, gid 80, as on macOS

#include "pdaccounts.h"
#include "pdaccounts_client.h"

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <grp.h>

static void
usage(void)
{
	fprintf(stderr,
	    "usage: dsuser list\n"
	    "       dsuser add NAME [-n \"Real Name\"] [-u UID] [-s SHELL] [-p PASSWORD] [-a]\n"
	    "       dsuser passwd NAME [-p PASSWORD]\n"
	    "       dsuser picture NAME FILE\n"
	    "       dsuser delete NAME [-r]\n"
	    "       dsuser verify NAME\n"
	    "       dsuser group list\n"
	    "       dsuser group add NAME [-g GID]\n"
	    "       dsuser group delete NAME\n"
	    "       dsuser group addmember GROUP USER\n"
	    "       dsuser group removemember GROUP USER\n");
	exit(2);
}

static void
need_root(void)
{
	if (geteuid() != 0)
		errx(1, "local accounts can only be changed by root");
}

// asks twice on the terminal, a password never goes through argv unless -p was given
static char *
ask_password(char *buf, size_t len)
{
	char again[256];

	if (readpassphrase("New password: ", buf, len, RPP_REQUIRE_TTY) == NULL ||
	    readpassphrase("Retype new password: ", again, sizeof(again), RPP_REQUIRE_TTY) == NULL)
		errx(1, "no password given");
	if (strcmp(buf, again) != 0)
		errx(1, "the passwords don't match");
	memset(again, 0, sizeof(again));
	return buf;
}

static int
cmd_list(void)
{
	CFArrayRef names = pda_copy_user_names();

	for (CFIndex i = 0; i < CFArrayGetCount(names); i++) {
		char name[256], uid[32], real[512];
		CFMutableDictionaryRef rec;

		CFStringGetCString(CFArrayGetValueAtIndex(names, i), name, sizeof(name), kCFStringEncodingUTF8);
		rec = pda_copy_user(name);
		if (rec == NULL) {
			printf("%s\n", name);
			continue;
		}
		if (!pda_attr(rec, "uid", uid, sizeof(uid)))
			strcpy(uid, "?");
		if (!pda_attr(rec, "realname", real, sizeof(real)))
			real[0] = '\0';
		printf("%-16s %6s  %-5s  %s\n", name, uid, pda_group_has_member("admin", name) ? "admin" : "", real);
		CFRelease(rec);
	}
	CFRelease(names);
	return 0;
}

static int
cmd_add(int argc, char **argv)
{
	const char *name = argv[0], *real = NULL, *shell = "/bin/zsh", *password = NULL;
	char pw[256];
	uid_t uid = 0;
	int ch, admin = 0;

	need_root();
	optind = 1;
	while ((ch = getopt(argc, argv, "n:u:s:p:a")) != -1) {
		switch (ch) {
		case 'n': real = optarg; break;
		case 'u': uid = (uid_t)strtol(optarg, NULL, 10); break;
		case 's': shell = optarg; break;
		case 'p': password = optarg; break;
		case 'a': admin = 1; break;
		default: usage();
		}
	}
	if (!pda_valid_name(name))
		errx(1, "an account name uses letters, digits, '.', '_' and '-'");
	if (uid != 0 && uid < PDA_FIRST_UID)
		errx(1, "people get user IDs from %d up", PDA_FIRST_UID);
	if (password == NULL)
		password = ask_password(pw, sizeof(pw));
	if (pda_create_user(name, real, uid, shell, password, admin) != 0) {
		if (errno == EEXIST)
			errx(1, "%s already exists", name);
		err(1, "couldn't add %s", name);
	}
	memset(pw, 0, sizeof(pw));
	printf("%s added%s\n", name, admin ? " as an administrator" : "");
	return 0;
}

// a person changes their own password, pdaccountsd checks the old one
static int
passwd_through_daemon(const char *name)
{
	char old[256], pw[256], message[256];
	pdac_item items[4] = { { "op", "passwd", 0 }, { "user", name, 0 }, { "old", old, 0 }, { "new", pw, 0 } };
	int ret;

	if (readpassphrase("Old password: ", old, sizeof(old), RPP_REQUIRE_TTY) == NULL)
		errx(1, "no password given");
	ask_password(pw, sizeof(pw));
	ret = pdac_call(items, 4, message, sizeof(message));
	memset(old, 0, sizeof(old));
	memset(pw, 0, sizeof(pw));
	if (ret != 0)
		errx(1, "%s", message);
	printf("Password changed.\n");
	return 0;
}

static int
cmd_passwd(int argc, char **argv)
{
	const char *name = argv[0], *password = NULL;
	char pw[256];
	CFMutableDictionaryRef rec;
	int ch;

	optind = 1;
	while ((ch = getopt(argc, argv, "p:")) != -1) {
		if (ch == 'p')
			password = optarg;
		else
			usage();
	}
	if (geteuid() != 0)
		return passwd_through_daemon(name);
	rec = pda_copy_user(name);
	if (rec == NULL)
		errx(1, "no local user %s", name);
	if (password == NULL)
		password = ask_password(pw, sizeof(pw));
	if (pda_set_password(rec, password) != 0 || pda_write_user(rec) != 0)
		errx(1, "couldn't change %s's password", name);
	memset(pw, 0, sizeof(pw));
	CFRelease(rec);
	return 0;
}

static int
cmd_picture(const char *name, const char *file)
{
	char path[1100];
	CFMutableDictionaryRef rec;
	struct stat sb;

	need_root();
	rec = pda_copy_user(name);
	if (rec == NULL)
		errx(1, "no local user %s", name);
	if (stat(file, &sb) != 0 || !S_ISREG(sb.st_mode))
		errx(1, "%s is not a picture file", file);
	if (realpath(file, path) == NULL)
		err(1, "%s", file);
	pda_set_attr(rec, "picture", path);
	if (pda_write_user(rec) != 0)
		errx(1, "couldn't update %s", name);
	CFRelease(rec);
	return 0;
}

static int
cmd_delete(int argc, char **argv)
{
	const char *name = argv[0];
	int ch, removeHome = 0;

	need_root();
	optind = 1;
	while ((ch = getopt(argc, argv, "r")) != -1) {
		if (ch == 'r')
			removeHome = 1;
		else
			usage();
	}
	if (pda_remove_user(name, removeHome) != 0)
		err(1, "couldn't remove %s", name);
	return 0;
}

static int
cmd_verify(const char *name)
{
	char pw[256];
	CFMutableDictionaryRef rec;
	size_t len;
	int ok;

	need_root();
	if (fgets(pw, sizeof(pw), stdin) == NULL)
		return 1;
	len = strlen(pw);
	if (len > 0 && pw[len - 1] == '\n')
		pw[len - 1] = '\0';
	rec = pda_copy_user(name);
	ok = rec != NULL && pda_check_password(rec, pw);
	memset(pw, 0, sizeof(pw));
	if (rec)
		CFRelease(rec);
	return ok ? 0 : 1;
}

static int
cmd_group_list(void)
{
	CFArrayRef names = pda_copy_group_names();

	for (CFIndex i = 0; i < CFArrayGetCount(names); i++) {
		char name[256], gid[32];
		CFMutableDictionaryRef rec;
		CFTypeRef users;

		CFStringGetCString(CFArrayGetValueAtIndex(names, i), name, sizeof(name), kCFStringEncodingUTF8);
		if ((rec = pda_copy_group(name)) == NULL)
			continue;
		if (!pda_attr(rec, "gid", gid, sizeof(gid)))
			strcpy(gid, "?");
		printf("%-16s %6s ", name, gid);
		users = CFDictionaryGetValue(rec, CFSTR("users"));
		for (CFIndex k = 0; users != NULL && CFGetTypeID(users) == CFArrayGetTypeID() && k < CFArrayGetCount(users); k++) {
			char member[256];

			if (CFStringGetCString(CFArrayGetValueAtIndex(users, k), member, sizeof(member), kCFStringEncodingUTF8))
				printf(" %s", member);
		}
		printf("\n");
		CFRelease(rec);
	}
	CFRelease(names);
	return 0;
}

static int
cmd_group(int argc, char **argv)
{
	CFMutableDictionaryRef rec;

	if (argc >= 1 && strcmp(argv[0], "list") == 0)
		return cmd_group_list();
	need_root();
	if (argc >= 2 && strcmp(argv[0], "add") == 0) {
		gid_t gid = 0;
		int ch;

		optind = 2;
		while ((ch = getopt(argc, argv, "g:")) != -1) {
			if (ch == 'g')
				gid = (gid_t)strtol(optarg, NULL, 10);
			else
				usage();
		}
		if (!pda_valid_name(argv[1]))
			errx(1, "a group name uses letters, digits, '.', '_' and '-'");
		if ((rec = pda_copy_group(argv[1])) != NULL || getgrnam(argv[1]) != NULL)
			errx(1, "%s already exists", argv[1]);
		if (gid == 0)
			gid = strcmp(argv[1], "admin") == 0 ? PDA_ADMIN_GID : pda_next_gid();
		if (getgrgid(gid) != NULL)
			errx(1, "gid %u is taken", (unsigned)gid);
		rec = pda_new_group(argv[1], gid);
		if (pda_write_group(rec) != 0)
			err(1, "couldn't write %s", argv[1]);
		CFRelease(rec);
		printf("%s added, gid %u\n", argv[1], (unsigned)gid);
		return 0;
	}
	if (argc == 2 && strcmp(argv[0], "delete") == 0) {
		if (strcmp(argv[1], "admin") == 0)
			errx(1, "admin is how administrators are known, it stays");
		if (pda_delete_group(argv[1]) != 0)
			err(1, "couldn't delete %s", argv[1]);
		return 0;
	}
	if (argc == 3 && strcmp(argv[0], "addmember") == 0) {
		if ((rec = pda_copy_user(argv[2])) == NULL && getpwnam(argv[2]) == NULL)
			errx(1, "no user %s", argv[2]);
		if (rec)
			CFRelease(rec);
		if (pda_group_add_member(argv[1], argv[2]) != 0)
			errx(1, "no group %s", argv[1]);
		return 0;
	}
	if (argc == 3 && strcmp(argv[0], "removemember") == 0) {
		if (pda_group_remove_member(argv[1], argv[2]) != 0)
			errx(1, "no group %s", argv[1]);
		return 0;
	}
	usage();
	return 2;
}

int
main(int argc, char **argv)
{
	if (argc < 2)
		usage();
	if (strcmp(argv[1], "list") == 0)
		return cmd_list();
	if (argc < 3)
		usage();
	if (strcmp(argv[1], "group") == 0)
		return cmd_group(argc - 2, argv + 2);
	if (strcmp(argv[1], "add") == 0)
		return cmd_add(argc - 2, argv + 2);
	if (strcmp(argv[1], "passwd") == 0)
		return cmd_passwd(argc - 2, argv + 2);
	if (strcmp(argv[1], "picture") == 0 && argc == 4)
		return cmd_picture(argv[2], argv[3]);
	if (strcmp(argv[1], "delete") == 0)
		return cmd_delete(argc - 2, argv + 2);
	if (strcmp(argv[1], "verify") == 0)
		return cmd_verify(argv[2]);
	usage();
	return 2;
}
