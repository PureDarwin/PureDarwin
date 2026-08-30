# Guard-object allocation policy

The contemporary techniques we use to group and protect smaller allocations,
such as zalloc and kalloc_type, are premised around type-isolation and VA
sequestering. These techniques are impractical for larger allocations because
they consume outsized chunks of virtual address space, and could lead to
exhaustion. We therefore use a different strategy for larger allocations, which
we call guard objects.


## Algorithm

Allocation policies are assigned to particular regions of kernel address space.
The strategy specified by this document is used by regions such as the various
pointer ranges of `kmem_alloc()`, which previously used a first-fit allocator.
Under the guard-objects policy, the virtual address space is divided in
*chunks*. A chunk has a size class of the form $2^k \times \mathtt{PAGE\_SIZE}$,
and is made of $\mathcal{S}$ *slots* of that size. $\mathcal{S}$ varies with the
size class: smaller size-classes will have more slots, and larger size classes
will have fewer. Chunks are also configured to have $\mathcal{G}$ _guards_ and
up to $\mathcal{Q}$ _quarantines_.

Chunks maintain several other important pieces of information, such as:

* which slots are allocated and which slots are free;
* a dynamic count of quarantined slots within $[0, \mathcal{Q})$;
* a count of *available* slots, which are the number of free slots in excess
  of guards $\mathcal{G}$ and the number of currently quarantined slots.

A chunk can be in three states: *empty* if all its slots are free, *partial* if
it has at least one available slot, *full* if it has no available slots. Chunks
are maintained in lists segregated by size class and state.

### Allocating memory

Memory requests for a given size are rounded up to the next size class of the
form $2^k$. The allocator must first select an appropriate chunk. Partial chunks
are preferred, and if no partial chunk exists, an empty chunk is allocated from
unused virtual address space.

A random slot is then chosen from any of the free slots in that chunk, and the
available count of the chunk is decremented. If the chunk has now exhausted its
available slots — only quarantined slots and guards are left — it's placed on
its corresponding size class's full list.

Visually, let’s consider an example with two partial chunks A and B, with 8
slots each, for $\mathcal{G} = \mathcal{Q} = 2$:

```
                A───────────────┬─┬─┬─┬─┐     B───────────────┬─┬─┬─┬─┐
                │ Available:  1 │X│ │ │X│     │ Available:  4 │ │ │X│ │
             ┌─▶│ Quarantine: 0 ├─┼─┼─┼─┤────▶│ Quarantine: 0 ├─┼─┼─┼─┤
┌─────────┐  │  │ Guards:     2 │X│X│X│ │     │ Guards:     2 │ │X│ │ │
│ partial │──┘  └───────────────┴─┴─┴─┴─┘     └───────────────┴─┴─┴─┴─┘
├─────────┤
│  full   │
└─────────┘

Legend:
┌─┐                  ┌─┐
│ │ free slot        │X│ allocated slot
└─┘                  └─┘
```

The first allocation will be performed from chunk A, using its last available
slot and moving it to the full list:

```
                B───────────────┬─┬─┬─┬─┐
                │ Available:  4 │ │ │X│ │
             ┌─▶│ Quarantine: 0 ├─┼─┼─┼─┤
┌─────────┐  │  │ Guards:     2 │ │X│ │ │
│ partial │──┘  └───────────────┴─┴─┴─┴─┘
├─────────┤
│  full   │──┐  A───────────────┬─┬─┬─┬─┐
└─────────┘  │  │ Available:  0 │X│ │X│X│
             └─▶│ Quarantine: 0 ├─┼─┼─┼─┤
                │ Guards:     2 │X│X│X│ │
                └───────────────┴─┴─┴─┴─┘
```

When the next allocation request in this size class arrives, the allocator will
select B because A is now full:

```
                B───────────────┬─┬─┬─┬─┐
                │ Available:  3 │ │ │X│ │
             ┌─▶│ Quarantine: 0 ├─┼─┼─┼─┤
┌─────────┐  │  │ Guards:     2 │ │X│X│ │
│ partial │──┘  └───────────────┴─┴─┴─┴─┘
├─────────┤
│  full   │──┐  A───────────────┬─┬─┬─┬─┐
└─────────┘  │  │ Available:  0 │X│ │X│X│
             └─▶│ Quarantine: 0 ├─┼─┼─┼─┤
                │ Guards:     2 │X│X│X│ │
                └───────────────┴─┴─┴─┴─┘
```

### Deallocating memory

Deallocating a virtual memory range works in reverse. First, we evaluate which
chunk and slot correspond to the range being freed. Since the semantics of the
virtual memory subsystem mandate that we must support partial deallocations, we
next consider whether the slot has become only partially free. If so, we have
nothing more to do for now; the slot remains in use.

If however the slot is now entirely free, then the quarantine count of the chunk
is incremented. If at least $\mathcal{G} + \mathcal{Q}$ are free, then the
quarantine is cleared. The idea behind this policy is that maintaining a good
entropy requires enough free slots to choose from. As a result, once the free
slot count dips below $\mathcal{G} + \mathcal{Q}$, freed slots are quarantined
rather than made immediately available.

Finally, we evaluate whether the chunk needs to be moved:

* if a chunk's slots are all fully free, the chunk is marked as empty, and is
  typically returned to the system as free space;
* if the chunk previously had no slots available, but has any available now,
  the chunk is moved to the partially-free chunks list.

Visually, let’s consider an example with a chunk using 16 slots,
under a configuration in which $\mathcal{G} = \mathcal{Q} = 4$:

```
                 A───────────────┬─┬─┬─┬─┬─┬─┬─┬─┐
┌─────────┐      │ Available:  1 │ │X│X│X│ │ │X│ │
│ partial │─────▶│ Quarantine: 1 ├─┼─┼─┼─┼─┼─┼─┼─┤
├─────────┤      │ Guards:     4 │ │X│X│X│X│X│ │X│
│  full   │      └───────────────┴─┴─┴─┴─┴─┴─┴─┴─┘
└─────────┘

Legend:
┌─┐                  ┌─┐
│ │ free slot        │X│ allocated slot
└─┘                  └─┘
```

If we now free an element, its slot is marked free, and the quarantine count
is increased:

```
                 A───────────────┬─┬─┬─┬─┬─┬─┬─┬─┐
┌─────────┐      │ Available:  1 │ │X│X│X│ │ │X│ │
│ partial │─────▶│ Quarantine: 2 ├─┼─┼─┼─┼─┼─┼─┼─┤
├─────────┤      │ Guards:     4 │ │X│X│ │X│X│ │X│
│  full   │      └───────────────┴─┴─┴─┴─┴─┴─┴─┴─┘
└─────────┘
```

If we allocate the last available element, a slot is now marked used,
the available count drops to 0, and causes the chunk to now be full,
and the quarantine stays untouched:

```
┌─────────┐
│ partial │      A───────────────┬─┬─┬─┬─┬─┬─┬─┬─┐
├─────────┤      │ Available:  0 │X│X│X│X│ │ │X│ │
│  full   │─────▶│ Quarantine: 2 ├─┼─┼─┼─┼─┼─┼─┼─┤
└─────────┘      │ Guards:     4 │ │X│X│ │X│X│ │X│
                 └───────────────┴─┴─┴─┴─┴─┴─┴─┴─┘
```

Freeing just one element would return just one slot, and bump the quarantine
count to 3. It takes freeing two elements for more than $\mathcal{G} +
\mathcal{Q}$ slots to be free, leading to clearing the quarantine:

```
                 A───────────────┬─┬─┬─┬─┬─┬─┬─┬─┐
┌─────────┐      │ Available:  4 │X│X│X│X│ │ │X│ │
│ partial │─────▶│ Quarantine: 0 ├─┼─┼─┼─┼─┼─┼─┼─┤
├─────────┤      │ Guards:     4 │ │X│X│ │ │ │ │X│
│  full   │      └───────────────┴─┴─┴─┴─┴─┴─┴─┴─┘
└─────────┘
```

### Operation clamping to a slot

As long as a VM operation does not exceed slot bounds, any VM call is permitted,
whether it is configuration altering calls such as `vm_protect()` or
`vm_inherit()`, taking copies of the range with `mach_make_memory_entry()`, or
replacing part of the mapping with `vm_allocate(..., VM_FLAGS_FIXED |
VM_FLAGS_OVERWRITE)`.

However, operations that cross a slot boundary are not permitted, and lead to
termination. When guard object policies are in effect, allocations are
randomized, and even in a single threaded context, clients can't assume that
consecutive allocations will be served in address order, and as a result,
operations crossing slot boundaries are always bugs.


## Security motivation

With the algorithm explanation, the “guard object” moniker should start to make
sense: the goal is that unused free slots are object-sized guards — in the
guard-page sense. Unlike usage of traditional guard pages, which only protect
against linear buffer overflows, this scheme also adds a probabilistic
mitigation against use-after-free or non-linear buffer overflows, forcing
attackers to contend with a high probability of hitting these guards.

Due to visible timing differences, it is assumed that an attacker can observe
when a chunk is being allocated or freed. However, this doesn't give them an
edge because the system maintains a guarantee that at least
$\mathcal{G}/\mathcal{S}$ of the address space causes a crash when accessed at
any given time. This is why we can let go of any form of sequestering of the
address space for ranges managed with the guard-object allocation policy.

### Use-after-Free exploitation strategies and failure rates

Attackers attempting to exploit a use-after-free will be able to cause an
element to be freed, knowing that a dangling pointer to it remains. They will
then try to replace this freed element with another one of a different type
that they control, creating a type confusion.

Because allocating new chunks causes visible timing differences, we can assume
that attackers are able to fill chunks with all slots corresponding to elements
that they control. They also know which elements are in the same chunk, but
don't know which element corresponds to which slot.

**In the absence of the quarantine**, the best strategy for attackers trying to
exploit a use-after free is to perform $\mathcal{S} − \mathcal{G}$ rounds
of freeing then reallocating each element in the chunk, where the first free is
to the element they are trying to use-after-free, so that they retain a dangling
pointer to the original slot. Each round, the allocator will choose one slot
among $\mathcal{G} + 1$, when only one corresponds to the slot that was freed by
triggering the bug. The failure rate the attackers face with this strategy is

$$failure\_rate = \left(
\frac{\mathcal{G}}{\mathcal{G+1}}\right)^{\mathcal{S} - \mathcal{G}}$$

* $\mathcal{S} = 8, \mathcal{G} = 2$ yields a 8.8% failure rate;
* $\mathcal{S} = 16, \mathcal{G} = 4$ yields a 6.9 failure rate.

**Using the quarantine** further reduces an attacker's odds of success. Unlike
before, they need to free at least $\mathcal{Q}$ elements before elements are
made available for allocations again. As a result, a round now needs to be
$\mathcal{Q}$ deallocations followed by $\mathcal{Q}$ allocations. As before,
the very first deallocation is to the element the attacker tries to
use-after-free. A round gives attackers $\frac{\mathcal{G}}{
\mathcal{G}+\mathcal{Q}}$ chances of failure. The aggregate failure rate for
this strategy is

$$failure\_rate =\left( 1 - \frac{(\mathcal{S} - \mathcal{G)} \bmod \mathcal{Q}
}{\mathcal{G} + \mathcal{Q}} \right)
\left(\frac{\mathcal{G}}{\mathcal{G} + \mathcal{Q}}\right)^{
\left\lfloor \frac{\mathcal{S} -\mathcal{G}}{\mathcal{Q}} \right\rfloor}$$

* $\mathcal{S}=8, \mathcal{G}=1, \mathcal{Q}=4$ yields a 8% failure rate;
* $\mathcal{S}=8, \mathcal{G}=2, \mathcal{Q}=2$ yields a 12.5% failure rate;
* $\mathcal{S}=16, \mathcal{G}=4, \mathcal{Q}=3$ yields a 10.7% failure rate;
* $\mathcal{S}=16, \mathcal{G}=4, \mathcal{Q}=4$ yields a 12.5% failure rate.

### Out-of-bound exploitation strategies

Exploiting out-of-bound bugs requires knowing the distance between allocated
objects, which an attacker a priori doesn’t know without some information leak.
The regions protected by guard objects are coarsely randomized in the address
space, so that attackers can’t predict how far an allocation is from other juicy
exploitation targets in the address space such as the `__DATA` segment.  Lastly,
guard objects are combined in XNU with type isolation and allocation fronts. It
makes cross-type attacks unreliable and unpredictable — as the various buckets
of types are randomized per boot.

The last remaining avenue of attack targets types that fall in the same
allocation front. However, attackers still have to contend with the uniform
$\mathcal{G}/\mathcal{S}$ density of holes, making out-of-bound unreliable with
a probability of failure close to $\mathcal{G}/\mathcal{S}$. This probability of
failure is typically worse than use-after-free for attackers, and intuitively,
this is precisely because they need to know more information — the distance
between objects — unlike use-after-free where that distance is obviously known
to be zero.

### Choice of parameters

In the actual implementation, $\mathcal{S}$ scales up with the size of the
allocations going up — as a way to limit the amount of metadata needed.
Maintaining the $\mathcal{G}/\mathcal{S}$ and $\mathcal{Q}/\mathcal{S}$ ratios
constant for any $\mathcal{S}$ allows for all probabilities to become
independent of $\mathcal{S}$.

Our goal is to make attackers face at least 10% chance of failure — in the
absence of information disclosure — which is why we chose numbers where
$\mathcal{G} = \mathcal{Q} = \mathcal{S}/4$, yielding:

* 25% failure rates for out-of-bound exploitation;
* 12.5% failure rates for use-after-free exploitation.

