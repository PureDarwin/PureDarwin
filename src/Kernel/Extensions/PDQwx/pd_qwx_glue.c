// SPDX-License-Identifier: MIT
// what PDQwx.cpp needs from qwx(4) without seeing its softc: attach, detach and a firmware check

#include "pd_qwx_compat.h"
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

// the linux helpers qwx.c defines before its headers
#define BITS_PER_LONG		64
#define GENMASK(h, l)	(((~0UL) >> (BITS_PER_LONG - (h) - 1)) & ((~0UL) << (l)))
#define BIT(x)		(1UL << (x))

#include "qwxreg.h"
#include "qwxvar.h"
#include "pd_qwx_glue.h"

extern const struct cfattach qwx_pci_ca;
int	qwx_init_hw_params(struct qwx_softc *);
int	qwx_core_fetch_board_data_api_n(struct qwx_softc *, const u_char **, size_t *,
	    u_char *, size_t, const char *, int, int, int);
int	qwx_loadfirmware(struct qwx_softc *, int, const char *, u_char **, size_t *);
void	qwx_free_firmware(struct qwx_softc *);

// the attach hook qwx defers to after the root mount, one per device
struct pd_qwx_hook {
	struct device	*dev;
	void		(*fn)(struct device *);
};
static struct pd_qwx_hook pd_qwx_hooks[4];

void
pd_qwx_mountroot(struct device *dev, void (*fn)(struct device *))
{
	for (int i = 0; i < (int)nitems(pd_qwx_hooks); i++) {
		if (pd_qwx_hooks[i].dev == NULL || pd_qwx_hooks[i].dev == dev) {
			pd_qwx_hooks[i].dev = dev;
			pd_qwx_hooks[i].fn = fn;
			return;
		}
	}
	printf("%s: no room for the attach hook\n", dev->dv_xname);
}

static void
(*pd_qwx_take_hook(struct device *dev))(struct device *)
{
	for (int i = 0; i < (int)nitems(pd_qwx_hooks); i++) {
		if (pd_qwx_hooks[i].dev == dev) {
			void (*fn)(struct device *) = pd_qwx_hooks[i].fn;

			pd_qwx_hooks[i].dev = NULL;
			pd_qwx_hooks[i].fn = NULL;
			return fn;
		}
	}
	return NULL;
}

struct device *
pd_qwx_alloc(const char *name)
{
	struct device *dev = malloc(qwx_pci_ca.ca_devsize, M_DEVBUF, M_WAITOK | M_ZERO);

	if (dev == NULL)
		return NULL;
	strlcpy(dev->dv_xname, name, sizeof(dev->dv_xname));
	return dev;
}

void
pd_qwx_free(struct device *dev)
{
	free(dev, M_DEVBUF, qwx_pci_ca.ca_devsize);
}

int
pd_qwx_match(struct pci_attach_args *pa)
{
	return qwx_pci_ca.ca_match(NULL, NULL, pa);
}

// the pci half, interrupts and rings, no firmware yet
int
pd_qwx_attach(struct device *dev, struct pci_attach_args *pa)
{
	pd80211_lock();
	qwx_pci_ca.ca_attach(NULL, dev, pa);
	pd80211_unlock();

	// qwx_pci_attach registers its hook only once everything is set up
	for (int i = 0; i < (int)nitems(pd_qwx_hooks); i++) {
		if (pd_qwx_hooks[i].dev == dev)
			return 0;
	}
	return ENXIO;
}

// the firmware half, run once the root file system is there
int
pd_qwx_attach_hook(struct device *dev)
{
	void (*fn)(struct device *) = pd_qwx_take_hook(dev);
	struct qwx_softc *sc = (struct qwx_softc *)dev;

	if (fn == NULL)
		return ENXIO;
	fn(dev);
	return sc->attached ? 0 : EIO;
}

void
pd_qwx_detach(struct device *dev)
{
	pd80211_lock();
	qwx_pci_ca.ca_detach(dev, 0);
	pd80211_unlock();
}

// the board file variant linux reads from the device tree, LE_X13S on the thinkpad x13s
void
pd_qwx_set_variant(struct device *dev, const char *variant)
{
	struct qwx_softc *sc = (struct qwx_softc *)dev;

	strlcpy(sc->qmi_target.bdf_ext, variant, sizeof(sc->qmi_target.bdf_ext));
}

struct ieee80211com *
pd_qwx_ic(struct device *dev)
{
	return &((struct qwx_softc *)dev)->sc_ic;
}

int
if_setlladdr(struct ifnet *ifp, const uint8_t *lladdr)
{
	memcpy(((struct arpcom *)ifp)->ac_enaddr, lladdr, ETHER_ADDR_LEN);
	return 0;
}

// a chain becomes one cluster, the caller's pointer follows the new mbuf
int
pd_qwx_m_defrag(struct mbuf **mp, int how)
{
	struct mbuf *m = *mp, *n;
	int len = M_PKTLEN(m);

	if (M_NEXT(m) == NULL)
		return 0;
	if (len > MAXMCLBYTES)
		return ENOBUFS;
	n = m_gethdr(how, MT_DATA);
	if (n == NULL)
		return ENOBUFS;
	if (len > MHLEN && m_clget(n, how, len) == NULL) {
		m_freem(n);
		return ENOBUFS;
	}
	if (m_dup_pkthdr(n, m, how) != 0) {
		m_freem(n);
		return ENOBUFS;
	}
	m_copydata(m, 0, len, mtod(n, caddr_t));
	M_SETLEN(n, len);
	M_SETPKTLEN(n, len);
	m_setcookie(n, m_cookie(m));
	m_freem(m);
	*mp = n;
	return 0;
}

// counts the board entries of a board-2 file and copies out the first name
static int
pd_qwx_board_names(const u_char *buf, size_t len, char *first, size_t firstlen)
{
	size_t off = 20;
	int boards = 0;

	first[0] = '\0';
	if (len < off || memcmp(buf, ATH11K_BOARD_MAGIC, sizeof(ATH11K_BOARD_MAGIC)) != 0)
		return -1;
	while (off + 8 <= len) {
		uint32_t id = le32toh(*(const uint32_t *)(buf + off));
		uint32_t ielen = le32toh(*(const uint32_t *)(buf + off + 4));
		size_t in = off + 8;

		if (ielen > len - in)
			return -1;
		// a board container holds a name element and a data element
		while (id == 0 && in + 8 <= off + 8 + ielen) {
			uint32_t sid = le32toh(*(const uint32_t *)(buf + in));
			uint32_t slen = le32toh(*(const uint32_t *)(buf + in + 4));

			if (sid == 0) {
				if (boards++ == 0)
					strlcpy(first, (const char *)buf + in + 8, MIN(firstlen, (size_t)slen + 1));
			}
			in += 8 + roundup(slen, 4);
		}
		off += 8 + roundup(ielen, 4);
	}
	return boards;
}

// no hardware needed: hw params for the chip, then amss, m3 and the board file found and parsed
int
pd_qwx_fwcheck(int rev21)
{
	struct qwx_softc *sc;
	u_char *data;
	size_t len;
	const u_char *bdf;
	size_t bdf_len;
	int error, failed = 0;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK | M_ZERO);
	if (sc == NULL)
		return ENOMEM;
	strlcpy(sc->sc_dev.dv_xname, "qwxtest", sizeof(sc->sc_dev.dv_xname));
	strlcpy(sc->sc_bus_str, "pci", sizeof(sc->sc_bus_str));
	sc->sc_hw_rev = rev21 ? ATH11K_HW_WCN6855_HW21 : ATH11K_HW_WCN6855_HW20;
	error = qwx_init_hw_params(sc);
	if (error) {
		free(sc, M_DEVBUF, sizeof(*sc));
		return error;
	}
	printf("qwxtest: %s, firmware directory %s\n", sc->hw_params.name, sc->hw_params.fw.dir);

	error = qwx_loadfirmware(sc, QWX_FW_AMSS, ATH11K_AMSS_FILE, &data, &len);
	printf("qwxtest: amss error %d, %zu bytes\n", error, error ? 0 : len);
	failed |= error != 0;
	// amss.bin is an ELF image, its first bytes say so
	if (error == 0 && (len < 4 || memcmp(data, "\177ELF", 4) != 0)) {
		printf("qwxtest: amss is not an ELF image\n");
		failed = 1;
	}

	error = qwx_loadfirmware(sc, QWX_FW_M3, ATH11K_M3_FILE, &data, &len);
	printf("qwxtest: m3 error %d, %zu bytes\n", error, error ? 0 : len);
	failed |= error != 0;

	// the qmi ids come from the chip, so the check takes the first board name in the file
	error = qwx_loadfirmware(sc, QWX_FW_BOARD, ATH11K_BOARD_API2_FILE, &data, &len);
	printf("qwxtest: board-2 error %d, %zu bytes\n", error, error ? 0 : len);
	failed |= error != 0;
	if (error == 0) {
		char name[128];
		int boards = pd_qwx_board_names(data, len, name, sizeof(name));

		printf("qwxtest: board-2 has %d boards, first '%s'\n", boards, name);
		error = boards > 0 ? qwx_core_fetch_board_data_api_n(sc, &bdf, &bdf_len, data, len,
		    name, 0, 0, 1) : ENOENT;
		printf("qwxtest: lookup error %d, board data %zu bytes\n", error, error ? 0 : bdf_len);
		failed |= error != 0;
	}

	qwx_free_firmware(sc);
	free(sc, M_DEVBUF, sizeof(*sc));
	printf("qwxtest: %s\n", failed ? "FAIL" : "PASS");
	return failed ? EIO : 0;
}

// reference counts, finalize sleeps until the last holder lets go

void
refcnt_init(struct refcnt *r)
{
	r->r_refs = 1;
}

void
refcnt_take(struct refcnt *r)
{
	OSIncrementAtomic(&r->r_refs);
}

int
refcnt_rele(struct refcnt *r)
{
	return OSDecrementAtomic(&r->r_refs) == 1;
}

void
refcnt_rele_wake(struct refcnt *r)
{
	if (refcnt_rele(r))
		wakeup(r);
}

void
refcnt_finalize(struct refcnt *r, const char *wmesg)
{
	refcnt_rele(r);
	while (r->r_refs > 0)
		tsleep_nsec(r, PZERO, wmesg, INFSLP);
}

// hlen bytes opened up at skip, the bytes after it move back and the chain grows as needed
struct mbuf *
m_makespace(struct mbuf *m, int skip, int hlen, int *off)
{
	int tail = M_PKTLEN(m) - skip;
	uint8_t *saved = NULL;
	struct mbuf *n;

	if (tail < 0 || hlen <= 0)
		return NULL;
	if (tail > 0) {
		saved = malloc(tail, M_DEVBUF, M_NOWAIT);
		if (saved == NULL)
			return NULL;
		m_copydata(m, skip, tail, saved);
	} else {
		saved = malloc(hlen, M_DEVBUF, M_NOWAIT | M_ZERO);
		if (saved == NULL)
			return NULL;
	}
	if (tail > 0 && m_copyback(m, skip + hlen, tail, saved, M_DONTWAIT) != 0)
		goto fail;
	if (tail == 0 && m_copyback(m, skip, hlen, saved, M_DONTWAIT) != 0)
		goto fail;
	free(saved, M_DEVBUF, tail > 0 ? tail : hlen);

	for (n = m; n != NULL && skip >= M_LEN(n); n = M_NEXT(n))
		skip -= M_LEN(n);
	*off = skip;
	return n;

fail:
	free(saved, M_DEVBUF, tail > 0 ? tail : hlen);
	return NULL;
}
