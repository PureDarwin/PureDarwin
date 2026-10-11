// host tests for PDObjMap: lookup, release and reuse, growth to the cap, against a reference map
// build: c++ -std=c++17 -Wall -Werror -fsanitize=address,undefined -I .. objmaptest.cpp -o objmaptest
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <set>
#include "PDObjMap.h"

static int sFail;

static void
check(bool ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        sFail++;
}

// a record like the texture table's: each slot remembers whose it is
struct Rec {
    uint32_t id, w, h;
    uint16_t pf;
};
typedef PDObjMap<Rec> Map;

static Map *
fresh_map(void)
{
    return (Map *)calloc(1, sizeof(Map));
}

static void
free_map(Map *m)
{
    m->destroy();
    free(m);
}

// every live id found at its slot with its own record, slots unique, counts in step, load under 70%
static bool
consistent(Map *m)
{
    std::set<uint32_t> slots;
    uint32_t n = 0;

    for (uint32_t s = 1; s < m->npages * Map::kPageSlots; s++) {
        uint32_t id = m->pages[s / Map::kPageSlots]->ids[s % Map::kPageSlots];

        if (id == 0)
            continue;
        n++;
        if (m->find(id) != s || !slots.insert(s).second)
            return false;
        if (m->at(s).id != id)
            return false;
    }
    return n == m->count && (m->npages == 0 || m->pages[0]->ids[0] == 0) &&
        (uint64_t)m->count * 10 <= (uint64_t)m->hcap * 7;
}

static uint32_t
put(Map *m, uint32_t id, bool *fresh)
{
    uint32_t s = m->get(id, fresh);

    if (s != 0 && *fresh) {
        m->at(s).id = id;
        m->at(s).w = id * 3;
    }
    return s;
}

// random get/drop/find against a reference map, ids from 1..narrow, mixed with ids from the whole range
static void
random_phase(uint32_t narrow, bool mixWide, uint32_t steps)
{
    Map *m = fresh_map();
    std::unordered_map<uint32_t, uint32_t> ref, bySlot;
    bool ok = true, fresh;
    uint32_t seed = 12345 + narrow, fulls = 0, maxLive = 0;

    for (uint32_t step = 0; step < steps && ok; step++) {
        seed = seed * 1103515245u + 12345u;
        uint32_t r = seed >> 8;
        uint32_t id = (r & 1) || !mixWide ? 1 + (r >> 1) % narrow : 1 + (r >> 1);
        uint32_t op = (seed >> 4) % 3;

        if (op == 0) {
            uint32_t s = put(m, id, &fresh);
            auto it = ref.find(id);

            if (it != ref.end()) {
                ok = s == it->second && !fresh && m->at(s).id == id;
            } else if (s == 0) {
                // full: only at the cap, and nothing changed
                ok = ref.size() == Map::kMaxSlots - 1;
                fulls++;
            } else {
                ok = fresh && m->at(s).w == id * 3 && bySlot.count(s) == 0;
                ref[id] = s;
                bySlot[s] = id;
            }
        } else if (op == 1) {
            uint32_t s = m->drop(id);
            auto it = ref.find(id);

            ok = it == ref.end() ? s == 0 : s == it->second && m->at(s).id == 0 && m->at(s).w == 0;
            if (it != ref.end()) {
                bySlot.erase(it->second);
                ref.erase(it);
            }
        } else {
            uint32_t s = m->find(id);
            auto it = ref.find(id);

            ok = it == ref.end() ? s == 0 : s == it->second && m->at(s).id == id;
        }
        if (ref.size() > maxLive)
            maxLive = ref.size();
        if (step % 50000 == 0)
            ok = ok && consistent(m);
    }
    char what[160];
    snprintf(what, sizeof(what), "%u random ops, ids 1..%u%s, match a reference map", steps, narrow,
        mixWide ? " and wide" : "");
    check(ok, what);
    check(consistent(m), "final state consistent");
    printf("     (peak %u live, %u refused at the cap, %u pages, hash %u)\n", maxLive, fulls, m->npages, m->hcap);
    free_map(m);
}

int
main(void)
{
    Map *m = fresh_map();
    bool fresh;
    uint32_t s1, s2, s3;

    // an empty map has nothing and reads zeros
    check(m->find(5) == 0 && m->drop(5) == 0 && m->at(0).id == 0 && m->at(7).w == 0, "empty map finds nothing, reads zeros");

    // basic insert and lookup, including ids past any old table bound
    s1 = put(m, 5, &fresh);
    check(s1 != 0 && fresh, "new id gets a fresh slot");
    check(m->get(5, &fresh) == s1 && !fresh, "same id, same slot, not fresh");
    s2 = put(m, 1025, &fresh);
    s3 = put(m, 0xfffffff0u, &fresh);
    check(s2 != 0 && s3 != 0 && s2 != s1 && s3 != s1 && s2 != s3, "ids past 1024 map to distinct slots");
    check(m->find(1025) == s2 && m->at(s2).id == 1025 && m->at(s3).id == 0xfffffff0u, "lookups find them and their records");
    check(m->get(0, &fresh) == 0 && m->find(0) == 0 && m->drop(0) == 0, "id 0 never has a slot");
    check(m->at(0).id == 0, "slot 0 reads zeros");

    // release and reuse: a reused slot starts zeroed
    check(m->drop(1025) == s2 && m->find(1025) == 0, "drop frees the id");
    check(m->at(s2).id == 0 && m->at(s2).w == 0, "drop zeroes the record");
    check(m->drop(1025) == 0, "a second drop is a no-op");
    check(m->find(5) == s1 && m->find(0xfffffff0u) == s3, "others survive a drop");
    m->at(s1).pf = 77;
    s2 = m->get(1025, &fresh);
    check(s2 != 0 && fresh && m->at(s2).w == 0 && m->at(s2).pf == 0, "a dropped id comes back fresh and zeroed");
    check(m->at(s1).pf == 77, "an existing record keeps its contents");
    m->at(s2).id = 1025;
    check(consistent(m), "consistent after reuse");
    free_map(m);

    // colliding ids: same hash bucket chain, drop from the middle keeps the rest reachable
    m = fresh_map();
    put(m, 1, &fresh);	// sizes the hash
    {
        uint32_t ids[8], n = 0;

        for (uint32_t id = 2; n < 8; id++) {
            if (m->hash(id) == m->hash(1))
                ids[n++] = id;
        }
        for (uint32_t i = 0; i < 8; i++)
            put(m, ids[i], &fresh);
        m->drop(ids[3]);
        bool all = m->find(ids[3]) == 0 && m->find(1) != 0;
        for (uint32_t i = 0; i < 8; i++) {
            if (i != 3 && m->find(ids[i]) == 0)
                all = false;
        }
        check(all, "collision chain stays reachable after a middle drop");
        check(consistent(m), "consistent after collision drops");
    }
    free_map(m);

    // growth: no eviction, records stay where they were through every rehash and new page
    m = fresh_map();
    {
        bool ok = true;
        Rec *first = NULL;

        for (uint32_t id = 1; id < Map::kMaxSlots && ok; id++) {
            uint32_t s = put(m, id * 7919, &fresh);

            ok = s != 0 && fresh;
            if (id == 1)
                first = &m->at(s);
        }
        check(ok && m->count == Map::kMaxSlots - 1, "65535 ids fit, nothing evicted");
        check(m->npages == Map::kMaxPages && first == &m->at(m->find(7919)) && first->id == 7919,
            "the first record never moved through 63 page adds and the rehashes");
        ok = true;
        for (uint32_t id = 1; id < Map::kMaxSlots && ok; id++)
            ok = m->at(m->find(id * 7919)).id == id * 7919;
        check(ok, "every id still finds its own record");
        check(m->get(123456789, &fresh) == 0 && !fresh && m->count == Map::kMaxSlots - 1,
            "at the cap a new id is refused, not evicted");
        check(consistent(m), "consistent at the cap");
        s1 = m->find(7919 * 500);
        check(m->drop(7919 * 500) == s1, "a destroyed id frees its slot at the cap");
        s2 = put(m, 123456789, &fresh);
        check(s2 == s1 && fresh && m->at(s2).id == 123456789, "and the next new id takes it");
    }
    free_map(m);

    random_phase(3000, true, 2000000);	// past one page, wide ids too
    random_phase(600, false, 2000000);	// within one page
    random_phase(8000, false, 2000000);	// a few pages, under the cap
    random_phase(200000, false, 3000000);	// up against the cap

    printf("%s\n", sFail ? "FAILED" : "all passed");
    return sFail != 0;
}
