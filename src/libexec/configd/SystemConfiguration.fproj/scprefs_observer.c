/*
 * Copyright (c) 2012, 2013, 2015-2020 Apple Inc. All rights reserved.
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
#include <CommonCrypto/CommonDigest.h>
#include <dirent.h>
#include <notify.h>
#include <os/log.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef	SC_LOG_HANDLE
#define SC_LOG_HANDLE	__log_SCPreferences
#endif	//SC_LOG_HANDLE
os_log_t	SC_LOG_HANDLE(void);

#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/scprefs_observer.h>

#if	!TARGET_OS_IPHONE
#define	MANAGED_PREFERENCES_PATH	"/Library/Managed Preferences"
#define	PREFS_OBSERVER_KEY		"com.apple.MCX._managementStatusChangedForDomains"	// posted by CF (OK for macOS, iOS)
#else	// !TARGET_OS_IPHONE
#define	MANAGED_PREFERENCES_MOBILE_PATH	"/Library/Managed Preferences/mobile"
#define	PREFS_OBSERVER_KEY		"com.apple.ManagedConfiguration.profileListChanged"	// posted by ManagedConfiguration
#endif	// !TARGET_OS_IPHONE

#pragma mark -
#pragma mark Utils

#if	!TARGET_OS_IPHONE
static void
iterate_dir(const char *d_name, const char *f_name,
	    CC_SHA256_CTX *ctxP, Boolean *found)
{
	DIR *dir;
	struct dirent * dp;

	dir = opendir(d_name);

	if (dir == NULL) {
		/* if directory path does not exist */
		return;
	}

	while ((dp = readdir(dir)) != NULL) {
		char		full_path[MAXPATHLEN];
		struct stat	s;

		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0)) {
			continue;
		}

		/* check path */
		snprintf(full_path, sizeof(full_path), "%s/%s", d_name, dp->d_name);
		if (stat(full_path, &s) == 0) {
			if (S_ISDIR(s.st_mode)) {
				// if sub-directory, iterate
				iterate_dir(full_path, f_name, ctxP, found);
			} else if (strcmp(f_name, dp->d_name) == 0) {
				/*
				 * if this is the requested file, include
				 * the path and last modification time in
				 * the digest
				*/
				CC_SHA256_Update(ctxP, full_path, (CC_LONG)strlen(full_path));
				CC_SHA256_Update(ctxP,
						 (void *)&s.st_mtimespec,
						 sizeof(s.st_mtimespec));
				*found = TRUE;
			}
		}
	}
	closedir(dir);
	return;
}
#else	// !TARGET_OS_IPHONE
static void
observe_plist(const char *d_name, const char *f_name,
	      CC_SHA256_CTX *ctxP, Boolean *found)
{
	char		full_path[MAXPATHLEN];
	struct stat	s;

	snprintf(full_path, sizeof(full_path), "%s/%s", d_name, f_name);
	if ((stat(full_path, &s) == 0) && S_ISREG(s.st_mode)) {
		CC_SHA256_Update(ctxP, full_path, (CC_LONG)strlen(full_path));
		CC_SHA256_Update(ctxP,
				 (void *)&s.st_mtimespec,
				 sizeof(s.st_mtimespec));
		*found = TRUE;
	}

	return;
}
#endif	// !TARGET_OS_IPHONE

static CF_RETURNS_RETAINED CFDataRef
build_digest(const char *top_dir, const char *file)
{
	unsigned char	bytes[CC_SHA256_DIGEST_LENGTH];
	CC_SHA256_CTX	ctx;
	CFDataRef	digest = NULL;
	Boolean		found = FALSE;

	CC_SHA256_Init(&ctx);
#if	!TARGET_OS_IPHONE
	iterate_dir(top_dir, file, &ctx, &found);
#else	// !TARGET_OS_IPHONE
	observe_plist(top_dir, file, &ctx, &found);
#endif	// !TARGET_OS_IPHONE
	CC_SHA256_Final(bytes, &ctx);
	if (found) {
		digest = CFDataCreate(NULL, bytes, sizeof(bytes));
	}
	return (digest);
}

#pragma mark -
#pragma mark perfs_observer Private

struct _scprefs_observer_t {
	_scprefs_observer_type			type;
	dispatch_block_t			block;
	CFDataRef				digest;
	SLIST_ENTRY(_scprefs_observer_t)	next;
	dispatch_queue_t			queue;
	char					file[0];
};

static const char *
prefs_observer_get_prefs_path(scprefs_observer_t observer)
{
	switch (observer->type) {
#if	!TARGET_OS_IPHONE
	case scprefs_observer_type_mcx:
		return MANAGED_PREFERENCES_PATH;
#else	// !TARGET_OS_IPHONE
	case scprefs_observer_type_global:
		return MANAGED_PREFERENCES_MOBILE_PATH;
#endif	// !TARGET_OS_IPHONE
	}

	return NULL;
}

/*
 * Iterate through all of the directories and subdirectories.
 * If the file within those directories has changed,
 * then generate the notification.
 */
static Boolean
has_changed(scprefs_observer_t  observer) {
	Boolean         changed;
	CFDataRef       digest = NULL;
	const char *    starting_path;

	starting_path = prefs_observer_get_prefs_path(observer);

	digest = build_digest(starting_path, observer->file);

	/* compare old vs. new digest */
	changed = _SC_CFEqual(digest, observer->digest)?FALSE:TRUE;

	/* save the digest */
	if (observer->digest != NULL) {
		CFRelease(observer->digest);
	}

	observer->digest = digest;

	SC_log(changed ? LOG_INFO : LOG_DEBUG,
	       "preferences file: \"%s\" %s",
	       observer->file,
	       changed ? "changed" : "did not change");
	return changed;
}

static dispatch_queue_t
prefs_observer_queue;

/* This holds the list of the observers */
static SLIST_HEAD(mylist, _scprefs_observer_t) head;

static void
prefs_observer_release(scprefs_observer_t observer)
{
	SLIST_REMOVE(&head, observer, _scprefs_observer_t, next);

	/* Now free the observer */
	if (observer->digest != NULL) {
		CFRelease(observer->digest);
	}

	free(observer);
}

static void
prefs_observer_handle_notifications()
{
	scprefs_observer_t observer;

	SC_log(LOG_DEBUG, "PrefsObserver notification received");

	SLIST_FOREACH(observer, &head, next) {
		/* if the preferences plist changed, call the block */
		if (has_changed(observer)) {
			dispatch_async(observer->queue, observer->block);
		}
	}
}

static void
_prefs_observer_init()
{
	uint32_t	status;
	static int	token;

	prefs_observer_queue = dispatch_queue_create("com.apple.SystemConfiguration.SCPreferencesObserver", NULL);

	SLIST_INIT(&head);

	status = notify_register_dispatch(PREFS_OBSERVER_KEY,
					  &token,
					  prefs_observer_queue,
					  ^(int token) {
#pragma unused(token)
						  prefs_observer_handle_notifications();
					  });
	if (status != NOTIFY_STATUS_OK) {
		SC_log(LOG_INFO, "notify_register_dispatch() failed: %d", status);
	}
}

static scprefs_observer_t
prefs_observer_priv_create(_scprefs_observer_type type,
			   const char *plist_name,
			   dispatch_queue_t queue,
			   dispatch_block_t block)
{
	scprefs_observer_t	observer;
	size_t			path_buflen;

	path_buflen = strlen(plist_name) + 1;

	observer = (scprefs_observer_t)malloc(sizeof(struct _scprefs_observer_t) + path_buflen);
	memset((void *)observer, 0, sizeof(struct _scprefs_observer_t));

	/* Create the observer */
	observer->type = type;
	strlcpy(observer->file, plist_name, path_buflen);

	observer->queue = queue;
	observer->block = Block_copy(block);

	return (observer);
}

#pragma mark -
#pragma mark perfs_observer Public SPI
scprefs_observer_t
_scprefs_observer_watch(_scprefs_observer_type type, const char *plist_name,
			   dispatch_queue_t queue, dispatch_block_t block)
{
	scprefs_observer_t elem;
	static dispatch_once_t initialized;

	dispatch_once(&initialized, ^{
		_prefs_observer_init();
	});

	elem = prefs_observer_priv_create(type, plist_name, queue, block);
	SC_log(LOG_INFO, "Created a new element to watch for %s", elem->file);

	dispatch_sync(prefs_observer_queue, ^{
		/* Enqueue the request */
		SLIST_INSERT_HEAD(&head, elem, next);
	});
	return (elem);
}

/* This will cancel/deregister the given watcher.  This will be synchronized on the
 * internally created queue. */
void
_scprefs_observer_cancel(scprefs_observer_t observer)
{
	dispatch_sync(prefs_observer_queue, ^{
		prefs_observer_release((scprefs_observer_t)observer);
	});
}

#pragma mark -

#ifdef TEST_MAIN
int main()
{
	int random = 1;

	_sc_verbose = 1;

	dispatch_queue_t q = dispatch_queue_create("com.apple.SystemConfiguration.PrefsObserver.mainQ", NULL);

	dispatch_queue_t q1 = dispatch_queue_create("com.apple.SystemConfiguration.PrefsObserver.testQ1", NULL);

	dispatch_block_t b1 = ^{
	printf("Block 1 executed\n");
	};

	dispatch_queue_t q2 = dispatch_queue_create("com.apple.SystemConfiguration.PrefsObserver.testQ2", NULL);
	dispatch_block_t b2  = ^{
	printf("Block 2 executed\n");
	};

	dispatch_queue_t q3 =  dispatch_queue_create("com.apple.SystemConfiguration.PrefsObserver.testQ2", NULL);

	dispatch_block_t b3 = ^{
	printf("Block 3 executed\n");
	};

	__block scprefs_observer_t observer1 = _scprefs_observer_watch(scprefs_observer_type_mcx, "com.apple.SystemConfiguration", q1, b1);

	__block scprefs_observer_t observer2 = _scprefs_observer_watch(scprefs_observer_type_mcx, "foo", q2, b2);

	__block scprefs_observer_t observer3 = _scprefs_observer_watch(scprefs_observer_type_mcx, "bar", q3, b3);

	__block scprefs_observer_t observer = NULL;

	while (1) {
	switch (random % 3)
	{
	    case 0:
		dispatch_async(q, ^{
			_SC_prefs_observer_cancel(observer1);
			observer1 = NULL;
		});
		dispatch_async(q, ^{
			if (observer != NULL)  _SC_prefs_observer_cancel(observer);
			observer = _SC_prefs_observer_watch(SC_prefs_observer_type_mcx, "test", q2, b2);
		});
		dispatch_sync(q, ^{
			observer1 = observer;
		});
		sleep(random);
		break;
	    case 1:
		dispatch_async(q, ^{
			_SC_prefs_observer_cancel(observer2);
		});
		dispatch_async(q, ^{
			if (observer != NULL) _SC_prefs_observer_cancel(observer);
		});
		dispatch_sync(q, ^{
			observer = _SC_prefs_observer_watch(SC_prefs_observer_type_mcx, "test", q2, b2);
		});
		sleep(random);
		break;
	    case 2:
		sleep (random);
	    default:
		break;
	}
	random++;
	}
	dispatch_main();
}
#endif
