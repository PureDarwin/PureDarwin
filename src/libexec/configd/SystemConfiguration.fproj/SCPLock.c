/*
 * Copyright (c) 2000-2010, 2013, 2015-2018, 2021 Apple Inc. All rights reserved.
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

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "SCPreferencesInternal.h"
#include "SCHelper_client.h"

#include <grp.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/attr.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/param.h>

/* CrashReporter "Application Specific Information" */
#include <CrashReporterClient.h>


static Boolean
__SCPreferencesLock_helper(SCPreferencesRef prefs, Boolean wait)
{
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	uint32_t		status		= kSCStatusOK;
	CFDataRef		reply		= NULL;

	if (prefsPrivate->helper_port == MACH_PORT_NULL) {
		ok = __SCPreferencesCreate_helper(prefs);
		if (!ok) {
			return FALSE;
		}
	}

	// have the helper "lock" the prefs
	status = kSCStatusOK;
	reply  = NULL;
	ok = _SCHelperExec(prefsPrivate->helper_port,
			   wait ? SCHELPER_MSG_PREFS_LOCKWAIT : SCHELPER_MSG_PREFS_LOCK,
			   prefsPrivate->signature,
			   &status,
			   NULL);
	if (!ok) {
		goto fail;
	}

	if (status != kSCStatusOK) {
		goto error;
	}

	__SCPreferencesUpdateLockedState(prefs, TRUE);
	return TRUE;

    fail :

	// close helper
	if (prefsPrivate->helper_port != MACH_PORT_NULL) {
		_SCHelperClose(&prefsPrivate->helper_port);
	}

	status = kSCStatusAccessError;

    error :

	// return error
	_SCErrorSet(status);
	return FALSE;
}


static int
createParentDirectory(const char *path)
{
	char	dir[PATH_MAX];
	int	ret;
	char	*scan;
	char	*slash;

	// get parent directory path
	if (strlcpy(dir, path, sizeof(dir)) >= sizeof(dir)) {
		errno = ENOENT;
		return -1;
	}

	slash = strrchr(dir, '/');
	if ((slash == NULL) || (slash == dir)) {
		errno = ENOENT;
		return -1;
	}
	*slash = '\0';

	// create parent directories
	for (scan = dir; TRUE; scan = slash) {
		mode_t	mode	= S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;	// 755
		char	sep	= '\0';

		if (slash != NULL) {
			sep = *slash;
			*slash = '\0';
		}

		ret = mkdir(dir, mode);
		if (ret == 0) {
			static	gid_t	group	= -1;

			// set group
			if (group == (gid_t)-1) {
				char		buf[256];
				struct group	grp;
				struct group	*grpP	= NULL;

				if ((getgrnam_r("wheel", &grp, buf, sizeof(buf), &grpP) == 0) &&
				    (grpP != NULL)) {
					group = grpP->gr_gid;
				} else {
					SC_log(LOG_NOTICE, "getgrnam_r() failed: %s", strerror(errno));
					group = 0;	// wheel
				}
			}

			if (chown(dir, -1, group) == -1) {
				SC_log(LOG_NOTICE, "chown() failed: %s", strerror(errno));
			}

			// set [force] mode
			if (chmod(dir, mode) == -1) {
				SC_log(LOG_NOTICE, "chmod() failed: %s", strerror(errno));
			}

			if ((slash == NULL) || (scan == dir)) {
				return 0;
			}
		} else if ((errno == ENOENT) && (scan == dir)) {
			// the initial mkdir (of the full dir path) can fail
			;
		} else if (errno == EROFS) {
			return -1;
		} else if (errno != EEXIST) {
			break;
		}

		if (slash != NULL) {
			*slash = sep;
		} else {
			break;
		}
		slash = strchr(scan + 1, '/');
	}

	SC_log(LOG_NOTICE, "mkdir() failed: %s", strerror(errno));
	return -1;
}


static void
reportDelay(SCPreferencesRef prefs, struct timeval *delay, Boolean isStale)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	SC_log(LOG_ERR,
	       "SCPreferences(%@:%@) lock delayed for %d.%3.3d seconds%s",
	       prefsPrivate->name,
	       prefsPrivate->prefsID,
	       (int)delay->tv_sec,
	       delay->tv_usec / 1000,
	       isStale ? " (stale)" : "");
	return;
}


static Boolean
has_O_EXLOCK(SCPreferencesPrivateRef prefsPrivate)
{
#pragma pack(push, 4)
	struct {
		u_int32_t		size;
		vol_capabilities_attr_t	capabilities;
	} attrbuf;
#pragma pack(pop)
	struct attrlist			attrs;
	int				fd;
	int				ret;
	struct statfs			statbuf;

	fd = open(prefsPrivate->lockPath, O_WRONLY|O_CREAT, 0644);
	if (fd == -1) {
		SC_log(LOG_NOTICE, "open() failed: %s", strerror(errno));
		return FALSE;
	}

	ret = fstatfs(fd, &statbuf);
	unlink(prefsPrivate->lockPath);
	close(fd);
	if (ret == -1) {
		SC_log(LOG_NOTICE, "fstatfs() failed: %s", strerror(errno));
		return FALSE;
	}

	memset(&attrs, 0, sizeof(attrs));
	attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrs.volattr     = ATTR_VOL_INFO | ATTR_VOL_CAPABILITIES;
	memset(&attrbuf, 0, sizeof(attrbuf));
	ret = getattrlist(statbuf.f_mntonname,	// path (of mount point)
			  &attrs,		// attribute list
			  &attrbuf,		// attribute buffer
			  sizeof(attrbuf),
			  0);			// options
	if (ret == -1) {
		SC_log(LOG_NOTICE, "getattrlist() failed: %s", strerror(errno));
		return FALSE;
	}

	if ((attrbuf.capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_FLOCK) &&
	    (attrbuf.capabilities.valid       [VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_FLOCK)) {
		return TRUE;
	}

	return FALSE;
}


static Boolean
lockWithSCDynamicStore(SCPreferencesRef	prefs, Boolean wait)
{
	CFArrayRef		changes;
	Boolean			locked		= FALSE;
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	int			sc_status	= kSCStatusOK;

	// add SCDynamicStore session (for lock monitoring)
	ok = __SCPreferencesAddSession(prefs);
	if (!ok) {
		return FALSE;
	}

	// add [lock] notification
	ok = SCDynamicStoreAddWatchedKey(prefsPrivate->session,
					 prefsPrivate->sessionKeyLock,
					 FALSE);
	if (!ok) {
		sc_status = SCError();
		SC_log(LOG_INFO, "SCDynamicStoreAddWatchedKey() failed");
	}

	// add SCDynamicStore session (for the actual lock)
	if (ok) {
		prefsPrivate->sessionNoO_EXLOCK = SCDynamicStoreCreate(NULL, prefsPrivate->name, NULL, NULL);
		if (prefsPrivate->sessionNoO_EXLOCK == NULL) {
			sc_status = SCError();
			SC_log(LOG_INFO, "SCDynamicStoreCreate() failed");
			ok = FALSE;
		}
	}

	while (ok) {
		CFDateRef	value;

		// Attempt to acquire the lock
		value = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
		ok = SCDynamicStoreAddTemporaryValue(prefsPrivate->sessionNoO_EXLOCK,
						     prefsPrivate->sessionKeyLock,
						     value);
		CFRelease(value);
		if (ok) {
			locked = TRUE;
			break;
		}

		if (!wait) {
			sc_status = kSCStatusPrefsBusy;
			break;
		}

		// wait for the lock to be released
		ok = SCDynamicStoreNotifyWait(prefsPrivate->session);
		if (!ok) {
			sc_status = SCError();
			SC_log(LOG_INFO, "SCDynamicStoreNotifyWait() failed");
			break;
		}

		// clear out any notifications
		changes = SCDynamicStoreCopyNotifiedKeys(prefsPrivate->session);
		if (changes != NULL) {
			CFRelease(changes);
		} else {
			SC_log(LOG_INFO, "SCDynamicStoreCopyNotifiedKeys() failed");
			break;
		}
	}

	// remove [lock] notification
	(void) SCDynamicStoreRemoveWatchedKey(prefsPrivate->session,
					      prefsPrivate->sessionKeyLock,
					      0);

	// clear out any notifications
	changes = SCDynamicStoreCopyNotifiedKeys(prefsPrivate->session);
	if (changes != NULL) {
		CFRelease(changes);
	}

	__SCPreferencesRemoveSession(prefs);

	if (!locked && (prefsPrivate->sessionNoO_EXLOCK != NULL)) {
		CFRelease(prefsPrivate->sessionNoO_EXLOCK);
		prefsPrivate->sessionNoO_EXLOCK = NULL;
	}

	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);
	}

	return locked;
}


static void
__wait_for_PreferencesMonitor(void)
{
	CFArrayRef		keys;
	Boolean			ok;
	SCDynamicStoreRef	store	= NULL;

	CRSetCrashLogMessage("Waiting for configd/PreferencesMonitor");

	store = SCDynamicStoreCreate(NULL, CFSTR("__wait_for_PreferencesMonitor"), NULL, NULL);
	if (store == NULL) {
		goto done;
	}

	keys = CFArrayCreate(NULL, (const void **)&kSCDynamicStoreDomainSetup, 1, &kCFTypeArrayCallBacks);
	ok = SCDynamicStoreSetNotificationKeys(store, keys, NULL);
	CFRelease(keys);
	if (!ok) {
		SC_log(LOG_NOTICE, "SCDynamicStoreSetNotificationKeys() failed: %s", SCErrorString(SCError()));
		goto done;
	}

	while (TRUE) {
		CFArrayRef	changedKeys;
		CFDictionaryRef	dict;

		// check if quiet
		dict = SCDynamicStoreCopyValue(store, kSCDynamicStoreDomainSetup);
		if (dict != NULL) {
			// the "Setup:" key is present indicating that
			// configd/PreferencesMonitor has progressed past
			// its call to __SCNetworkConfigurationUpgrade()
			CFRelease(dict);
			break;
		}

		ok = SCDynamicStoreNotifyWait(store);
		if (!ok) {
			SC_log(LOG_NOTICE, "SCDynamicStoreNotifyWait() failed: %s", SCErrorString(SCError()));
			goto done;
		}

		changedKeys = SCDynamicStoreCopyNotifiedKeys(store);
		if (changedKeys != NULL) {
			CFRelease(changedKeys);
		}
	}

    done :

	CRSetCrashLogMessage(NULL);

	if (store != NULL) CFRelease(store);
	return;
}


/*
 * Locking the default network configuration can result in a deadlock.
 * Specifically, the following can occur :
 *
 * 1. the configd/InterfaceNamer and configd/PreferencesMonitor startup code
 *    includes checks to see if the configuration needs to be upgraded.  If
 *    needed, the plugins will acquire the lock, perform the upgrade, and
 *    release the lock.
 *
 *    Note: the list of network interfaces is NOT available until after the
 *          upgrade process has completed
 *
 * 2. a process (like CommCenter) opens the current network configuration
 *    and checks the current network set.  If no network set exists then
 *    the lock is acquired, a call is made to get the list of available
 *    network interfaces, a new template configuration is created, and
 *    release the lock.
 *
 * If (1) happens before (2) then we're fine.  But, if (2) acquires the
 * lock first then we deadlock because we need the list of interfaces and
 * that won't happen until (1) has completed.
 *
 * To avoid this deadlock we wait until after the upgrade check/action.
 */
static void
avoid_SCNetworkConfiguration_deadlock(SCPreferencesRef prefs)
{
	static dispatch_once_t	once;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	CFBooleanRef		val		= NULL;

	if ((prefsPrivate->options != NULL) &&
	    CFDictionaryGetValueIfPresent(prefsPrivate->options,
					  kSCPreferencesOptionAvoidDeadlock,
					  (const void **)&val) &&
	    isA_CFBoolean(val) &&
	    !CFBooleanGetValue(val)) {
		// don't block SCPreferencesLock() until the upgrade
		// check/action has completed
		return;
	}

	if (!__SCPreferencesUsingDefaultPrefs(prefs)) {
		// if not the default network configuration
		return;
	}

	// to avoid a startup deadlock, we need to wait for
	// configd/PreferencesMonitor to progress past its
	// call to __SCNetworkConfigurationUpgrade().
	dispatch_once(&once, ^{
		__wait_for_PreferencesMonitor();
	});

	return;
}


Boolean
SCPreferencesLock(SCPreferencesRef prefs, Boolean wait)
{
	char			buf[32];
	struct timeval		lockStart;
	struct timeval		lockElapsed;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	int			sc_status	= kSCStatusFailed;
	struct stat		statBuf;
	struct stat		statBuf2;

	if (prefs == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return FALSE;
	}

	if (prefsPrivate->locked) {
		/* sorry, you already have the lock */
		_SCErrorSet(kSCStatusLocked);
		return FALSE;
	}

	if (prefsPrivate->authorizationData != NULL) {
		return __SCPreferencesLock_helper(prefs, wait);
	}

	if (!prefsPrivate->isRoot) {
		_SCErrorSet(kSCStatusAccessError);
		return FALSE;
	}

	avoid_SCNetworkConfiguration_deadlock(prefs);

	pthread_mutex_lock(&prefsPrivate->lock);

	__SCPreferencesAddSessionKeys(prefs);

	if (prefsPrivate->lockPath == NULL) {
		char	*path;
		CFIndex	pathLen;

		path = prefsPrivate->path;
		pathLen = strlen(path) + sizeof("-lock");
		prefsPrivate->lockPath = CFAllocatorAllocate(NULL, pathLen, 0);
		snprintf(prefsPrivate->lockPath, pathLen, "%s-lock", path);
	}

	(void)gettimeofday(&lockStart, NULL);

    retry :

	if (prefsPrivate->sessionKeyLock != NULL) {
		if (lockWithSCDynamicStore(prefs, wait)) {
			goto locked;
		}

		goto error;
	}

	prefsPrivate->lockFD = open(prefsPrivate->lockPath,
				    wait ? O_WRONLY|O_CREAT|O_EXLOCK
					 : O_WRONLY|O_CREAT|O_EXLOCK|O_NONBLOCK,
				    0644);
	if (prefsPrivate->lockFD == -1) {
		switch (errno) {
			case ENOENT :
				if ((prefsPrivate->prefsID == NULL) ||
				    !CFStringHasPrefix(prefsPrivate->prefsID, CFSTR("/"))) {
					int	ret;

					// create parent (/Library/Preferences/SystemConfiguration)
					ret = createParentDirectory(prefsPrivate->lockPath);
					if (ret == 0) {
						SC_log(LOG_INFO, "created directory for \"%s\"",
						       prefsPrivate->path);
						goto retry;
					} else if (errno == EROFS) {
						goto locked;
					}
				}
				break;
			case EROFS :
				// if read-only filesystem
				goto locked;
			case EWOULDBLOCK :
				// if already locked (and we are not blocking)
				sc_status = kSCStatusPrefsBusy;
				goto error;
			case ENOTSUP :
				if (!has_O_EXLOCK(prefsPrivate)) {
					// O_EXLOCK *not* available, use SCDynamicStore
					prefsPrivate->sessionKeyLock = _SCPNotificationKey(NULL,
											   prefsPrivate->prefsID,
											   kSCPreferencesKeyLock);
					goto retry;
				}
				errno = ENOTSUP;
				break;
			default :
				break;
		}

		sc_status = errno;
		SC_log(LOG_NOTICE, "open() failed: %s", strerror(errno));
		goto error;
	}

	if ((stat(prefsPrivate->lockPath, &statBuf) == -1) ||
	    (fstat(prefsPrivate->lockFD, &statBuf2) == -1) ||
	    (statBuf.st_dev != statBuf2.st_dev) ||
	    (statBuf.st_ino != statBuf2.st_ino)) {
		// if the lock file was unlinked or re-created
		close(prefsPrivate->lockFD);
		prefsPrivate->lockFD = -1;
		goto retry;
	}

	// we have the lock

	snprintf(buf, sizeof(buf), "%d\n", getpid());
	(void) write(prefsPrivate->lockFD, buf, strlen(buf));

    locked :

	(void)gettimeofday(&prefsPrivate->lockTime, NULL);
	timersub(&prefsPrivate->lockTime, &lockStart, &lockElapsed);

	if (prefsPrivate->accessed) {
		CFDataRef       currentSignature;
		Boolean		match;

		/*
		 * the preferences have been accessed since the
		 * session was created so we need to compare
		 * the signature of the stored preferences.
		 */
		if (stat(prefsPrivate->path, &statBuf) == -1) {
			if (errno == ENOENT) {
				memset(&statBuf, 0, sizeof(statBuf));
			} else {
				SC_log(LOG_INFO, "stat() failed: %s",
				      strerror(errno));
				goto stale;
			}
		}

		currentSignature = __SCPSignatureFromStatbuf(&statBuf);
		match = CFEqual(prefsPrivate->signature, currentSignature);
		CFRelease(currentSignature);
		if (!match) {
			/*
			 * the preferences have been updated since the
			 * session was accessed so we've got no choice
			 * but to deny the lock request.
			 */
			goto stale;
		}
//	} else {
//		/*
//		 * the file contents have changed but since we
//		 * haven't accessed any of the preference data we
//		 * don't need to return an error.  Simply proceed.
//		 */
	}

	if (lockElapsed.tv_sec > 0) {
		// if we waited more than 1 second to acquire the lock
		reportDelay(prefs, &lockElapsed, FALSE);
	}

	SC_log(LOG_DEBUG, "SCPreferences() lock: %s",
	       prefsPrivate->path);

	__SCPreferencesUpdateLockedState(prefs, TRUE);
	pthread_mutex_unlock(&prefsPrivate->lock);
	return TRUE;

    stale :

	sc_status = kSCStatusStale;
	unlink(prefsPrivate->lockPath);

	if (lockElapsed.tv_sec > 0) {
		// if we waited more than 1 second to acquire the lock
		reportDelay(prefs, &lockElapsed, TRUE);
	}

    error :

	if (prefsPrivate->lockFD != -1)	{
		close(prefsPrivate->lockFD);
		prefsPrivate->lockFD = -1;
	}

	pthread_mutex_unlock(&prefsPrivate->lock);
	_SCErrorSet(sc_status);
	return FALSE;
}
