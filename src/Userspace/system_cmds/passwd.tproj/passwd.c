/*
 * Copyright (c) 1999-2016 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
#include <TargetConditionals.h>

#define _PASSWD_FILE "/etc/master.passwd"

#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <libc.h>
#include <ctype.h>
#include <string.h>
#include "passwd.h"

#ifdef __SLICK__
#define _PASSWORD_LEN 8
#endif

char* progname = "passwd";

static char *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

void
getpasswd(char *name, int isroot, int minlen, int mixcase, int nonalpha,
	char *old_pw, char **new_pw, char **old_clear, char **new_clear)
{
	int i, tries, pw_ok, upper, lower, alpha, notalpha;
    size_t len;
	int isNull;
	char *p;
	static char obuf[_PASSWORD_LEN+1];
	static char nbuf[_PASSWORD_LEN+1];
	char salt[9];

	printf("Changing password for %s.\n", name);

	p = "";
	isNull = 0;
	if (old_pw == NULL) isNull = 1;
	if ((isNull == 0) && (old_pw[0] == '\0')) isNull = 1;
	if ((isroot == 0) && (isNull == 0))
	{
		p = getpass("Old password:");
		if (strcmp(crypt(p, old_pw), old_pw))
		{
			errno = EACCES;
			fprintf(stderr, "Sorry\n");
			exit(1);
		}
	}
	//strcpy(obuf, p);
	snprintf( obuf, sizeof(obuf), "%s", p );

	tries = 0;
	nbuf[0] = '\0';
	for (;;)
	{
		p = getpass("New password:");
		if (!*p)
		{
			printf("Password unchanged.\n");
			exit(0);
		}

		tries++;
		len = strlen(p);
		upper = 0;
		lower = 0;
		alpha = 0;
		notalpha = 0;
		for (i = 0; i < len; i++)
		{
			if (isupper(p[i])) upper++;
			if (islower(p[i])) lower++;
			if (isalpha(p[i])) alpha++;
			else notalpha++;
		}


		pw_ok = 1;
		if (len < minlen) pw_ok = 0;
		if ((mixcase == 1) && ((upper == 0) || (lower == 0))) pw_ok = 0;
		if ((nonalpha == 1) && (notalpha == 0)) pw_ok = 0;

		/*
		 * An insistent root may override security options.
		 */
		if ((isroot == 1) && (tries > 2)) pw_ok = 1;

		/*
		 * A very insistent user may override security options.
		 */
		if (tries > 4) pw_ok = 1;

		if (pw_ok == 0)
		{
			if (len < minlen)
				printf("Password must be at least %d characters long.\n", minlen);
			if ((mixcase == 1) && ((upper == 0) || (lower == 0)))
				printf("Password must contain both upper and lower case characters.\n");
			if ((nonalpha == 1) && (notalpha == 0))
				printf("Password must contain non-alphabetic characters.\n");
			continue;
		}

		//strcpy(nbuf, p);
		snprintf( nbuf, sizeof(nbuf), "%s", p );

		if (!strcmp(nbuf, getpass("Retype new password:"))) break;

		printf("Mismatch; try again, EOF to quit.\n");
	}

	/*
	 * Create a random salt
	 */
	srandom((int)time((time_t *)NULL));
	salt[0] = saltchars[random() % strlen(saltchars)];
	salt[1] = saltchars[random() % strlen(saltchars)];
	salt[2] = '\0';
	*new_pw = crypt(nbuf, salt);

	*old_clear = obuf;
	*new_clear = nbuf;
	return;
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-i infosystem] -l location]] [-u authname] [name]\n", progname);
	fprintf(stderr, "  infosystem:\n");
	fprintf(stderr, "    file\n");
	fprintf(stderr, "    NIS\n");
	fprintf(stderr, "    OpenDirectory\n");
	fprintf(stderr, "    PAM\n");
	fprintf(stderr, "  location (for infosystem):\n");
	fprintf(stderr, "    file           location is path to file (default is %s)\n", _PASSWD_FILE);
	fprintf(stderr, "    NIS            location is NIS domain name\n");
	fprintf(stderr, "    OpenDirectory  location is directory node name\n");
	fprintf(stderr, "    PAM            location is not used\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char* user = NULL;
	char* locn = NULL;
	char* auth = NULL;
	int infosystem, ch;
	int free_user = 0;
	int res = -1;

#ifdef INFO_PAM
	infosystem = INFO_PAM;
#else
#ifdef INFO_OPEN_DIRECTORY
	infosystem = INFO_OPEN_DIRECTORY;
#else
	infosystem = INFO_FILE;
#endif
#endif

#ifdef INFO_OPEN_DIRECTORY
	/* PAM is the default infosystem, but we still want to use OpenDirectory directly when run by root */
	if (0 == getuid())
		infosystem = INFO_OPEN_DIRECTORY;
#endif

	while ((ch = getopt(argc, argv, "i:l:u:")) != -1)
		switch(ch) {
		case 'i':
			if (!strcasecmp(optarg, "file")) {
				infosystem = INFO_FILE;
#ifdef INFO_NIS
			} else if (!strcasecmp(optarg, "NIS")) {
				infosystem = INFO_NIS;
			} else if (!strcasecmp(optarg, "YP")) {
				infosystem = INFO_NIS;
#endif
#ifdef INFO_OPEN_DIRECTORY
			} else if (!strcasecmp(optarg, "opendirectory")) {
				infosystem = INFO_OPEN_DIRECTORY;
#endif
#ifdef INFO_PAM
			} else if (!strcasecmp(optarg, "PAM")) {
				infosystem = INFO_PAM;
#endif
			} else {
				fprintf(stderr, "%s: Unknown info system \'%s\'.\n",
					progname, optarg);
				usage();
			}
			break;
		case 'l':
			locn = optarg;
			break;
		case 'u':
			auth = optarg;
			break;
		case '?':
		default:
			usage();
			break;
	}
	argc -= optind;
	argv += optind;

	if (argc > 1) {
		usage();
	} else if (argc == 1) {
		user = argv[0];
	}

#ifdef INFO_PAM
	if (INFO_PAM == infosystem && NULL != locn)
		usage();
#endif

	if (user == NULL)
	{
		/*
		 * Verify that the login name exists.
		 * lukeh 24 Dec 1997
		 */

		/* getlogin() is the wrong thing to use here because it returns the wrong user after su */
		/* sns 5 Jan 2005 */

		struct passwd * userRec = getpwuid(getuid());
		if (userRec != NULL && userRec->pw_name != NULL) {
			/* global static mem is volatile; must strdup */
			user = strdup(userRec->pw_name);
			free_user = 1;
		}

		if (user == NULL)
		{
			fprintf(stderr, "you don't have a login name\n");
			exit(1);
		}
	}

	// a person's account lives in dslocal, its hash in ShadowHashData that dsuser writes
	// as a person, dsuser asks pdaccountsd and gives it the old password
	if (infosystem == INFO_FILE && locn == NULL && strchr(user, '/') == NULL)
	{
		char record[1024];

		snprintf(record, sizeof(record), "/var/db/dslocal/nodes/Default/users/%s.plist", user);
		if (access(record, F_OK) == 0 || (errno == EACCES && getuid() != 0))
		{
			execl("/usr/sbin/dsuser", "dsuser", "passwd", user, (char *)NULL);
			perror("/usr/sbin/dsuser");
			exit(1);
		}
	}

	switch (infosystem)
	{
		case INFO_FILE:
			res = file_passwd(user, locn);
			break;
#ifdef INFO_NIS
		case INFO_NIS:
			res = nis_passwd(user, locn);
			break;
#endif
#ifdef INFO_OPEN_DIRECTORY
		case INFO_OPEN_DIRECTORY:
			res = od_passwd(user, locn, auth);
			break;
#endif
#ifdef INFO_PAM
		case INFO_PAM:
			res = pam_passwd(user);
			break;
#endif
	}

	if (res == 0)
	{
		printf("\n");
		printf("################################### WARNING ###################################\n");
		printf("# This tool does not update the login keychain password.                      #\n");
		printf("# To update it, run `security set-keychain-password` as the user in question, #\n");
		printf("# or as root providing a path to such user's login keychain.                  #\n");
		printf("###############################################################################\n");
		printf("\n");
	}

	if (free_user == 1)
		free(user);

	exit(0);
}
