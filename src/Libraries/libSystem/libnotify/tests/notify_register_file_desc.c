//
//  notify_register_file_desc.c
//
//  Created by Brycen Wershing on 6/29/20.
//


#include <stdlib.h>
#include <notify.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <darwintest.h>
#include <signal.h>
#include "../libnotify.h"

T_DECL(notify_register_file_desc, "Make sure mach port registrations works",
		T_META("owner", "Core Darwin Daemons & Tools"),
		T_META_ASROOT(YES))
{
	const char *KEY = "com.apple.notify.test.file_desc";
	int rc, fd, tok;

	rc = notify_register_file_descriptor(KEY, &fd, 0, &tok);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "register file desc should work");
	T_ASSERT_NE(fcntl(fd, F_GETFD), -1, "file descriptor should exist");

	rc = notify_cancel(tok);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "cancel should work");
	T_ASSERT_EQ(fcntl(fd, F_GETFD), -1, "file descriptor should not exist");
}

