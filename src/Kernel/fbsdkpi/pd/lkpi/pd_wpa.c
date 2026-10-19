// SPDX-License-Identifier: MIT
// an in-kernel wpa2-psk supplicant, 4-way and group key handshakes (802.11-2016 12.7)
// ccmp pairwise with the ap's group cipher, keys go to net80211 through WPAKEY and MLME ioctls

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/libkern.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ioctl.h>

#include <crypto/sha1.h>
#include <crypto/rijndael/rijndael.h>

#include <string.h>

int pd_fbsd_transmit(void *ifp, const void *frame, size_t len);
extern int (*pd_fbsd_eapol_input)(struct ifnet *ifp, struct mbuf *m);

#define ETHERTYPE_PAE		0x888e
#define EAPOL_KEY		3
#define EAPOL_DESC_RSN		2

#define KEYINFO_VERSION_MASK	0x0007
#define KEYINFO_PAIRWISE	0x0008
#define KEYINFO_INSTALL		0x0040
#define KEYINFO_ACK		0x0080
#define KEYINFO_MIC		0x0100
#define KEYINFO_SECURE		0x0200
#define KEYINFO_ENCDATA		0x1000

#define PD_WPA_MAX_VAPS		4
#define PD_WPA_MAX_FRAME	512

// the eapol-key frame body after the 4-byte eapol header
struct eapol_key {
	uint8_t		type;
	uint8_t		info[2];
	uint8_t		keylen[2];
	uint8_t		replay[8];
	uint8_t		nonce[32];
	uint8_t		iv[16];
	uint8_t		rsc[8];
	uint8_t		id[8];
	uint8_t		mic[16];
	uint8_t		datalen[2];
} __packed;

struct pd_wpa {
	struct ieee80211vap	*vap;
	bool			active;
	uint8_t			pmk[32];
	uint8_t			ie[64];
	int			ielen;
	uint8_t			snonce[32];
	uint8_t			anonce[32];
	// kck, kek and the ccmp temporal key
	uint8_t			ptk[48];
	bool			have_ptk;
	uint64_t		replay;
	bool			have_replay;
};

static struct pd_wpa sWpa[PD_WPA_MAX_VAPS];
static struct mtx sWpaLock;
static bool sWpaInit;

// received frames wait here for the task, the handshake calls back into
// net80211's ioctls which must not run on the receive path
#define PD_WPA_QUEUE	4

struct pd_wpa_frame {
	struct ifnet	*ifp;
	int		len;
	uint8_t		data[PD_WPA_MAX_FRAME];
};

static struct pd_wpa_frame sQueue[PD_WPA_QUEUE];
static int sQueueHead, sQueueCount;
static struct task sWpaTask;

static struct pd_wpa *
wpa_for(struct ieee80211vap *vap, bool create)
{
	for (int i = 0; i < PD_WPA_MAX_VAPS; i++) {
		if (sWpa[i].vap == vap)
			return &sWpa[i];
	}
	if (!create)
		return NULL;
	for (int i = 0; i < PD_WPA_MAX_VAPS; i++) {
		if (sWpa[i].vap == NULL) {
			memset(&sWpa[i], 0, sizeof(sWpa[i]));
			sWpa[i].vap = vap;
			return &sWpa[i];
		}
	}
	return NULL;
}

static inline uint16_t
be16(const uint8_t *p)
{
	return (p[0] << 8) | p[1];
}

static inline void
put_be16(uint8_t *p, uint16_t v)
{
	p[0] = v >> 8;
	p[1] = v;
}

static inline uint64_t
be64(const uint8_t *p)
{
	uint64_t v = 0;
	for (int i = 0; i < 8; i++)
		v = (v << 8) | p[i];
	return v;
}

static void
hmac_sha1(const uint8_t *key, size_t keylen, const uint8_t *data, size_t len,
	uint8_t digest[20])
{
	struct sha1_ctxt ctx;
	uint8_t k[64], pad[64], inner[20];

	memset(k, 0, sizeof(k));
	if (keylen > 64) {
		SHA1Init(&ctx);
		SHA1Update(&ctx, key, keylen);
		SHA1Final(k, &ctx);
	} else {
		memcpy(k, key, keylen);
	}

	for (int i = 0; i < 64; i++)
		pad[i] = k[i] ^ 0x36;
	SHA1Init(&ctx);
	SHA1Update(&ctx, pad, 64);
	SHA1Update(&ctx, data, len);
	SHA1Final(inner, &ctx);

	for (int i = 0; i < 64; i++)
		pad[i] = k[i] ^ 0x5c;
	SHA1Init(&ctx);
	SHA1Update(&ctx, pad, 64);
	SHA1Update(&ctx, inner, 20);
	SHA1Final(digest, &ctx);
}

// the sha1 prf of 12.7.1.2, label, a zero byte, data and a counter byte
static void
prf_sha1(const uint8_t *key, size_t keylen, const char *label,
	const uint8_t *data, size_t datalen, uint8_t *out, size_t outlen)
{
	uint8_t buffer[128], digest[20];
	size_t labellen = strlen(label);
	size_t n = labellen + 1 + datalen;

	memcpy(buffer, label, labellen);
	buffer[labellen] = 0;
	memcpy(buffer + labellen + 1, data, datalen);

	for (uint8_t i = 0; outlen > 0; i++) {
		buffer[n] = i;
		hmac_sha1(key, keylen, buffer, n + 1, digest);
		size_t take = MIN(outlen, sizeof(digest));
		memcpy(out, digest, take);
		out += take;
		outlen -= take;
	}
}

// rfc 3394 key unwrap, in holds n+1 blocks of 8 bytes
static int
aes_unwrap(const uint8_t *kek, const uint8_t *in, size_t inlen, uint8_t *out)
{
	rijndael_ctx ctx;
	uint8_t a[8], b[16];
	size_t n = inlen / 8 - 1;

	if (inlen % 8 != 0 || n < 2)
		return EINVAL;

	rijndael_set_key(&ctx, kek, 128);
	memcpy(a, in, 8);
	memcpy(out, in + 8, n * 8);
	for (int j = 5; j >= 0; j--) {
		for (size_t i = n; i >= 1; i--) {
			uint64_t t = (uint64_t)n * j + i;
			memcpy(b, a, 8);
			for (int k = 0; k < 8; k++)
				b[7 - k] ^= (uint8_t)(t >> (8 * k));
			memcpy(b + 8, out + (i - 1) * 8, 8);
			rijndael_decrypt(&ctx, b, b);
			memcpy(a, b, 8);
			memcpy(out + (i - 1) * 8, b + 8, 8);
		}
	}

	for (int k = 0; k < 8; k++) {
		if (a[k] != 0xa6)
			return EBADMSG;
	}
	return 0;
}

static int
wpa_ioctl(struct ifnet *ifp, int type, void *data, int len)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strlcpy(ireq.i_name, ifp->if_xname, sizeof(ireq.i_name));
	ireq.i_type = type;
	ireq.i_len = len;
	ireq.i_data = data;
	int error = ifp->if_ioctl(ifp, SIOCS80211, (caddr_t)&ireq);
	if (error != 0)
		if_printf(ifp, "wpa: net80211 set %d failed: %d\n", type, error);
	return error;
}

static int
cipher_for_suite(uint8_t suite)
{
	return suite == 2 ? IEEE80211_CIPHER_TKIP : IEEE80211_CIPHER_AES_CCM;
}

static int
install_key(struct ifnet *ifp, int cipher, int keyix, const uint8_t *mac,
	const uint8_t *key, int keylen, uint64_t rsc, bool group)
{
	struct ieee80211req_key ik;

	memset(&ik, 0, sizeof(ik));
	ik.ik_type = cipher;
	ik.ik_keyix = group ? keyix : IEEE80211_KEYIX_NONE;
	ik.ik_keylen = keylen;
	// as wpa_supplicant's bsd driver: pairwise keys transmit and receive,
	// group keys only receive here
	ik.ik_flags = IEEE80211_KEY_RECV | (group ? IEEE80211_KEY_GROUP : IEEE80211_KEY_XMIT);
	memcpy(ik.ik_macaddr, mac, 6);
	ik.ik_keyrsc = rsc;
	memcpy(ik.ik_keydata, key, keylen);
	return wpa_ioctl(ifp, IEEE80211_IOC_WPAKEY, &ik, sizeof(ik));
}

// sends an eapol-key frame with the given fields, the mic covers the whole frame
static void
send_key(struct pd_wpa *wpa, struct ifnet *ifp, const uint8_t *aa, uint8_t eapolver,
	uint16_t info, const uint8_t *replay, const uint8_t *nonce,
	const uint8_t *data, int datalen)
{
	uint8_t frame[PD_WPA_MAX_FRAME];
	struct ieee80211vap *vap = wpa->vap;
	int bodylen = sizeof(struct eapol_key) + datalen;

	if (14 + 4 + bodylen > (int)sizeof(frame))
		return;
	memset(frame, 0, sizeof(frame));

	memcpy(frame, aa, 6);
	memcpy(frame + 6, vap->iv_myaddr, 6);
	put_be16(frame + 12, ETHERTYPE_PAE);

	uint8_t *eapol = frame + 14;
	eapol[0] = eapolver;
	eapol[1] = EAPOL_KEY;
	put_be16(eapol + 2, bodylen);

	struct eapol_key *key = (struct eapol_key *)(eapol + 4);
	key->type = EAPOL_DESC_RSN;
	put_be16(key->info, info);
	memcpy(key->replay, replay, 8);
	if (nonce != NULL)
		memcpy(key->nonce, nonce, 32);
	put_be16(key->datalen, datalen);
	if (datalen > 0)
		memcpy((uint8_t *)(key + 1), data, datalen);

	if ((info & KEYINFO_MIC) != 0) {
		uint8_t digest[20];
		hmac_sha1(wpa->ptk, 16, eapol, 4 + bodylen, digest);
		memcpy(key->mic, digest, 16);
	}

	pd_fbsd_transmit(ifp, frame, 14 + 4 + bodylen);
}

static bool
check_mic(struct pd_wpa *wpa, uint8_t *eapol, int len, struct eapol_key *key)
{
	uint8_t saved[16], digest[20];

	memcpy(saved, key->mic, 16);
	memset(key->mic, 0, 16);
	hmac_sha1(wpa->ptk, 16, eapol, len, digest);
	memcpy(key->mic, saved, 16);
	return timingsafe_bcmp(saved, digest, 16) == 0;
}

// finds the gtk kde in decrypted key data and installs it
static int
install_gtk(struct pd_wpa *wpa, struct ifnet *ifp, const uint8_t *data, int len,
	const uint8_t *rsc)
{
	static const uint8_t broadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	const uint8_t *p = data, *end = data + len;
	uint8_t group_suite = wpa->ielen >= 8 ? wpa->ie[7] : 4;

	while (p + 2 <= end && p + 2 + p[1] <= end) {
		// vendor kde 00-0f-ac:1 carries keyid, a reserved byte and the gtk
		if (p[0] == 0xdd && p[1] >= 6 && p[2] == 0x00 && p[3] == 0x0f
			&& p[4] == 0xac && p[5] == 0x01) {
			int keyid = p[6] & 0x3;
			int keylen = p[1] - 6;
			uint64_t keyrsc = 0;
			for (int i = 7; i >= 0; i--)
				keyrsc = (keyrsc << 8) | rsc[i];
			return install_key(ifp, cipher_for_suite(group_suite), keyid, broadcast,
				p + 8, keylen, keyrsc, true);
		}
		if (p[0] == 0xdd && p[1] == 0)
			break;
		p += 2 + p[1];
	}
	return ENOENT;
}

static void
handle_key(struct pd_wpa *wpa, struct ifnet *ifp, const uint8_t *aa,
	uint8_t *eapol, int len)
{
	struct ieee80211vap *vap = wpa->vap;
	struct eapol_key *key = (struct eapol_key *)(eapol + 4);
	uint16_t info = be16(key->info);
	int datalen = be16(key->datalen);
	uint8_t *data = (uint8_t *)(key + 1);

	if (key->type != EAPOL_DESC_RSN || (int)sizeof(*key) + datalen > len - 4)
		return;
	if ((info & KEYINFO_VERSION_MASK) != 2 || (info & KEYINFO_ACK) == 0)
		return;

	// replays are dropped once a counter has been seen
	uint64_t replay = be64(key->replay);
	if (wpa->have_replay && replay <= wpa->replay)
		return;

	if ((info & KEYINFO_PAIRWISE) != 0 && (info & KEYINFO_MIC) == 0) {
		// message 1: derive the ptk and answer with snonce and our rsn ie
		uint8_t data_in[76];
		const uint8_t *sa = vap->iv_myaddr;

		memcpy(wpa->anonce, key->nonce, 32);
		arc4random_buf(wpa->snonce, 32);
		bool sa_first = memcmp(sa, aa, 6) < 0;
		memcpy(data_in, sa_first ? sa : aa, 6);
		memcpy(data_in + 6, sa_first ? aa : sa, 6);
		bool sn_first = memcmp(wpa->snonce, wpa->anonce, 32) < 0;
		memcpy(data_in + 12, sn_first ? wpa->snonce : wpa->anonce, 32);
		memcpy(data_in + 44, sn_first ? wpa->anonce : wpa->snonce, 32);
		prf_sha1(wpa->pmk, 32, "Pairwise key expansion", data_in, sizeof(data_in),
			wpa->ptk, sizeof(wpa->ptk));
		wpa->have_ptk = true;

		send_key(wpa, ifp, aa, eapol[0], 2 | KEYINFO_PAIRWISE | KEYINFO_MIC,
			key->replay, wpa->snonce, wpa->ie, wpa->ielen);
		if_printf(ifp, "wpa: 4-way message 1, sent 2\n");
		return;
	}

	if (!wpa->have_ptk || !check_mic(wpa, eapol, len, key)) {
		if_printf(ifp, "wpa: eapol-key with a bad mic dropped\n");
		return;
	}
	wpa->replay = replay;
	wpa->have_replay = true;

	uint8_t plain[PD_WPA_MAX_FRAME];
	int plainlen = 0;
	if ((info & KEYINFO_ENCDATA) != 0) {
		if (datalen < 24 || datalen > (int)sizeof(plain) + 8
			|| aes_unwrap(wpa->ptk + 16, data, datalen, plain) != 0) {
			if_printf(ifp, "wpa: key data did not unwrap\n");
			return;
		}
		plainlen = datalen - 8;
	}

	if ((info & KEYINFO_PAIRWISE) != 0) {
		// message 3: the anonce must match message 1
		if (memcmp(key->nonce, wpa->anonce, 32) != 0) {
			if_printf(ifp, "wpa: message 3 anonce mismatch\n");
			return;
		}
		send_key(wpa, ifp, aa, eapol[0],
			2 | KEYINFO_PAIRWISE | KEYINFO_MIC | KEYINFO_SECURE, key->replay,
			NULL, NULL, 0);
		// message 4 leaves in the clear before the pairwise key exists
		pause("wpa4", 20);

		int error = install_key(ifp, IEEE80211_CIPHER_AES_CCM, 0, aa,
			wpa->ptk + 32, 16, 0, false);
		if (error == 0 && plainlen > 0)
			error = install_gtk(wpa, ifp, plain, plainlen, key->rsc);
		// a station's port opens with its keys, MLME AUTHORIZE is for hostap only
		if_printf(ifp, "wpa: 4-way message 3, sent 4, keys %s\n",
			error == 0 ? "installed" : "failed");
		return;
	}

	// group key message 1 rekeys the gtk
	if (plainlen > 0 && install_gtk(wpa, ifp, plain, plainlen, key->rsc) == 0) {
		send_key(wpa, ifp, aa, eapol[0], 2 | KEYINFO_MIC | KEYINFO_SECURE,
			key->replay, NULL, NULL, 0);
		if_printf(ifp, "wpa: group key updated\n");
	}
}

static void
wpa_task(void *arg, int pending)
{
	for (;;) {
		struct pd_wpa_frame frame;

		mtx_lock(&sWpaLock);
		if (sQueueCount == 0) {
			mtx_unlock(&sWpaLock);
			return;
		}
		frame = sQueue[sQueueHead];
		sQueueHead = (sQueueHead + 1) % PD_WPA_QUEUE;
		sQueueCount--;
		struct pd_wpa *wpa = wpa_for(frame.ifp->if_softc, false);
		bool active = wpa != NULL && wpa->active;
		mtx_unlock(&sWpaLock);

		// one handshake per vap at a time, only this task touches its state
		if (!active)
			continue;
		uint8_t *eapol = frame.data + 14;
		int eapollen = MIN(frame.len - 14, 4 + be16(eapol + 2));
		if (eapol[1] == EAPOL_KEY)
			handle_key(wpa, frame.ifp, frame.data + 6, eapol, eapollen);
	}
}

// called for every received 802.1X frame, it is consumed when a handshake runs
static int
wpa_eapol_input(struct ifnet *ifp, struct mbuf *m)
{
	int len = m->m_pkthdr.len;

	mtx_lock(&sWpaLock);
	struct pd_wpa *wpa = wpa_for(ifp->if_softc, false);
	if (wpa == NULL || !wpa->active) {
		mtx_unlock(&sWpaLock);
		return 0;
	}

	if (len >= 14 + 4 + (int)sizeof(struct eapol_key) && len <= PD_WPA_MAX_FRAME
		&& sQueueCount < PD_WPA_QUEUE) {
		struct pd_wpa_frame *frame = &sQueue[(sQueueHead + sQueueCount) % PD_WPA_QUEUE];
		frame->ifp = ifp;
		frame->len = len;
		m_copydata(m, 0, len, frame->data);
		sQueueCount++;
	}
	mtx_unlock(&sWpaLock);
	m_freem(m);
	taskqueue_enqueue(taskqueue_thread, &sWpaTask);
	return 1;
}

static void
wpa_init(void)
{
	if (sWpaInit)
		return;
	mtx_init(&sWpaLock, "pd wpa", NULL, MTX_DEF);
	TASK_INIT(&sWpaTask, 0, wpa_task, NULL);
	pd_fbsd_eapol_input = wpa_eapol_input;
	sWpaInit = true;
}

int
pd_wpa_configure(struct ieee80211vap *vap, const uint8_t *pmk,
	const uint8_t *rsnie, int rsnie_len)
{
	wpa_init();
	if (rsnie_len > (int)sizeof(((struct pd_wpa *)0)->ie))
		return EINVAL;

	mtx_lock(&sWpaLock);
	struct pd_wpa *wpa = wpa_for(vap, true);
	if (wpa == NULL) {
		mtx_unlock(&sWpaLock);
		return ENOSPC;
	}
	memset(wpa, 0, sizeof(*wpa));
	wpa->vap = vap;
	memcpy(wpa->pmk, pmk, 32);
	memcpy(wpa->ie, rsnie, rsnie_len);
	wpa->ielen = rsnie_len;
	wpa->active = true;
	mtx_unlock(&sWpaLock);
	return 0;
}

void
pd_wpa_reset(struct ieee80211vap *vap)
{
	wpa_init();
	mtx_lock(&sWpaLock);
	struct pd_wpa *wpa = wpa_for(vap, false);
	if (wpa != NULL) {
		memset(wpa, 0, sizeof(*wpa));
		wpa->vap = vap;
	}
	mtx_unlock(&sWpaLock);
}
