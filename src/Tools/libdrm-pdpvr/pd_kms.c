// libdrm-pdpvr's kms half: one connector, encoder, crtc and primary plane over PDSun60iDisplay, enough atomic modesetting for mesa's VK_KHR_display,
// page flip events through the display file's pipe, and prime descriptors that carry a kernel export token

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include "pd_pvr_internal.h"
#include "pd_pvr_uapi.h"

#define PRIME_MAGIC		0x42445044u	// "PDDB"
#define MAX_PRIMES		128
#define MAX_FBS			64
#define MAX_BLOBS		32

// object and property ids, fixed: there is one of each
#define ID_CRTC			0x31
#define ID_ENCODER		0x32
#define ID_CONNECTOR		0x33
#define ID_PLANE		0x34
#define ID_MODE_BLOB		0x40
#define FIRST_BLOB		0x80
#define FIRST_FB		0x1000

enum {
	P_CONN_CRTC_ID = 0x101, P_DPMS,
	P_ACTIVE = 0x110, P_MODE_ID,
	P_TYPE = 0x120, P_FB_ID, P_PLANE_CRTC_ID, P_CRTC_X, P_CRTC_Y, P_CRTC_W, P_CRTC_H,
	P_SRC_X, P_SRC_Y, P_SRC_W, P_SRC_H,
};

struct prop_def {
	uint32_t id, flags;
	const char *name;
	uint64_t min, max;
};

static const struct prop_def props[] = {
	{ P_CONN_CRTC_ID, DRM_MODE_PROP_OBJECT | DRM_MODE_PROP_ATOMIC, "CRTC_ID", 0, 0 },
	{ P_DPMS, DRM_MODE_PROP_ENUM, "DPMS", 0, 0 },
	{ P_ACTIVE, DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC, "ACTIVE", 0, 1 },
	{ P_MODE_ID, DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC, "MODE_ID", 0, 0 },
	{ P_TYPE, DRM_MODE_PROP_ENUM | DRM_MODE_PROP_IMMUTABLE, "type", 0, 0 },
	{ P_FB_ID, DRM_MODE_PROP_OBJECT | DRM_MODE_PROP_ATOMIC, "FB_ID", 0, 0 },
	{ P_PLANE_CRTC_ID, DRM_MODE_PROP_OBJECT | DRM_MODE_PROP_ATOMIC, "CRTC_ID", 0, 0 },
	{ P_CRTC_X, DRM_MODE_PROP_SIGNED_RANGE | DRM_MODE_PROP_ATOMIC, "CRTC_X", 0, 0x7fffffff },
	{ P_CRTC_Y, DRM_MODE_PROP_SIGNED_RANGE | DRM_MODE_PROP_ATOMIC, "CRTC_Y", 0, 0x7fffffff },
	{ P_CRTC_W, DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC, "CRTC_W", 0, 0x7fffffff },
	{ P_CRTC_H, DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC, "CRTC_H", 0, 0x7fffffff },
	{ P_SRC_X, DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC, "SRC_X", 0, 0xffffffff },
	{ P_SRC_Y, DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC, "SRC_Y", 0, 0xffffffff },
	{ P_SRC_W, DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC, "SRC_W", 0, 0xffffffff },
	{ P_SRC_H, DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC, "SRC_H", 0, 0xffffffff },
};

struct prime_hdr {
	uint32_t magic, version;
	uint64_t token, size;
};

struct prime_fd {
	int fd;
	dev_t dev;
	ino_t ino;
	uint64_t token;
	io_connect_t conn;
};

struct kms_fb {
	uint32_t id, handle, width, height, format, pitch, offset;
	int fd;
};

struct kms_blob {
	uint32_t id;
	size_t len;
	void *data;
};

struct atomic_item {
	uint32_t obj, prop;
	uint64_t value;
};

struct _drmModeAtomicReq {
	uint32_t cursor, size;
	struct atomic_item *items;
};

// a nonblocking flip handed to the flip thread
struct flip_req {
	int fd, evfd;
	io_connect_t conn;
	uint32_t handle, pitch, offset, fb;
	bool restore, event;
	uint64_t user_data;
};

static pthread_mutex_t klock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t kcond = PTHREAD_COND_INITIALIZER;
static struct prime_fd primes[MAX_PRIMES];
static struct kms_fb fbs[MAX_FBS];
static struct kms_blob blobs[MAX_BLOBS];
static uint32_t next_fb = FIRST_FB, next_blob = FIRST_BLOB;
static uint32_t current_fb;
static struct pd_pvr_display_info dinfo;
static bool have_dinfo;
static struct flip_req pending;
static bool flip_busy, flip_thread_up;
static pthread_t flip_thread;

static int
neg_errno(int err)
{
	errno = err;
	return -err;
}


static bool
prime_live(const struct prime_fd *p)
{
	struct stat st;

	return fstat(p->fd, &st) == 0 && st.st_dev == p->dev && st.st_ino == p->ino;
}

// exports whose descriptor is gone give their token back, called with klock held
static void
prime_collect(void)
{
	for (int i = 0; i < MAX_PRIMES; i++) {
		bool shared = false;

		if (primes[i].token == 0 || prime_live(&primes[i]))
			continue;
		for (int j = 0; j < MAX_PRIMES; j++) {
			if (j != i && primes[j].token == primes[i].token && primes[j].conn == primes[i].conn &&
			    prime_live(&primes[j]))
				shared = true;
		}
		if (!shared)
			pd_call(primes[i].conn, PD_PVR_SEL_PRIME_RELEASE, &primes[i].token, 1, NULL, 0);
		memset(&primes[i], 0, sizeof(primes[i]));
	}
}

int
pd_prime_export(io_connect_t conn, uint32_t handle, uint32_t flags, int *prime_fd)
{
	uint64_t in = handle, out[2];
	struct prime_hdr hdr = { PRIME_MAGIC, 1, 0, 0 };
	char path[] = "/tmp/.pd-prime-XXXXXX";
	struct prime_fd *slot = NULL;
	struct stat st;
	int fd;

	pthread_mutex_lock(&klock);
	prime_collect();
	for (int i = 0; i < MAX_PRIMES && slot == NULL; i++) {
		if (primes[i].token == 0)
			slot = &primes[i];
	}
	pthread_mutex_unlock(&klock);
	if (slot == NULL)
		return pd_fail(EMFILE);
	if (pd_call(conn, PD_PVR_SEL_PRIME_EXPORT, &in, 1, out, 2) != 0)
		return -1;

	// a sparse file of the buffer's size, so lseek(SEEK_END) answers as a dma-buf does
	hdr.token = out[0];
	hdr.size = out[1];
	fd = mkstemp(path);
	if (fd < 0)
		return -1;
	unlink(path);
	if (pwrite(fd, &hdr, sizeof(hdr), 0) != (ssize_t)sizeof(hdr) ||
	    ftruncate(fd, hdr.size > sizeof(hdr) ? (off_t)hdr.size : (off_t)sizeof(hdr)) != 0 || fstat(fd, &st) != 0) {
		close(fd);
		return pd_fail(EIO);
	}
	if (flags & DRM_CLOEXEC)
		fcntl(fd, F_SETFD, FD_CLOEXEC);

	pthread_mutex_lock(&klock);
	*slot = (struct prime_fd){ fd, st.st_dev, st.st_ino, hdr.token, conn };
	pthread_mutex_unlock(&klock);
	if (pd_debug())
		fprintf(stderr, "libdrm-pdpvr: export handle %u as fd %d, %llu bytes\n", handle, fd,
		    (unsigned long long)hdr.size);
	*prime_fd = fd;
	return 0;
}

int
pd_prime_import(io_connect_t conn, int prime_fd, uint32_t *handle)
{
	struct prime_hdr hdr;
	uint64_t out[2];

	if (pread(prime_fd, &hdr, sizeof(hdr), 0) != (ssize_t)sizeof(hdr) || hdr.magic != PRIME_MAGIC)
		return pd_fail(EINVAL);
	if (pd_call(conn, PD_PVR_SEL_PRIME_IMPORT, &hdr.token, 1, out, 2) != 0)
		return -1;
	*handle = (uint32_t)out[0];
	pthread_mutex_lock(&klock);
	prime_collect();
	pthread_mutex_unlock(&klock);
	return 0;
}


static io_connect_t
display_conn(int fd, int *evfd)
{
	int kind;
	io_connect_t conn = pd_file_lookup(fd, true, &kind, evfd);

	if (conn == IO_OBJECT_NULL || kind != PD_FILE_DISPLAY) {
		errno = conn == IO_OBJECT_NULL ? EBADF : EOPNOTSUPP;
		return IO_OBJECT_NULL;
	}
	return conn;
}

static bool
display_info(io_connect_t conn)
{
	size_t len = sizeof(dinfo);

	if (have_dinfo)
		return true;
	if (IOConnectCallStructMethod(conn, PD_PVR_SEL_DISPLAY_INFO, NULL, 0, &dinfo, &len) != KERN_SUCCESS)
		return false;
	have_dinfo = true;
	return true;
}

// cea 1080p60 when that is what runs, otherwise blanking stretched to the measured rate
static void
fill_mode(drmModeModeInfo *m)
{
	uint32_t w = dinfo.width, h = dinfo.height;
	uint32_t refresh = dinfo.refresh_mhz ? dinfo.refresh_mhz : 60000;

	memset(m, 0, sizeof(*m));
	m->hdisplay = w;
	m->vdisplay = h;
	if (w == 1920 && h == 1080) {
		m->hsync_start = 2008;
		m->hsync_end = 2052;
		m->htotal = 2200;
		m->vsync_start = 1084;
		m->vsync_end = 1089;
		m->vtotal = 1125;
	} else {
		m->hsync_start = w + 88;
		m->hsync_end = w + 132;
		m->htotal = w + 280;
		m->vsync_start = h + 4;
		m->vsync_end = h + 9;
		m->vtotal = h + 45;
	}
	m->clock = (uint32_t)((uint64_t)m->htotal * m->vtotal * refresh / 1000000);
	m->vrefresh = (refresh + 500) / 1000;
	m->flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;
	m->type = DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DRIVER;
	snprintf(m->name, sizeof(m->name), "%ux%u", w, h);
}

static struct kms_fb *
find_fb(uint32_t id)
{
	for (int i = 0; i < MAX_FBS; i++) {
		if (fbs[i].id == id && id != 0)
			return &fbs[i];
	}
	return NULL;
}

void
pd_kms_forget(int fd)
{
	pthread_mutex_lock(&klock);
	for (int i = 0; i < MAX_FBS; i++) {
		if (fbs[i].fd == fd && fbs[i].id != 0) {
			if (fbs[i].id == current_fb)
				current_fb = 0;
			memset(&fbs[i], 0, sizeof(fbs[i]));
		}
	}
	pthread_mutex_unlock(&klock);
}


static void
send_event(int evfd, uint32_t type, uint64_t user_data, uint64_t seq, uint64_t ns)
{
	struct drm_event_vblank ev;

	memset(&ev, 0, sizeof(ev));
	ev.base.type = type;
	ev.base.length = sizeof(ev);
	ev.user_data = user_data;
	ev.tv_sec = (uint32_t)(ns / 1000000000ULL);
	ev.tv_usec = (uint32_t)(ns % 1000000000ULL / 1000);
	ev.sequence = (uint32_t)seq;
	ev.crtc_id = ID_CRTC;
	if (write(evfd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev) && pd_debug())
		fprintf(stderr, "libdrm-pdpvr: page flip event lost\n");
}

// blocks until the buffer is latched, the kernel waits for the gpu work queued before it first
static int
do_flip(const struct flip_req *r, uint64_t *seq, uint64_t *ns)
{
	uint64_t in[3] = { r->restore ? 0 : r->handle, r->offset, r->pitch }, out[2] = { 0, 0 };

	if (pd_call(r->conn, PD_PVR_SEL_FLIP, in, 3, out, 2) != 0)
		return -1;
	*seq = out[0];
	*ns = out[1];
	return 0;
}

static void *
flip_main(void *arg)
{
	(void)arg;

	pthread_mutex_lock(&klock);
	for (;;) {
		struct flip_req r;
		uint64_t seq = 0, ns = 0;
		int ret;

		while (!flip_busy)
			pthread_cond_wait(&kcond, &klock);
		r = pending;
		pthread_mutex_unlock(&klock);

		ret = do_flip(&r, &seq, &ns);

		pthread_mutex_lock(&klock);
		if (ret == 0)
			current_fb = r.restore ? 0 : r.fb;
		// free before the event: its handler queues the next flip at once
		flip_busy = false;
		pthread_cond_broadcast(&kcond);
		if (r.event)
			send_event(r.evfd, DRM_EVENT_FLIP_COMPLETE, r.user_data, seq, ns);
	}
	return NULL;
}

static int
queue_flip(const struct flip_req *r, bool nonblock)
{
	uint64_t seq, ns;

	pthread_mutex_lock(&klock);
	if (nonblock) {
		if (flip_busy) {
			pthread_mutex_unlock(&klock);
			return neg_errno(EBUSY);
		}
		if (!flip_thread_up) {
			if (pthread_create(&flip_thread, NULL, flip_main, NULL) != 0) {
				pthread_mutex_unlock(&klock);
				return neg_errno(ENOMEM);
			}
			pthread_detach(flip_thread);
			flip_thread_up = true;
		}
		pending = *r;
		flip_busy = true;
		pthread_cond_broadcast(&kcond);
		pthread_mutex_unlock(&klock);
		return 0;
	}
	while (flip_busy)
		pthread_cond_wait(&kcond, &klock);
	pthread_mutex_unlock(&klock);

	if (do_flip(r, &seq, &ns) != 0)
		return -errno;
	pthread_mutex_lock(&klock);
	current_fb = r->restore ? 0 : r->fb;
	pthread_mutex_unlock(&klock);
	if (r->event)
		send_event(r->evfd, DRM_EVENT_FLIP_COMPLETE, r->user_data, seq, ns);
	return 0;
}

static int
flip_to(int fd, uint32_t fb_id, bool nonblock, bool event, uint64_t user_data)
{
	struct flip_req r = { 0 };
	struct kms_fb *fb;
	io_connect_t conn = display_conn(fd, &r.evfd);

	if (conn == IO_OBJECT_NULL)
		return -errno;
	r.fd = fd;
	r.conn = conn;
	r.event = event;
	r.user_data = user_data;
	if (fb_id == 0) {
		r.restore = true;
	} else {
		pthread_mutex_lock(&klock);
		fb = find_fb(fb_id);
		if (fb != NULL) {
			r.handle = fb->handle;
			r.pitch = fb->pitch;
			r.offset = fb->offset;
			r.fb = fb_id;
		}
		pthread_mutex_unlock(&klock);
		if (fb == NULL)
			return neg_errno(ENOENT);
	}
	return queue_flip(&r, nonblock);
}


drmModeResPtr
drmModeGetResources(int fd)
{
	io_connect_t conn = display_conn(fd, NULL);
	drmModeResPtr r;

	if (conn == IO_OBJECT_NULL || !display_info(conn))
		return NULL;
	r = calloc(1, sizeof(*r));
	if (r == NULL)
		return NULL;
	r->crtcs = malloc(sizeof(uint32_t));
	r->connectors = malloc(sizeof(uint32_t));
	r->encoders = malloc(sizeof(uint32_t));
	r->fbs = calloc(MAX_FBS, sizeof(uint32_t));
	if (r->crtcs == NULL || r->connectors == NULL || r->encoders == NULL || r->fbs == NULL) {
		drmModeFreeResources(r);
		return NULL;
	}
	r->count_crtcs = r->count_connectors = r->count_encoders = 1;
	r->crtcs[0] = ID_CRTC;
	r->connectors[0] = ID_CONNECTOR;
	r->encoders[0] = ID_ENCODER;
	pthread_mutex_lock(&klock);
	for (int i = 0; i < MAX_FBS; i++) {
		if (fbs[i].id != 0 && fbs[i].fd == fd)
			r->fbs[r->count_fbs++] = fbs[i].id;
	}
	pthread_mutex_unlock(&klock);
	r->min_width = r->max_width = dinfo.width;
	r->min_height = r->max_height = dinfo.height;
	return r;
}

void
drmModeFreeResources(drmModeResPtr r)
{
	if (r == NULL)
		return;
	free(r->fbs);
	free(r->crtcs);
	free(r->connectors);
	free(r->encoders);
	free(r);
}

static drmModeConnectorPtr
get_connector(int fd, uint32_t id)
{
	io_connect_t conn = display_conn(fd, NULL);
	drmModeConnectorPtr c;

	if (conn == IO_OBJECT_NULL || !display_info(conn))
		return NULL;
	if (id != ID_CONNECTOR) {
		errno = ENOENT;
		return NULL;
	}
	c = calloc(1, sizeof(*c));
	if (c == NULL)
		return NULL;
	c->modes = calloc(1, sizeof(*c->modes));
	c->props = calloc(2, sizeof(uint32_t));
	c->prop_values = calloc(2, sizeof(uint64_t));
	c->encoders = calloc(1, sizeof(uint32_t));
	if (c->modes == NULL || c->props == NULL || c->prop_values == NULL || c->encoders == NULL) {
		drmModeFreeConnector(c);
		return NULL;
	}
	c->connector_id = ID_CONNECTOR;
	c->encoder_id = ID_ENCODER;
	c->connector_type = DRM_MODE_CONNECTOR_HDMIA;
	c->connector_type_id = 1;
	c->connection = DRM_MODE_CONNECTED;
	c->subpixel = DRM_MODE_SUBPIXEL_UNKNOWN;
	c->count_modes = 1;
	fill_mode(&c->modes[0]);
	c->count_props = 2;
	c->props[0] = P_CONN_CRTC_ID;
	c->prop_values[0] = ID_CRTC;
	c->props[1] = P_DPMS;
	c->prop_values[1] = DRM_MODE_DPMS_ON;
	c->count_encoders = 1;
	c->encoders[0] = ID_ENCODER;
	return c;
}

drmModeConnectorPtr
drmModeGetConnector(int fd, uint32_t connector_id)
{
	return get_connector(fd, connector_id);
}

drmModeConnectorPtr
drmModeGetConnectorCurrent(int fd, uint32_t connector_id)
{
	return get_connector(fd, connector_id);
}

void
drmModeFreeConnector(drmModeConnectorPtr c)
{
	if (c == NULL)
		return;
	free(c->modes);
	free(c->props);
	free(c->prop_values);
	free(c->encoders);
	free(c);
}

drmModeEncoderPtr
drmModeGetEncoder(int fd, uint32_t encoder_id)
{
	drmModeEncoderPtr e;

	if (display_conn(fd, NULL) == IO_OBJECT_NULL)
		return NULL;
	if (encoder_id != ID_ENCODER) {
		errno = ENOENT;
		return NULL;
	}
	e = calloc(1, sizeof(*e));
	if (e == NULL)
		return NULL;
	e->encoder_id = ID_ENCODER;
	e->encoder_type = DRM_MODE_ENCODER_TMDS;
	e->crtc_id = ID_CRTC;
	e->possible_crtcs = 1;
	return e;
}

void
drmModeFreeEncoder(drmModeEncoderPtr e)
{
	free(e);
}

drmModeCrtcPtr
drmModeGetCrtc(int fd, uint32_t crtc_id)
{
	io_connect_t conn = display_conn(fd, NULL);
	drmModeCrtcPtr c;

	if (conn == IO_OBJECT_NULL || !display_info(conn))
		return NULL;
	if (crtc_id != ID_CRTC) {
		errno = ENOENT;
		return NULL;
	}
	c = calloc(1, sizeof(*c));
	if (c == NULL)
		return NULL;
	c->crtc_id = ID_CRTC;
	pthread_mutex_lock(&klock);
	c->buffer_id = current_fb;
	pthread_mutex_unlock(&klock);
	c->width = dinfo.width;
	c->height = dinfo.height;
	c->mode_valid = 1;
	fill_mode(&c->mode);
	return c;
}

void
drmModeFreeCrtc(drmModeCrtcPtr c)
{
	free(c);
}

drmModePlaneResPtr
drmModeGetPlaneResources(int fd)
{
	drmModePlaneResPtr r;

	if (display_conn(fd, NULL) == IO_OBJECT_NULL)
		return NULL;
	r = calloc(1, sizeof(*r));
	if (r == NULL || (r->planes = malloc(sizeof(uint32_t))) == NULL) {
		free(r);
		return NULL;
	}
	r->count_planes = 1;
	r->planes[0] = ID_PLANE;
	return r;
}

void
drmModeFreePlaneResources(drmModePlaneResPtr r)
{
	if (r == NULL)
		return;
	free(r->planes);
	free(r);
}

drmModePlanePtr
drmModeGetPlane(int fd, uint32_t plane_id)
{
	drmModePlanePtr p;

	if (display_conn(fd, NULL) == IO_OBJECT_NULL)
		return NULL;
	if (plane_id != ID_PLANE) {
		errno = ENOENT;
		return NULL;
	}
	p = calloc(1, sizeof(*p));
	if (p == NULL || (p->formats = calloc(2, sizeof(uint32_t))) == NULL) {
		free(p);
		return NULL;
	}
	// the layer scans a8r8g8b8, x8r8g8b8 is the same bytes with alpha ignored by the blender
	p->count_formats = 2;
	p->formats[0] = DRM_FORMAT_XRGB8888;
	p->formats[1] = DRM_FORMAT_ARGB8888;
	p->plane_id = ID_PLANE;
	p->crtc_id = ID_CRTC;
	pthread_mutex_lock(&klock);
	p->fb_id = current_fb;
	pthread_mutex_unlock(&klock);
	p->possible_crtcs = 1;
	return p;
}

void
drmModeFreePlane(drmModePlanePtr p)
{
	if (p == NULL)
		return;
	free(p->formats);
	free(p);
}


static const struct prop_def *
find_prop(uint32_t id)
{
	for (size_t i = 0; i < sizeof(props) / sizeof(props[0]); i++) {
		if (props[i].id == id)
			return &props[i];
	}
	return NULL;
}

drmModePropertyPtr
drmModeGetProperty(int fd, uint32_t property_id)
{
	const struct prop_def *d = find_prop(property_id);
	drmModePropertyPtr p;

	if (display_conn(fd, NULL) == IO_OBJECT_NULL)
		return NULL;
	if (d == NULL) {
		errno = ENOENT;
		return NULL;
	}
	p = calloc(1, sizeof(*p));
	if (p == NULL)
		return NULL;
	p->prop_id = d->id;
	p->flags = d->flags;
	snprintf(p->name, sizeof(p->name), "%s", d->name);
	if (d->flags & (DRM_MODE_PROP_RANGE | DRM_MODE_PROP_SIGNED_RANGE)) {
		p->values = calloc(2, sizeof(uint64_t));
		if (p->values != NULL) {
			p->count_values = 2;
			p->values[0] = d->min;
			p->values[1] = d->max;
		}
	} else if (d->id == P_TYPE || d->id == P_DPMS) {
		static const char *types[] = { "Overlay", "Primary", "Cursor" };
		static const char *dpms[] = { "On", "Standby", "Suspend", "Off" };
		const char **names = d->id == P_TYPE ? types : dpms;
		int n = d->id == P_TYPE ? 3 : 4;

		p->enums = calloc(n, sizeof(*p->enums));
		p->values = calloc(n, sizeof(uint64_t));
		if (p->enums != NULL && p->values != NULL) {
			p->count_enums = p->count_values = n;
			for (int i = 0; i < n; i++) {
				p->enums[i].value = i;
				p->values[i] = i;
				snprintf(p->enums[i].name, sizeof(p->enums[i].name), "%s", names[i]);
			}
		}
	}
	return p;
}

void
drmModeFreeProperty(drmModePropertyPtr p)
{
	if (p == NULL)
		return;
	free(p->values);
	free(p->enums);
	free(p->blob_ids);
	free(p);
}

drmModeObjectPropertiesPtr
drmModeObjectGetProperties(int fd, uint32_t object_id, uint32_t object_type)
{
	uint32_t ids[12];
	uint64_t vals[12];
	uint32_t n = 0, fb;
	drmModeObjectPropertiesPtr r;

	(void)object_type;
	if (display_conn(fd, NULL) == IO_OBJECT_NULL)
		return NULL;
	pthread_mutex_lock(&klock);
	fb = current_fb;
	pthread_mutex_unlock(&klock);

	switch (object_id) {
	case ID_CONNECTOR:
		ids[n] = P_CONN_CRTC_ID; vals[n++] = ID_CRTC;
		ids[n] = P_DPMS; vals[n++] = DRM_MODE_DPMS_ON;
		break;
	case ID_CRTC:
		ids[n] = P_ACTIVE; vals[n++] = 1;
		ids[n] = P_MODE_ID; vals[n++] = ID_MODE_BLOB;
		break;
	case ID_PLANE:
		ids[n] = P_TYPE; vals[n++] = DRM_PLANE_TYPE_PRIMARY;
		ids[n] = P_FB_ID; vals[n++] = fb;
		ids[n] = P_PLANE_CRTC_ID; vals[n++] = ID_CRTC;
		ids[n] = P_CRTC_X; vals[n++] = 0;
		ids[n] = P_CRTC_Y; vals[n++] = 0;
		ids[n] = P_CRTC_W; vals[n++] = dinfo.width;
		ids[n] = P_CRTC_H; vals[n++] = dinfo.height;
		ids[n] = P_SRC_X; vals[n++] = 0;
		ids[n] = P_SRC_Y; vals[n++] = 0;
		ids[n] = P_SRC_W; vals[n++] = (uint64_t)dinfo.width << 16;
		ids[n] = P_SRC_H; vals[n++] = (uint64_t)dinfo.height << 16;
		break;
	default:
		errno = ENOENT;
		return NULL;
	}
	r = calloc(1, sizeof(*r));
	if (r == NULL)
		return NULL;
	r->props = malloc(n * sizeof(uint32_t));
	r->prop_values = malloc(n * sizeof(uint64_t));
	if (r->props == NULL || r->prop_values == NULL) {
		drmModeFreeObjectProperties(r);
		return NULL;
	}
	memcpy(r->props, ids, n * sizeof(uint32_t));
	memcpy(r->prop_values, vals, n * sizeof(uint64_t));
	r->count_props = n;
	return r;
}

void
drmModeFreeObjectProperties(drmModeObjectPropertiesPtr p)
{
	if (p == NULL)
		return;
	free(p->props);
	free(p->prop_values);
	free(p);
}

int
drmModeConnectorSetProperty(int fd, uint32_t connector_id, uint32_t property_id, uint64_t value)
{
	(void)connector_id;
	(void)property_id;
	(void)value;

	return display_conn(fd, NULL) == IO_OBJECT_NULL ? -errno : 0;
}

int
drmModeObjectSetProperty(int fd, uint32_t object_id, uint32_t object_type, uint32_t property_id,
    uint64_t value)
{
	(void)object_type;

	if (object_id == ID_PLANE && property_id == P_FB_ID)
		return flip_to(fd, (uint32_t)value, false, false, 0);
	return drmModeConnectorSetProperty(fd, object_id, property_id, value);
}

int
drmModeCreatePropertyBlob(int fd, const void *data, size_t size, uint32_t *id)
{
	if (display_conn(fd, NULL) == IO_OBJECT_NULL)
		return -errno;
	pthread_mutex_lock(&klock);
	for (int i = 0; i < MAX_BLOBS; i++) {
		if (blobs[i].id == 0 && (blobs[i].data = malloc(size ? size : 1)) != NULL) {
			memcpy(blobs[i].data, data, size);
			blobs[i].len = size;
			blobs[i].id = next_blob++;
			*id = blobs[i].id;
			pthread_mutex_unlock(&klock);
			return 0;
		}
	}
	pthread_mutex_unlock(&klock);
	return neg_errno(ENOSPC);
}

int
drmModeDestroyPropertyBlob(int fd, uint32_t id)
{
	(void)fd;

	pthread_mutex_lock(&klock);
	for (int i = 0; i < MAX_BLOBS; i++) {
		if (blobs[i].id == id) {
			free(blobs[i].data);
			memset(&blobs[i], 0, sizeof(blobs[i]));
		}
	}
	pthread_mutex_unlock(&klock);
	return 0;
}

drmModePropertyBlobPtr
drmModeGetPropertyBlob(int fd, uint32_t blob_id)
{
	io_connect_t conn = display_conn(fd, NULL);
	drmModePropertyBlobPtr b;

	if (conn == IO_OBJECT_NULL || !display_info(conn))
		return NULL;
	b = calloc(1, sizeof(*b));
	if (b == NULL)
		return NULL;
	b->id = blob_id;
	if (blob_id == ID_MODE_BLOB) {
		drmModeModeInfo *m = malloc(sizeof(*m));

		if (m != NULL)
			fill_mode(m);
		b->data = m;
		b->length = sizeof(*m);
	} else {
		pthread_mutex_lock(&klock);
		for (int i = 0; i < MAX_BLOBS; i++) {
			if (blobs[i].id == blob_id && (b->data = malloc(blobs[i].len ? blobs[i].len : 1)) != NULL) {
				memcpy(b->data, blobs[i].data, blobs[i].len);
				b->length = (uint32_t)blobs[i].len;
			}
		}
		pthread_mutex_unlock(&klock);
	}
	if (b->data == NULL) {
		free(b);
		errno = ENOENT;
		return NULL;
	}
	return b;
}

void
drmModeFreePropertyBlob(drmModePropertyBlobPtr b)
{
	if (b == NULL)
		return;
	free(b->data);
	free(b);
}


int
drmModeAddFB2WithModifiers(int fd, uint32_t width, uint32_t height, uint32_t pixel_format,
    const uint32_t bo_handles[4], const uint32_t pitches[4], const uint32_t offsets[4],
    const uint64_t modifier[4], uint32_t *buf_id, uint32_t flags)
{
	if (display_conn(fd, NULL) == IO_OBJECT_NULL)
		return -errno;
	if ((flags & DRM_MODE_FB_MODIFIERS) && modifier != NULL && modifier[0] != DRM_FORMAT_MOD_LINEAR)
		return neg_errno(EINVAL);
	if (pixel_format != DRM_FORMAT_XRGB8888 && pixel_format != DRM_FORMAT_ARGB8888)
		return neg_errno(EINVAL);

	pthread_mutex_lock(&klock);
	for (int i = 0; i < MAX_FBS; i++) {
		if (fbs[i].id == 0) {
			fbs[i] = (struct kms_fb){ next_fb++, bo_handles[0], width, height, pixel_format,
			    pitches[0], offsets[0], fd };
			*buf_id = fbs[i].id;
			pthread_mutex_unlock(&klock);
			if (pd_debug())
				fprintf(stderr, "libdrm-pdpvr: fb %u = handle %u %ux%u pitch %u\n", *buf_id,
				    bo_handles[0], width, height, pitches[0]);
			return 0;
		}
	}
	pthread_mutex_unlock(&klock);
	return neg_errno(ENOSPC);
}

int
drmModeAddFB2(int fd, uint32_t width, uint32_t height, uint32_t pixel_format, const uint32_t bo_handles[4],
    const uint32_t pitches[4], const uint32_t offsets[4], uint32_t *buf_id, uint32_t flags)
{
	return drmModeAddFB2WithModifiers(fd, width, height, pixel_format, bo_handles, pitches, offsets, NULL,
	    buf_id, flags & ~DRM_MODE_FB_MODIFIERS);
}

int
drmModeAddFB(int fd, uint32_t width, uint32_t height, uint8_t depth, uint8_t bpp, uint32_t pitch,
    uint32_t bo_handle, uint32_t *buf_id)
{
	uint32_t handles[4] = { bo_handle }, pitches[4] = { pitch }, offsets[4] = { 0 };

	if (bpp != 32)
		return neg_errno(EINVAL);
	return drmModeAddFB2(fd, width, height, depth == 32 ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888,
	    handles, pitches, offsets, buf_id, 0);
}

int
drmModeRmFB(int fd, uint32_t buffer_id)
{
	struct kms_fb *fb;
	bool shown;

	pthread_mutex_lock(&klock);
	fb = find_fb(buffer_id);
	shown = fb != NULL && buffer_id == current_fb;
	if (fb != NULL)
		memset(fb, 0, sizeof(*fb));
	pthread_mutex_unlock(&klock);
	if (fb == NULL)
		return neg_errno(ENOENT);
	// a removed framebuffer leaves the plane, as in linux
	if (shown)
		flip_to(fd, 0, false, false, 0);
	return 0;
}

int
drmModeCloseFB(int fd, uint32_t buffer_id)
{
	struct kms_fb *fb;

	(void)fd;
	pthread_mutex_lock(&klock);
	fb = find_fb(buffer_id);
	if (fb != NULL)
		memset(fb, 0, sizeof(*fb));
	pthread_mutex_unlock(&klock);
	return fb != NULL ? 0 : neg_errno(ENOENT);
}


int
drmModeSetCrtc(int fd, uint32_t crtc_id, uint32_t buffer_id, uint32_t x, uint32_t y, uint32_t *connectors,
    int count, drmModeModeInfoPtr mode)
{
	(void)x;
	(void)y;
	(void)connectors;
	(void)count;
	(void)mode;

	if (crtc_id != ID_CRTC)
		return neg_errno(ENOENT);
	return flip_to(fd, buffer_id, false, false, 0);
}

int
drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id, uint32_t flags, void *user_data)
{
	if (crtc_id != ID_CRTC || fb_id == 0)
		return neg_errno(EINVAL);
	return flip_to(fd, fb_id, true, flags & DRM_MODE_PAGE_FLIP_EVENT, (uint64_t)(uintptr_t)user_data);
}

int
drmModeSetPlane(int fd, uint32_t plane_id, uint32_t crtc_id, uint32_t fb_id, uint32_t flags, int32_t crtc_x,
    int32_t crtc_y, uint32_t crtc_w, uint32_t crtc_h, uint32_t src_x, uint32_t src_y, uint32_t src_w,
    uint32_t src_h)
{
	(void)crtc_id;
	(void)flags;
	(void)crtc_x;
	(void)crtc_y;
	(void)crtc_w;
	(void)crtc_h;
	(void)src_x;
	(void)src_y;
	(void)src_w;
	(void)src_h;

	if (plane_id != ID_PLANE)
		return neg_errno(ENOENT);
	return flip_to(fd, fb_id, false, false, 0);
}

drmModeAtomicReqPtr
drmModeAtomicAlloc(void)
{
	return calloc(1, sizeof(struct _drmModeAtomicReq));
}

void
drmModeAtomicFree(drmModeAtomicReqPtr req)
{
	if (req == NULL)
		return;
	free(req->items);
	free(req);
}

int
drmModeAtomicAddProperty(drmModeAtomicReqPtr req, uint32_t object_id, uint32_t property_id, uint64_t value)
{
	if (req == NULL)
		return -EINVAL;
	if (req->cursor == req->size) {
		uint32_t cap = req->size ? req->size * 2 : 16;
		struct atomic_item *items = realloc(req->items, cap * sizeof(*items));

		if (items == NULL)
			return -ENOMEM;
		req->items = items;
		req->size = cap;
	}
	req->items[req->cursor++] = (struct atomic_item){ object_id, property_id, value };
	return req->cursor;
}

int
drmModeAtomicGetCursor(const drmModeAtomicReqPtr req)
{
	return req != NULL ? (int)req->cursor : 0;
}

void
drmModeAtomicSetCursor(drmModeAtomicReqPtr req, int cursor)
{
	if (req != NULL && cursor >= 0 && (uint32_t)cursor <= req->cursor)
		req->cursor = cursor;
}

drmModeAtomicReqPtr
drmModeAtomicDuplicate(const drmModeAtomicReqPtr req)
{
	drmModeAtomicReqPtr d = drmModeAtomicAlloc();

	if (d == NULL || req == NULL)
		return d;
	for (uint32_t i = 0; i < req->cursor; i++) {
		if (drmModeAtomicAddProperty(d, req->items[i].obj, req->items[i].prop, req->items[i].value) < 0) {
			drmModeAtomicFree(d);
			return NULL;
		}
	}
	return d;
}

int
drmModeAtomicMerge(drmModeAtomicReqPtr base, const drmModeAtomicReqPtr augment)
{
	if (base == NULL || augment == NULL)
		return -EINVAL;
	for (uint32_t i = 0; i < augment->cursor; i++) {
		int ret = drmModeAtomicAddProperty(base, augment->items[i].obj, augment->items[i].prop,
		    augment->items[i].value);

		if (ret < 0)
			return ret;
	}
	return 0;
}

// only the plane's framebuffer and the crtc's active state change anything, the mode is fixed
int
drmModeAtomicCommit(int fd, const drmModeAtomicReqPtr req, uint32_t flags, void *user_data)
{
	bool have_fb = false, off = false;
	uint32_t fb = 0;

	if (display_conn(fd, NULL) == IO_OBJECT_NULL)
		return -errno;
	if (req == NULL)
		return neg_errno(EINVAL);
	for (uint32_t i = 0; i < req->cursor; i++) {
		const struct atomic_item *it = &req->items[i];

		if (it->obj == ID_PLANE && it->prop == P_FB_ID) {
			have_fb = true;
			fb = (uint32_t)it->value;
		} else if (it->obj == ID_CRTC && it->prop == P_ACTIVE && it->value == 0) {
			off = true;
		}
	}
	if (have_fb && fb != 0) {
		pthread_mutex_lock(&klock);
		have_fb = find_fb(fb) != NULL;
		pthread_mutex_unlock(&klock);
		if (!have_fb)
			return neg_errno(EINVAL);
	}
	if (flags & DRM_MODE_ATOMIC_TEST_ONLY)
		return 0;
	if (off)
		fb = 0;
	else if (!have_fb)
		return 0;
	return flip_to(fd, fb, flags & DRM_MODE_ATOMIC_NONBLOCK, flags & DRM_MODE_PAGE_FLIP_EVENT,
	    (uint64_t)(uintptr_t)user_data);
}


int
drmHandleEvent(int fd, drmEventContextPtr evctx)
{
	char buffer[1024];
	int len, i = 0;

	len = (int)read(fd, buffer, sizeof(buffer));
	if (len == 0)
		return 0;
	if (len < (int)sizeof(struct drm_event))
		return -1;
	while (i + (int)sizeof(struct drm_event) <= len) {
		struct drm_event *e = (struct drm_event *)(buffer + i);
		struct drm_event_vblank *vb = (struct drm_event_vblank *)e;

		if (e->length < sizeof(*e) || i + (int)e->length > len)
			break;
		if (e->type == DRM_EVENT_FLIP_COMPLETE) {
			if (evctx->version >= 3 && evctx->page_flip_handler2 != NULL)
				evctx->page_flip_handler2(fd, vb->sequence, vb->tv_sec, vb->tv_usec, vb->crtc_id,
				    (void *)(uintptr_t)vb->user_data);
			else if (evctx->version >= 2 && evctx->page_flip_handler != NULL)
				evctx->page_flip_handler(fd, vb->sequence, vb->tv_sec, vb->tv_usec,
				    (void *)(uintptr_t)vb->user_data);
		} else if (e->type == DRM_EVENT_VBLANK && evctx->vblank_handler != NULL) {
			evctx->vblank_handler(fd, vb->sequence, vb->tv_sec, vb->tv_usec, (void *)(uintptr_t)vb->user_data);
		}
		i += e->length;
	}
	return 0;
}

// vblank counters and queued vblank events are not provided
int
drmCrtcGetSequence(int fd, uint32_t crtcId, uint64_t *sequence, uint64_t *ns)
{
	(void)fd;
	(void)crtcId;
	(void)sequence;
	(void)ns;

	return pd_fail(EOPNOTSUPP);
}

int
drmCrtcQueueSequence(int fd, uint32_t crtcId, uint32_t flags, uint64_t sequence, uint64_t *sequence_queued,
    uint64_t user_data)
{
	(void)fd;
	(void)crtcId;
	(void)flags;
	(void)sequence;
	(void)sequence_queued;
	(void)user_data;

	return pd_fail(EOPNOTSUPP);
}

void
drmModeFreeModeInfo(drmModeModeInfoPtr ptr)
{
	free(ptr);
}
