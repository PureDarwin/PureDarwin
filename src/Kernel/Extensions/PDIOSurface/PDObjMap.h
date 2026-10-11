#ifndef PD_OBJMAP_H
#define PD_OBJMAP_H

#include <stdint.h>
#include <stddef.h>
#ifdef KERNEL
#include <IOKit/IOLib.h>
static inline void *pdObjMapAlloc(size_t n) { return IOMallocZero(n); }
static inline void pdObjMapFree(void *p, size_t n) { IOFree(p, n); }
#else
#include <stdlib.h>
static inline void *pdObjMapAlloc(size_t n) { return calloc(1, n); }
static inline void pdObjMapFree(void *p, size_t) { free(p); }
#endif

// guest object ids (which climb past any fixed bound) -> records of type T, one map per id space.
// it grows, never evicts: a slot is freed only when the guest destroys its id
template <typename T>
struct PDObjMap {
	enum : uint32_t {
		kPageSlots = 1024,	// records come in pages that never move, so readers need no lock
		kMaxPages = 64,
		kMaxSlots = kPageSlots * kMaxPages,	// slots are u16 in the hash: 65535 usable, slot 0 never
		kFirstHash = 2048,
	};
	struct Page {
		uint32_t ids[kPageSlots];	// slot -> guest id, 0 = free
		T rec[kPageSlots];
	};

	uint32_t *hkey;		// guest id, 0 = empty (linear probing, backward-shift removal)
	uint16_t *hslot;
	uint32_t hcap;		// power of two, kept under 70% full
	Page *pages[kMaxPages];
	uint32_t npages;
	uint32_t count;		// slots in use
	uint32_t next;		// where the next free-slot search starts
	T zero;			// what slot 0 (an unknown id) reads as

	uint32_t
	hash(uint32_t id) const
	{
		return (id * 2654435761u) & (hcap - 1);
	}

	// the record of a slot: slot 0's is all zeros and only ever read
	T &
	at(uint32_t s)
	{
		if (s == 0 || s / kPageSlots >= __atomic_load_n(&npages, __ATOMIC_ACQUIRE))
			return zero;
		return __atomic_load_n(&pages[s / kPageSlots], __ATOMIC_ACQUIRE)->rec[s % kPageSlots];
	}

	uint32_t
	find(uint32_t id) const
	{
		if (id == 0 || hcap == 0)
			return 0;
		for (uint32_t h = hash(id);; h = (h + 1) & (hcap - 1)) {
			if (hkey[h] == 0)
				return 0;
			if (hkey[h] == id)
				return hslot[h];
		}
	}

	// id's slot, a new zeroed one (*fresh) if it has none. 0 for id 0, or when the map is at
	// kMaxSlots or out of memory: the caller reports that
	uint32_t
	get(uint32_t id, bool *fresh)
	{
		uint32_t s = find(id), h;

		*fresh = false;
		if (s != 0 || id == 0)
			return s;
		if ((count + 2) * 10 > hcap * 7 && !rehash(hcap ? hcap * 2 : (uint32_t)kFirstHash))
			return 0;
		if (count + 1 >= npages * kPageSlots && !addPage())
			return 0;
		for (s = next; s == 0 || pages[s / kPageSlots]->ids[s % kPageSlots] != 0; s = (s + 1) % (npages * kPageSlots)) {
		}
		next = (s + 1) % (npages * kPageSlots);
		pages[s / kPageSlots]->ids[s % kPageSlots] = id;
		pages[s / kPageSlots]->rec[s % kPageSlots] = T();
		count++;
		for (h = hash(id); hkey[h] != 0; h = (h + 1) & (hcap - 1)) {
		}
		hkey[h] = id;
		hslot[h] = (uint16_t)s;
		*fresh = true;
		return s;
	}

	// frees id's slot (zeroing its record) and returns it, 0 if id had none
	uint32_t
	drop(uint32_t id)
	{
		uint32_t h, s;

		if (id == 0 || hcap == 0)
			return 0;
		for (h = hash(id); hkey[h] != id; h = (h + 1) & (hcap - 1)) {
			if (hkey[h] == 0)
				return 0;
		}
		s = hslot[h];
		pages[s / kPageSlots]->ids[s % kPageSlots] = 0;
		pages[s / kPageSlots]->rec[s % kPageSlots] = T();
		count--;
		// close the gap: move up each later entry of the run that may no longer probe past it
		for (uint32_t j = (h + 1) & (hcap - 1); hkey[j] != 0; j = (j + 1) & (hcap - 1)) {
			uint32_t home = hash(hkey[j]);

			if (((j - home) & (hcap - 1)) >= ((j - h) & (hcap - 1))) {
				hkey[h] = hkey[j];
				hslot[h] = hslot[j];
				h = j;
			}
		}
		hkey[h] = 0;
		hslot[h] = 0;
		return s;
	}

	bool
	addPage(void)
	{
		Page *p;

		// zeroed: every record starts as T()
		if (npages >= kMaxPages || (p = (Page *)pdObjMapAlloc(sizeof(Page))) == NULL)
			return false;
		// published once and never freed before the map: a lock-free reader may hold a slot of it
		__atomic_store_n(&pages[npages], p, __ATOMIC_RELEASE);
		__atomic_store_n(&npages, npages + 1, __ATOMIC_RELEASE);
		return true;
	}

	bool
	rehash(uint32_t cap)
	{
		uint32_t *nk = (uint32_t *)pdObjMapAlloc(cap * sizeof(uint32_t));
		uint16_t *ns = (uint16_t *)pdObjMapAlloc(cap * sizeof(uint16_t));

		if (nk == NULL || ns == NULL || cap > 4 * (uint32_t)kMaxSlots) {
			if (nk != NULL)
				pdObjMapFree(nk, cap * sizeof(uint32_t));
			if (ns != NULL)
				pdObjMapFree(ns, cap * sizeof(uint16_t));
			return false;
		}
		for (uint32_t i = 0; i < hcap; i++) {
			uint32_t h;

			if (hkey[i] == 0)
				continue;
			for (h = (hkey[i] * 2654435761u) & (cap - 1); nk[h] != 0; h = (h + 1) & (cap - 1)) {
			}
			nk[h] = hkey[i];
			ns[h] = hslot[i];
		}
		if (hcap != 0) {
			pdObjMapFree(hkey, hcap * sizeof(uint32_t));
			pdObjMapFree(hslot, hcap * sizeof(uint16_t));
		}
		hkey = nk;
		hslot = ns;
		hcap = cap;
		return true;
	}

	// everything the map holds: the owner is going away
	void
	destroy(void)
	{
		if (hcap != 0) {
			pdObjMapFree(hkey, hcap * sizeof(uint32_t));
			pdObjMapFree(hslot, hcap * sizeof(uint16_t));
		}
		for (uint32_t i = 0; i < npages; i++) {
			pdObjMapFree(pages[i], sizeof(Page));
		}
		hkey = NULL;
		hslot = NULL;
		hcap = npages = count = next = 0;
	}
};

#endif
