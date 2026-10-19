/*
 * FSEvents.c - the CoreServices file system event stream API
 *
 * A stream reads the kernel's /dev/fsevents when the process may open it
 * (root), keeps the events under its watched paths and hands them to the
 * client's run loop or dispatch queue after the requested latency. Other
 * processes fall back to kqueue vnode watches on the watched trees.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <CoreServices/CoreServices.h>
#include <sys/event.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// the user side of xnu's bsd/sys/fsevents.h
#define FSE_CREATE_FILE         0
#define FSE_DELETE              1
#define FSE_STAT_CHANGED        2
#define FSE_RENAME              3
#define FSE_CONTENT_MODIFIED    4
#define FSE_EXCHANGE            5
#define FSE_FINDER_INFO_CHANGED 6
#define FSE_CREATE_DIR          7
#define FSE_CHOWN               8
#define FSE_XATTR_MODIFIED      9
#define FSE_XATTR_REMOVED       10
#define FSE_CLONE               14
#define FSE_MAX_EVENTS          16
#define FSE_EVENTS_DROPPED      999
#define FSE_TYPE_MASK           0x0fff
#define FSE_REPORT              1
#define FSE_ARG_STRING          0x0002
#define FSE_ARG_INO             0x0007
#define FSE_ARG_DEV             0x0009
#define FSE_ARG_MODE            0x000a
#define FSE_ARG_DONE            0xb33f
#define FSE_MODE_HLINK          (1U << 31)
#define FSE_MODE_LAST_HLINK     (1U << 30)
#define FSE_MODE_CLONE          (1U << 27)

typedef struct fsevent_clone_args {
	int8_t  *event_list;
	int32_t  num_events;
	int32_t  event_queue_depth;
	int32_t *fd;
} fsevent_clone_args;

#define FSEVENTS_CLONE          _IOW('s', 1, fsevent_clone_args)
#define FSEVENTS_GET_CURRENT_ID _IOR('s', 103, uint64_t)

#define MAX_EXCLUSIONS 8
#define KQ_MAX_WATCHES 4096

// one entry of a watched directory, what a rescan compares against
struct dent {
	char *name;
	ino_t ino;
	mode_t type;
};

// a kqueue vnode watch, on a directory (with its last listing) or a file
struct watch {
	int fd;
	char *path;
	int isdir;
	struct dent *ents;
	int nents;
};

struct pending {
	char *path;
	FSEventStreamEventFlags flags;
	uint64_t ino;
};

struct __FSEventStream {
	_Atomic int refcount;
	CFAllocatorRef allocator;
	FSEventStreamCallback callback;
	FSEventStreamContext context;
	CFArrayRef paths;
	char **roots;
	int nroots;
	char *excl[MAX_EXCLUSIONS];
	int nexcl;
	dev_t device;
	FSEventStreamEventId since;
	_Atomic FSEventStreamEventId latest;
	CFTimeInterval latency;
	FSEventStreamCreateFlags flags;

	// every change of state runs on this private serial queue
	dispatch_queue_t q;
	dispatch_queue_t userq;
	CFRunLoopRef rl;
	CFMutableArrayRef modes;
	_Atomic int running;
	int invalidated;

	int fd;
	int kernel;
	dispatch_source_t src;
	struct watch **watches;
	int nwatches;
	int dropped;

	struct pending *pend;
	size_t npend, cappend;
	int flush_scheduled;
	uint64_t last_delivery;
};

static _Atomic FSEventStreamEventId gEventId;
static dispatch_once_t gEventIdOnce;

// ids keep growing across processes as Apple's do, the kernel's counter seeds them
static void
event_id_init(void *unused)
{
	uint64_t id = 0;
	int dfd = open("/dev/fsevents", O_RDONLY | O_CLOEXEC);

	(void)unused;
	if (dfd >= 0) {
		int8_t list[FSE_MAX_EVENTS];
		int32_t cfd = -1;
		fsevent_clone_args args = { list, FSE_MAX_EVENTS, 1, &cfd };

		memset(list, 0, sizeof(list));
		if (ioctl(dfd, FSEVENTS_CLONE, &args) == 0 && cfd >= 0) {
			(void)ioctl(cfd, FSEVENTS_GET_CURRENT_ID, &id);
			close(cfd);
		}
		close(dfd);
	}
	if (id == 0)
		id = (uint64_t)time(NULL) << 16;
	atomic_store(&gEventId, id);
}

static FSEventStreamEventId
next_event_id(void)
{
	dispatch_once_f(&gEventIdOnce, NULL, event_id_init);
	return atomic_fetch_add(&gEventId, 1) + 1;
}

FSEventStreamEventId
FSEventsGetCurrentEventId(void)
{
	dispatch_once_f(&gEventIdOnce, NULL, event_id_init);
	return atomic_load(&gEventId);
}

static uint64_t
now_ns(void)
{
	return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

static char *
cfstring_path(CFStringRef s)
{
	char buf[PATH_MAX];

	if (s == NULL || CFGetTypeID(s) != CFStringGetTypeID() ||
	    !CFStringGetFileSystemRepresentation(s, buf, sizeof(buf)))
		return NULL;
	return strdup(buf);
}

// the canonical form events carry, a path that does not exist yet is kept as given
static char *
canonical(const char *path)
{
	char buf[PATH_MAX];
	size_t n;

	if (realpath(path, buf) == NULL)
		strlcpy(buf, path, sizeof(buf));
	n = strlen(buf);
	while (n > 1 && buf[n - 1] == '/')
		buf[--n] = '\0';
	return strdup(buf);
}

static int
under(const char *path, const char *root)
{
	size_t n = strlen(root);

	if (strcmp(root, "/") == 0)
		return path[0] == '/';
	return strncmp(path, root, n) == 0 && (path[n] == '/' || path[n] == '\0');
}

static int
watched(FSEventStreamRef s, const char *path)
{
	int in = 0;

	for (int i = 0; i < s->nroots && !in; i++)
		in = under(path, s->roots[i]);
	for (int i = 0; i < s->nexcl && in; i++)
		in = !under(path, s->excl[i]);
	return in;
}

static void
push(FSEventStreamRef s, const char *path, FSEventStreamEventFlags flags, uint64_t ino)
{
	// one entry per path in a batch, the flags of repeated changes are merged as Apple coalesces them
	for (size_t i = 0; i < s->npend; i++) {
		if (strcmp(s->pend[i].path, path) == 0) {
			s->pend[i].flags |= flags;
			if (ino)
				s->pend[i].ino = ino;
			return;
		}
	}
	if (s->npend == s->cappend) {
		size_t cap = s->cappend ? s->cappend * 2 : 32;
		struct pending *p = realloc(s->pend, cap * sizeof(*p));

		if (p == NULL)
			return;
		s->pend = p;
		s->cappend = cap;
	}
	s->pend[s->npend].path = strdup(path);
	s->pend[s->npend].flags = flags;
	s->pend[s->npend].ino = ino;
	if (s->pend[s->npend].path)
		s->npend++;
}

// file events report the item itself, the default reports the directory that holds it
static void
emit_item(FSEventStreamRef s, const char *path, FSEventStreamEventFlags flags, uint64_t ino)
{
	char dir[PATH_MAX];
	char *slash;

	if (!watched(s, path))
		return;
	if (s->flags & kFSEventStreamCreateFlagFileEvents) {
		push(s, path, flags, ino);
		return;
	}
	strlcpy(dir, path, sizeof(dir));
	slash = strrchr(dir, '/');
	if (slash == NULL)
		return;
	slash[1] = '\0';
	push(s, dir, kFSEventStreamEventFlagNone, 0);
}

// MustScanSubDirs and friends name a watched root, with the trailing slash of a directory event
static void
emit_root(FSEventStreamRef s, const char *root, FSEventStreamEventFlags flags)
{
	char p[PATH_MAX];

	strlcpy(p, root, sizeof(p));
	if (!(s->flags & kFSEventStreamCreateFlagFileEvents) && strcmp(p, "/") != 0 &&
	    !(flags & kFSEventStreamEventFlagRootChanged))
		strlcat(p, "/", sizeof(p));
	push(s, p, flags, 0);
}

struct batch {
	FSEventStreamRef s;
	size_t n;
	char **paths;
	FSEventStreamEventFlags *flags;
	FSEventStreamEventId *ids;
	uint64_t *inos;
};

static void
batch_free(struct batch *b)
{
	for (size_t i = 0; i < b->n; i++)
		free(b->paths[i]);
	free(b->paths);
	free(b->flags);
	free(b->ids);
	free(b->inos);
	free(b);
}

// takes the pending events and gives them ids, runs on the stream's queue
static struct batch *
take_batch(FSEventStreamRef s)
{
	struct batch *b;

	if (s->npend == 0)
		return NULL;
	b = calloc(1, sizeof(*b));
	if (b == NULL)
		return NULL;
	b->s = s;
	b->n = s->npend;
	b->paths = calloc(b->n, sizeof(char *));
	b->flags = calloc(b->n, sizeof(FSEventStreamEventFlags));
	b->ids = calloc(b->n, sizeof(FSEventStreamEventId));
	b->inos = calloc(b->n, sizeof(uint64_t));
	if (!b->paths || !b->flags || !b->ids || !b->inos) {
		b->n = 0;
		batch_free(b);
		return NULL;
	}
	for (size_t i = 0; i < b->n; i++) {
		b->paths[i] = s->pend[i].path;
		b->flags[i] = s->pend[i].flags;
		b->inos[i] = s->pend[i].ino;
		b->ids[i] = next_event_id();
	}
	s->npend = 0;
	s->last_delivery = now_ns();
	return b;
}

static void
invoke(struct batch *b)
{
	FSEventStreamRef s = b->s;
	void *paths = b->paths;
	CFMutableArrayRef cf = NULL;

	if (s->flags & kFSEventStreamCreateFlagUseCFTypes) {
		cf = CFArrayCreateMutable(kCFAllocatorDefault, (CFIndex)b->n, &kCFTypeArrayCallBacks);
		for (size_t i = 0; cf && i < b->n; i++) {
			CFStringRef str = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, b->paths[i]);

			if (str == NULL)
				str = CFSTR("");
			else if (s->flags & kFSEventStreamCreateFlagUseExtendedData) {
				CFMutableDictionaryRef d = CFDictionaryCreateMutable(kCFAllocatorDefault, 2,
				    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

				CFDictionarySetValue(d, kFSEventStreamEventExtendedDataPathKey, str);
				if (b->inos[i]) {
					CFNumberRef n = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &b->inos[i]);

					CFDictionarySetValue(d, kFSEventStreamEventExtendedFileIDKey, n);
					CFRelease(n);
				}
				CFRelease(str);
				CFArrayAppendValue(cf, d);
				CFRelease(d);
				continue;
			}
			CFArrayAppendValue(cf, str);
			CFRelease(str);
		}
		paths = cf;
	}
	if (atomic_load(&s->running) && b->n)
		s->callback(s, s->context.info, b->n, paths, b->flags, b->ids);
	atomic_store(&s->latest, b->ids[b->n - 1]);
	if (cf)
		CFRelease(cf);
}

// hands a batch to the client's queue or run loop, the stream stays alive until it ran
static void
dispatch_batch(FSEventStreamRef s, struct batch *b)
{
	if (b == NULL)
		return;
	FSEventStreamRetain(s);
	if (s->userq) {
		dispatch_async(s->userq, ^{
			invoke(b);
			batch_free(b);
			FSEventStreamRelease(s);
		});
	} else if (s->rl) {
		CFRunLoopPerformBlock(s->rl, s->modes, ^{
			invoke(b);
			batch_free(b);
			FSEventStreamRelease(s);
		});
		CFRunLoopWakeUp(s->rl);
	} else {
		batch_free(b);
		FSEventStreamRelease(s);
	}
}

static void
flush_now(FSEventStreamRef s)
{
	s->flush_scheduled = 0;
	if (atomic_load(&s->running))
		dispatch_batch(s, take_batch(s));
}

// latency is measured from the first event of a batch, NoDefer sends a lone event at once
static void
schedule_flush(FSEventStreamRef s)
{
	uint64_t lat = (uint64_t)(s->latency * NSEC_PER_SEC);

	if (s->npend == 0 || s->flush_scheduled)
		return;
	if ((s->flags & kFSEventStreamCreateFlagNoDefer) && now_ns() - s->last_delivery >= lat) {
		flush_now(s);
		return;
	}
	s->flush_scheduled = 1;
	FSEventStreamRetain(s);
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)lat), s->q, ^{
		flush_now(s);
		FSEventStreamRelease(s);
	});
}

static FSEventStreamEventFlags
mode_flags(int32_t mode)
{
	FSEventStreamEventFlags f;

	switch (mode & S_IFMT) {
	case S_IFDIR:
		f = kFSEventStreamEventFlagItemIsDir;
		break;
	case S_IFLNK:
		f = kFSEventStreamEventFlagItemIsSymlink;
		break;
	default:
		f = kFSEventStreamEventFlagItemIsFile;
		break;
	}
	if ((uint32_t)mode & FSE_MODE_HLINK)
		f |= kFSEventStreamEventFlagItemIsHardlink;
	if ((uint32_t)mode & FSE_MODE_LAST_HLINK)
		f |= kFSEventStreamEventFlagItemIsLastHardlink;
	return f;
}

// a delete or rename at a watched root or above it is what WatchRoot reports
static void
check_root(FSEventStreamRef s, const char *path)
{
	if (!(s->flags & kFSEventStreamCreateFlagWatchRoot) || path == NULL)
		return;
	for (int i = 0; i < s->nroots; i++) {
		if (under(s->roots[i], path))
			emit_root(s, s->roots[i], kFSEventStreamEventFlagRootChanged);
	}
}

static void
kernel_event(FSEventStreamRef s, int type, pid_t pid, const char *p1, const char *p2, int32_t mode,
    uint64_t ino)
{
	FSEventStreamEventFlags f = mode_flags(mode);

	if (type == FSE_EVENTS_DROPPED) {
		for (int i = 0; i < s->nroots; i++)
			emit_root(s, s->roots[i], kFSEventStreamEventFlagMustScanSubDirs |
			    kFSEventStreamEventFlagKernelDropped);
		return;
	}
	if (p1 == NULL)
		return;
	if (pid == getpid()) {
		if (s->flags & kFSEventStreamCreateFlagIgnoreSelf)
			return;
		if (s->flags & kFSEventStreamCreateFlagMarkSelf)
			f |= kFSEventStreamEventFlagOwnEvent;
	}
	switch (type) {
	case FSE_CREATE_FILE:
	case FSE_CREATE_DIR:
		f |= kFSEventStreamEventFlagItemCreated;
		break;
	case FSE_DELETE:
		f |= kFSEventStreamEventFlagItemRemoved;
		check_root(s, p1);
		break;
	case FSE_STAT_CHANGED:
		f |= kFSEventStreamEventFlagItemInodeMetaMod;
		break;
	case FSE_RENAME:
	case FSE_EXCHANGE:
		f |= kFSEventStreamEventFlagItemRenamed;
		check_root(s, p1);
		break;
	case FSE_CONTENT_MODIFIED:
		f |= kFSEventStreamEventFlagItemModified;
		break;
	case FSE_FINDER_INFO_CHANGED:
		f |= kFSEventStreamEventFlagItemFinderInfoMod;
		break;
	case FSE_CHOWN:
		f |= kFSEventStreamEventFlagItemChangeOwner;
		break;
	case FSE_XATTR_MODIFIED:
	case FSE_XATTR_REMOVED:
		f |= kFSEventStreamEventFlagItemXattrMod;
		break;
	case FSE_CLONE:
		f |= kFSEventStreamEventFlagItemCloned;
		break;
	default:
		return;
	}
	emit_item(s, p1, f, ino);
	// a rename or clone names its destination second
	if (p2 && (type == FSE_RENAME || type == FSE_EXCHANGE || type == FSE_CLONE))
		emit_item(s, p2, f, 0);
}

// each record is type and pid, then (type, length, data) arguments up to FSE_ARG_DONE
static void
kernel_read(FSEventStreamRef s)
{
	// streams read on their own queues at the same time, so each read has its own buffer
	char *buf = malloc(64 * 1024);
	ssize_t n = buf ? read(s->fd, buf, 64 * 1024) : -1;
	size_t off = 0;

	if (n <= 0) {
		free(buf);
		return;
	}
	while (off + 8 <= (size_t)n) {
		int32_t type, pid, mode = 0;
		const char *p1 = NULL, *p2 = NULL;
		uint64_t ino = 0;

		memcpy(&type, buf + off, 4);
		memcpy(&pid, buf + off + 4, 4);
		off += 8;
		for (;;) {
			uint16_t at, len;

			if (off + 2 > (size_t)n)
				goto out;
			memcpy(&at, buf + off, 2);
			off += 2;
			if (at == FSE_ARG_DONE)
				break;
			if (off + 2 > (size_t)n)
				goto out;
			memcpy(&len, buf + off, 2);
			off += 2;
			if (off + len > (size_t)n)
				goto out;
			if (at == FSE_ARG_STRING && len > 0 && buf[off + len - 1] == '\0') {
				if (p1 == NULL)
					p1 = buf + off;
				else if (p2 == NULL)
					p2 = buf + off;
			} else if (at == FSE_ARG_MODE && len >= 4) {
				memcpy(&mode, buf + off, 4);
			} else if (at == FSE_ARG_INO && len <= sizeof(ino)) {
				memcpy(&ino, buf + off, len);
			}
			off += len;
		}
		kernel_event(s, type & FSE_TYPE_MASK, pid, p1, p2, mode, ino);
	}
out:
	free(buf);
	schedule_flush(s);
}

static int
kernel_start(FSEventStreamRef s)
{
	int8_t list[FSE_MAX_EVENTS];
	int32_t cfd = -1;
	fsevent_clone_args args = { list, FSE_MAX_EVENTS, 4096, &cfd };
	int dfd;

	if (getenv("PD_FSEVENTS_KQUEUE") != NULL)
		return -1;
	dfd = open("/dev/fsevents", O_RDONLY | O_CLOEXEC);
	if (dfd < 0)
		return -1;
	memset(list, FSE_REPORT, sizeof(list));
	if (ioctl(dfd, FSEVENTS_CLONE, &args) != 0 || cfd < 0) {
		close(dfd);
		return -1;
	}
	close(dfd);
	(void)fcntl(cfd, F_SETFD, FD_CLOEXEC);

	// FSEVENTS_DEVICE_FILTER lists devices to skip, so every device is read and the paths decide
	s->fd = cfd;
	s->kernel = 1;
	s->src = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)cfd, 0, s->q);
	dispatch_source_set_event_handler(s->src, ^{
		kernel_read(s);
	});
	dispatch_source_set_cancel_handler(s->src, ^{
		close(cfd);
	});
	dispatch_resume(s->src);
	return 0;
}

// kqueue fallback: directories and files are watched one by one, so it has no history and
// sees only what fits under the watch limit (MustScanSubDirs says so), renames across directories show as remove and create
static int
watch_limit(void)
{
	struct rlimit rl;
	int lim = KQ_MAX_WATCHES;

	if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY &&
	    (rlim_t)lim > rl.rlim_cur / 2)
		lim = (int)(rl.rlim_cur / 2);
	return lim;
}

static int
dent_cmp(const void *a, const void *b)
{
	return strcmp(((const struct dent *)a)->name, ((const struct dent *)b)->name);
}

static void
free_ents(struct dent *ents, int n)
{
	for (int i = 0; i < n; i++)
		free(ents[i].name);
	free(ents);
}

static int
list_dir(const char *path, struct dent **out)
{
	DIR *d = opendir(path);
	struct dirent *e;
	struct dent *ents = NULL;
	int n = 0, cap = 0;

	*out = NULL;
	if (d == NULL)
		return 0;
	while ((e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		if (n == cap) {
			struct dent *p;

			cap = cap ? cap * 2 : 16;
			p = realloc(ents, (size_t)cap * sizeof(*p));
			if (p == NULL)
				break;
			ents = p;
		}
		ents[n].name = strdup(e->d_name);
		ents[n].ino = e->d_ino;
		ents[n].type = e->d_type == DT_DIR ? S_IFDIR : e->d_type == DT_LNK ? S_IFLNK : S_IFREG;
		if (ents[n].name)
			n++;
	}
	closedir(d);
	qsort(ents, (size_t)n, sizeof(*ents), dent_cmp);
	*out = ents;
	return n;
}

static void add_tree(FSEventStreamRef s, const char *path, int isdir);

static struct watch *
add_watch(FSEventStreamRef s, const char *path, int isdir)
{
	struct watch *w, **ws;
	struct kevent kev;
	int fd;

	if (s->nwatches >= watch_limit()) {
		s->dropped = 1;
		return NULL;
	}
	fd = open(path, O_EVTONLY | O_CLOEXEC | (isdir ? O_DIRECTORY : 0));
	if (fd < 0)
		return NULL;
	w = calloc(1, sizeof(*w));
	ws = realloc(s->watches, (size_t)(s->nwatches + 1) * sizeof(*ws));
	if (w == NULL || ws == NULL) {
		free(w);
		close(fd);
		return NULL;
	}
	s->watches = ws;
	w->fd = fd;
	w->path = strdup(path);
	w->isdir = isdir;
	EV_SET(&kev, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR,
	    NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_DELETE | NOTE_RENAME | NOTE_LINK, 0, w);
	if (kevent(s->fd, &kev, 1, NULL, 0, NULL) != 0) {
		close(fd);
		free(w->path);
		free(w);
		return NULL;
	}
	s->watches[s->nwatches++] = w;
	return w;
}

static void
drop_watch(FSEventStreamRef s, int i)
{
	struct watch *w = s->watches[i];

	close(w->fd);
	free_ents(w->ents, w->nents);
	free(w->path);
	free(w);
	s->watches[i] = s->watches[--s->nwatches];
}

// a removed directory takes the watches of everything below it along
static void
drop_tree(FSEventStreamRef s, const char *path)
{
	for (int i = s->nwatches - 1; i >= 0; i--) {
		if (i < s->nwatches && under(s->watches[i]->path, path))
			drop_watch(s, i);
	}
}

static void
child_path(char *out, size_t size, const char *dir, const char *name)
{
	snprintf(out, size, "%s%s%s", dir, strcmp(dir, "/") == 0 ? "" : "/", name);
}

static void
add_tree(FSEventStreamRef s, const char *path, int isdir)
{
	struct watch *w = add_watch(s, path, isdir);
	char child[PATH_MAX];

	if (w == NULL || !isdir)
		return;
	w->nents = list_dir(path, &w->ents);
	for (int i = 0; i < w->nents; i++) {
		child_path(child, sizeof(child), path, w->ents[i].name);
		if (!watched(s, child) || w->ents[i].type == S_IFLNK)
			continue;
		add_tree(s, child, w->ents[i].type == S_IFDIR);
	}
}

static FSEventStreamEventFlags
type_flags(mode_t type)
{
	return type == S_IFDIR ? kFSEventStreamEventFlagItemIsDir :
	    type == S_IFLNK ? kFSEventStreamEventFlagItemIsSymlink : kFSEventStreamEventFlagItemIsFile;
}

// a changed directory is listed again, the difference is what was created, removed or renamed
static void
rescan(FSEventStreamRef s, struct watch *w)
{
	struct dent *now;
	int n = list_dir(w->path, &now), i = 0, j = 0;
	char child[PATH_MAX];

	while (i < w->nents || j < n) {
		int c = i >= w->nents ? 1 : j >= n ? -1 : strcmp(w->ents[i].name, now[j].name);

		if (c == 0 && w->ents[i].ino == now[j].ino) {
			i++;
			j++;
			continue;
		}
		if (c <= 0) {
			struct dent *old = &w->ents[i++];
			FSEventStreamEventFlags f = kFSEventStreamEventFlagItemRemoved;

			// the same inode under a new name in this directory is a rename
			for (int k = 0; k < n; k++)
				if (now[k].ino == old->ino && strcmp(now[k].name, old->name))
					f = kFSEventStreamEventFlagItemRenamed;
			child_path(child, sizeof(child), w->path, old->name);
			emit_item(s, child, f | type_flags(old->type), old->ino);
			drop_tree(s, child);
		}
		if (c >= 0) {
			struct dent *cur = &now[j++];
			FSEventStreamEventFlags f = kFSEventStreamEventFlagItemCreated;

			for (int k = 0; k < w->nents; k++)
				if (w->ents[k].ino == cur->ino && strcmp(w->ents[k].name, cur->name))
					f = kFSEventStreamEventFlagItemRenamed;
			child_path(child, sizeof(child), w->path, cur->name);
			emit_item(s, child, f | type_flags(cur->type), cur->ino);
			if (cur->type != S_IFLNK && watched(s, child))
				add_tree(s, child, cur->type == S_IFDIR);
		}
	}
	free_ents(w->ents, w->nents);
	w->ents = now;
	w->nents = n;
}

static int
is_root(FSEventStreamRef s, const char *path)
{
	for (int i = 0; i < s->nroots; i++)
		if (strcmp(s->roots[i], path) == 0)
			return 1;
	return 0;
}

static void
kqueue_read(FSEventStreamRef s)
{
	struct kevent evs[64];
	struct timespec zero = { 0, 0 };
	int n = kevent(s->fd, NULL, 0, evs, 64, &zero);

	for (int i = 0; i < n; i++) {
		struct watch *w = evs[i].udata;
		int known = 0;

		// the watch may have gone with a directory removed earlier in this batch
		for (int k = 0; k < s->nwatches && !known; k++)
			known = s->watches[k] == w;
		if (!known)
			continue;
		if ((evs[i].fflags & (NOTE_DELETE | NOTE_RENAME)) && is_root(s, w->path)) {
			check_root(s, w->path);
			continue;
		}
		if (w->isdir && (evs[i].fflags & (NOTE_WRITE | NOTE_LINK)))
			rescan(s, w);
		else if (!w->isdir && (evs[i].fflags & (NOTE_WRITE | NOTE_EXTEND)))
			emit_item(s, w->path, kFSEventStreamEventFlagItemModified | kFSEventStreamEventFlagItemIsFile, 0);
		if (evs[i].fflags & NOTE_ATTRIB)
			emit_item(s, w->path, kFSEventStreamEventFlagItemInodeMetaMod | type_flags(w->isdir ? S_IFDIR : S_IFREG), 0);
	}
	schedule_flush(s);
}

static int
kqueue_start(FSEventStreamRef s)
{
	int kq = kqueue();

	if (kq < 0)
		return -1;
	s->fd = kq;
	s->kernel = 0;
	s->dropped = 0;
	for (int i = 0; i < s->nroots; i++) {
		struct stat st;

		if (stat(s->roots[i], &st) == 0)
			add_tree(s, s->roots[i], S_ISDIR(st.st_mode));
	}
	if (s->dropped) {
		for (int i = 0; i < s->nroots; i++)
			emit_root(s, s->roots[i], kFSEventStreamEventFlagMustScanSubDirs |
			    kFSEventStreamEventFlagUserDropped);
	}
	s->src = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)kq, 0, s->q);
	dispatch_source_set_event_handler(s->src, ^{
		kqueue_read(s);
	});
	dispatch_source_set_cancel_handler(s->src, ^{
		close(kq);
	});
	dispatch_resume(s->src);
	return 0;
}

static void
backend_stop(FSEventStreamRef s)
{
	if (s->src) {
		dispatch_source_cancel(s->src);
		dispatch_release(s->src);
		s->src = NULL;
	}
	while (s->nwatches)
		drop_watch(s, s->nwatches - 1);
	free(s->watches);
	s->watches = NULL;
	for (size_t i = 0; i < s->npend; i++)
		free(s->pend[i].path);
	s->npend = 0;
	s->fd = -1;
}

FSEventStreamRef
FSEventStreamCreate(CFAllocatorRef allocator, FSEventStreamCallback callback,
    FSEventStreamContext *context, CFArrayRef pathsToWatch, FSEventStreamEventId sinceWhen,
    CFTimeInterval latency, FSEventStreamCreateFlags flags)
{
	FSEventStreamRef s;
	CFIndex n;

	if (callback == NULL || pathsToWatch == NULL || CFArrayGetCount(pathsToWatch) == 0)
		return NULL;
	n = CFArrayGetCount(pathsToWatch);
	s = calloc(1, sizeof(*s));
	if (s == NULL)
		return NULL;
	s->roots = calloc((size_t)n, sizeof(char *));
	if (s->roots == NULL) {
		free(s);
		return NULL;
	}
	for (CFIndex i = 0; i < n; i++) {
		char *p = cfstring_path(CFArrayGetValueAtIndex(pathsToWatch, i));

		if (p) {
			s->roots[s->nroots++] = canonical(p);
			free(p);
		}
	}
	atomic_store(&s->refcount, 1);
	s->allocator = allocator ? CFRetain(allocator) : NULL;
	s->callback = callback;
	if (context) {
		s->context = *context;
		if (s->context.retain && s->context.info)
			s->context.info = (void *)s->context.retain(s->context.info);
	}
	s->paths = CFArrayCreateCopy(kCFAllocatorDefault, pathsToWatch);
	s->since = sinceWhen;
	atomic_store(&s->latest, sinceWhen == kFSEventStreamEventIdSinceNow ? FSEventsGetCurrentEventId() : sinceWhen);
	s->latency = latency < 0 ? 0 : latency;
	s->flags = flags;
	s->fd = -1;
	s->q = dispatch_queue_create("org.puredarwin.FSEventStream", DISPATCH_QUEUE_SERIAL);
	return s;
}

// the paths are relative to the device's mount point
FSEventStreamRef
FSEventStreamCreateRelativeToDevice(CFAllocatorRef allocator, FSEventStreamCallback callback,
    FSEventStreamContext *context, dev_t deviceToWatch, CFArrayRef pathsToWatchRelativeToDevice,
    FSEventStreamEventId sinceWhen, CFTimeInterval latency, FSEventStreamCreateFlags flags)
{
	struct statfs *mnts;
	int count = getmntinfo(&mnts, MNT_NOWAIT);
	CFMutableArrayRef abs;
	FSEventStreamRef s = NULL;
	const char *mount = NULL;

	for (int i = 0; i < count && mount == NULL; i++) {
		struct stat st;

		if (stat(mnts[i].f_mntonname, &st) == 0 && st.st_dev == deviceToWatch)
			mount = mnts[i].f_mntonname;
	}
	if (mount == NULL || pathsToWatchRelativeToDevice == NULL)
		return NULL;
	abs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	for (CFIndex i = 0; i < CFArrayGetCount(pathsToWatchRelativeToDevice); i++) {
		char *rel = cfstring_path(CFArrayGetValueAtIndex(pathsToWatchRelativeToDevice, i));
		char full[PATH_MAX];
		CFStringRef str;

		if (rel == NULL)
			continue;
		snprintf(full, sizeof(full), "%s/%s", strcmp(mount, "/") ? mount : "", rel[0] == '/' ? rel + 1 : rel);
		free(rel);
		str = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, full);
		if (str) {
			CFArrayAppendValue(abs, str);
			CFRelease(str);
		}
	}
	s = FSEventStreamCreate(allocator, callback, context, abs, sinceWhen, latency, flags);
	CFRelease(abs);
	if (s)
		s->device = deviceToWatch;
	return s;
}

void
FSEventStreamRetain(FSEventStreamRef s)
{
	if (s)
		atomic_fetch_add(&s->refcount, 1);
}

static void
destroy(void *ctx)
{
	FSEventStreamRef s = ctx;

	if (s->src || s->rl || s->userq)
		FSEventStreamInvalidate(s);
	if (s->context.release && s->context.info)
		s->context.release(s->context.info);
	for (int i = 0; i < s->nroots; i++)
		free(s->roots[i]);
	free(s->roots);
	for (int i = 0; i < s->nexcl; i++)
		free(s->excl[i]);
	free(s->pend);
	if (s->paths)
		CFRelease(s->paths);
	if (s->allocator)
		CFRelease(s->allocator);
	dispatch_release(s->q);
	free(s);
}

// the last reference can go on the stream's own queue (a pending flush), so teardown runs elsewhere
void
FSEventStreamRelease(FSEventStreamRef s)
{
	if (s == NULL || atomic_fetch_sub(&s->refcount, 1) != 1)
		return;
	dispatch_async_f(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), s, destroy);
}

void
FSEventStreamScheduleWithRunLoop(FSEventStreamRef s, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
	if (s == NULL || runLoop == NULL || runLoopMode == NULL)
		return;
	dispatch_sync(s->q, ^{
		if (s->rl == NULL) {
			s->rl = (CFRunLoopRef)CFRetain(runLoop);
			s->modes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		}
		if (s->rl == runLoop &&
		    !CFArrayContainsValue(s->modes, CFRangeMake(0, CFArrayGetCount(s->modes)), runLoopMode))
			CFArrayAppendValue(s->modes, runLoopMode);
	});
}

void
FSEventStreamUnscheduleFromRunLoop(FSEventStreamRef s, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
	if (s == NULL)
		return;
	dispatch_sync(s->q, ^{
		CFIndex i;

		if (s->rl != runLoop || s->modes == NULL)
			return;
		i = CFArrayGetFirstIndexOfValue(s->modes, CFRangeMake(0, CFArrayGetCount(s->modes)), runLoopMode);
		if (i != kCFNotFound)
			CFArrayRemoveValueAtIndex(s->modes, i);
		if (CFArrayGetCount(s->modes) == 0) {
			CFRelease(s->modes);
			CFRelease(s->rl);
			s->modes = NULL;
			s->rl = NULL;
		}
	});
}

void
FSEventStreamSetDispatchQueue(FSEventStreamRef s, dispatch_queue_t q)
{
	if (s == NULL)
		return;
	dispatch_sync(s->q, ^{
		if (q)
			dispatch_retain(q);
		if (s->userq)
			dispatch_release(s->userq);
		s->userq = q;
	});
}

Boolean
FSEventStreamStart(FSEventStreamRef s)
{
	__block Boolean ok = FALSE;

	if (s == NULL)
		return FALSE;
	dispatch_sync(s->q, ^{
		if (s->invalidated || (s->userq == NULL && s->rl == NULL))
			return;
		if (s->src) {
			ok = TRUE;
			return;
		}
		if (kernel_start(s) != 0 && kqueue_start(s) != 0)
			return;
		atomic_store(&s->running, 1);
		// there is no stored history, a replay request ends at once with HistoryDone
		if (s->since != kFSEventStreamEventIdSinceNow && s->nroots)
			push(s, s->roots[0], kFSEventStreamEventFlagHistoryDone, 0);
		schedule_flush(s);
		ok = TRUE;
	});
	return ok;
}

void
FSEventStreamStop(FSEventStreamRef s)
{
	if (s == NULL)
		return;
	atomic_store(&s->running, 0);
	dispatch_sync(s->q, ^{
		backend_stop(s);
	});
}

void
FSEventStreamInvalidate(FSEventStreamRef s)
{
	if (s == NULL)
		return;
	FSEventStreamStop(s);
	dispatch_sync(s->q, ^{
		if (s->userq)
			dispatch_release(s->userq);
		s->userq = NULL;
		if (s->rl) {
			CFRelease(s->modes);
			CFRelease(s->rl);
		}
		s->modes = NULL;
		s->rl = NULL;
		s->invalidated = 1;
	});
}

FSEventStreamEventId
FSEventStreamFlushAsync(FSEventStreamRef s)
{
	if (s == NULL)
		return 0;
	FSEventStreamRetain(s);
	dispatch_async(s->q, ^{
		flush_now(s);
		FSEventStreamRelease(s);
	});
	return FSEventsGetCurrentEventId();
}

// what is pending is delivered on the calling thread before this returns
void
FSEventStreamFlushSync(FSEventStreamRef s)
{
	__block struct batch *b = NULL;

	if (s == NULL)
		return;
	// a read of the kernel's fd sleeps when it is empty, only kqueue can be polled here
	dispatch_sync(s->q, ^{
		if (s->src && !s->kernel)
			kqueue_read(s);
		b = take_batch(s);
	});
	if (b) {
		invoke(b);
		batch_free(b);
	}
}

FSEventStreamEventId
FSEventStreamGetLatestEventId(ConstFSEventStreamRef s)
{
	return s ? atomic_load(&((FSEventStreamRef)s)->latest) : 0;
}

dev_t
FSEventStreamGetDeviceBeingWatched(ConstFSEventStreamRef s)
{
	return s ? s->device : 0;
}

CFArrayRef
FSEventStreamCopyPathsBeingWatched(ConstFSEventStreamRef s)
{
	return s && s->paths ? CFArrayCreateCopy(kCFAllocatorDefault, s->paths) : NULL;
}

Boolean
FSEventStreamSetExclusionPaths(FSEventStreamRef s, CFArrayRef pathsToExclude)
{
	__block Boolean ok = TRUE;

	if (s == NULL || pathsToExclude == NULL || CFArrayGetCount(pathsToExclude) > MAX_EXCLUSIONS)
		return FALSE;
	dispatch_sync(s->q, ^{
		for (int i = 0; i < s->nexcl; i++)
			free(s->excl[i]);
		s->nexcl = 0;
		for (CFIndex i = 0; i < CFArrayGetCount(pathsToExclude); i++) {
			char *p = cfstring_path(CFArrayGetValueAtIndex(pathsToExclude, i));

			if (p == NULL) {
				ok = FALSE;
				continue;
			}
			s->excl[s->nexcl++] = canonical(p);
			free(p);
		}
	});
	return ok;
}

CFStringRef
FSEventStreamCopyDescription(ConstFSEventStreamRef s)
{
	CFMutableStringRef d;

	if (s == NULL)
		return NULL;
	d = CFStringCreateMutable(kCFAllocatorDefault, 0);
	CFStringAppendFormat(d, NULL, CFSTR("FSEventStreamRef @ %p:\n   flags = 0x%x\n   latency = %f\n"
	    "   since = %llu\n   latest = %llu\n   backend = %s\n   paths = %@\n"), s, (unsigned)s->flags,
	    s->latency, (unsigned long long)s->since, (unsigned long long)atomic_load(&((FSEventStreamRef)s)->latest),
	    s->src == NULL ? "stopped" : s->kernel ? "/dev/fsevents" : "kqueue", s->paths);
	return d;
}

void
FSEventStreamShow(ConstFSEventStreamRef s)
{
	CFStringRef d = FSEventStreamCopyDescription(s);
	char buf[2048];

	if (d == NULL)
		return;
	if (CFStringGetCString(d, buf, sizeof(buf), kCFStringEncodingUTF8))
		fputs(buf, stderr);
	CFRelease(d);
}

// there is no per-volume event store, so no volume UUIDs and nothing to purge
CFUUIDRef
FSEventsCopyUUIDForDevice(dev_t dev)
{
	(void)dev;
	return NULL;
}

FSEventStreamEventId
FSEventsGetLastEventIdForDeviceBeforeTime(dev_t dev, CFAbsoluteTime time)
{
	(void)dev;
	(void)time;
	return FSEventsGetCurrentEventId();
}

Boolean
FSEventsPurgeEventsForDeviceUpToEventId(dev_t dev, FSEventStreamEventId eventId)
{
	(void)dev;
	(void)eventId;
	return FALSE;
}
