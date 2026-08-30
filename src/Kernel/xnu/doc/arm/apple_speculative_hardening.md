# Apple Speculative Hardening

## Motivation
Speculative execution allows CPUs to continue executing instructions past
older—potentially long latency—instructions by predicting—or “speculating”—their
results. When these predictions are wrong, the CPU discards the incorrectly
executed instructions and restarts execution at the point of divergence with the
corrected result. While this strategy ensures that the program does ultimately
execute as required by the architecture, discarded instructions can have side
effects—such as cache line evictions and fills—which leave subtle traces all
across the micro-architecture.

Attackers can intentionally trigger mispredictions and observe
the side effects of discarded instructions across privilege boundaries in order
to extract—or “leak”—useful information from the other context. When the leaked
information is data, we refer to it as a speculative “data-leaking” side
channel. One of the most famous examples of a data-leaking side channel—and,
indeed, the focus of this particular mitigation—is [Spectre Variant
1](https://en.wikipedia.org/wiki/Spectre_(security_vulnerability)#Spectre_Variant_1)
in which attackers cause certain bounds check branches in the target’s
code—referred to as a “gadget”—to erroneously predict that an out-of-bounds
index is in-bounds, leading the CPU to speculatively load and leak out-of-bounds
memory from the target. In its strongest form, successful exploitation provides
an attacker with a rather awe-inspiring capability: an arbitrary read without
needing to leverage risky, heavily mitigated memory corruption bugs of any kind.

Contemporary discussions of these issues, however, tend to regard them as a
complete, end-to-end, attack and attempt to leverage the resulting arbitrary read
to achieve all of a modern attacker’s desires. While such attacks have an artful
simplicity to them and highlight the greatest extent to which data-leaking side
channels could be exploited, we expect that real-world adversaries will continue
to favor memory corruption bugs for as long as they are tractable because the
power, performance, and reliability of pure side-channel attacks pale in
comparison to that of a traditional memory corruption attack.

In this light, we believe data-leaking side channels are most relevant to
real-world adversaries not as an exploit in and of itself, but rather as a
technique to defeat exploit mitigations and make traditional memory corruption
bugs easier to exploit. This is particularly true in the case of MTE as reliably
exploiting all but the most powerful memory corruption bugs requires a priori
knowledge of allocation tags. Constructing the requisite arbitrary read primitive
with memory corruption, however, is quite difficult because such corruption too
must contend with MTE. Data-leaking side channels, on the other hand, are entirely
unencumbered by this dependency cycle and offer a risk-free means to learn
allocation tags and directly defeat MTE.

As such, we expect that it is not only necessary to mitigate Spectre V1 for a
strong deployment of MTE but too that it is *sufficient*—at least in the near
term—to mitigate it only to the extent it is useful in exploiting memory
corruption bugs.

The mitigation we implement to achieve this is called Apple Speculative
Hardening ("ASH").

## Mitigation overview
Our fundamental mitigation strategy with Apple Speculative Hardening (“ASH”) is
to limit both the reliability and usefulness of any given Spectre gadget
rather than altogether eliminating them. To do this, we wield both the compiler
and kernel to provide both deterministic and strong probabilistic guarantees
about what memory any particular gadget can leak. This forces attackers to
leverage a large number of gadgets which exist in specific memory regions to
reliably leak interesting MTE tags. While this is a significant divergence
from most existing Spectre mitigations which, at great cost, attempt to prevent
the speculative disclosure of any memory (i.e. Speculative Load Hardening), this
tradeoff allows ASH to deliver production ready performance while still strongly
protecting MTE.

### Review:  Kernel memory
Since ASH heavily relies on the layout of memory in the kernel, it is important
to first understand how and where memory is typically allocated in the kernel.

```
┌──────────────────────────────────────────────────────────────────┐                               
│                           Zone Submaps                           │                               
│                                                                  │                               
│ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │   ┌────────────┐ ┌───────────┐
│ │          │ │          │ │          │ │          │ │          │ │   │            │ │           │
│ │   GEN0   │ │   GEN1   │ │   GEN2   │ │   GEN3   │ │   DATA   │ │...│   KMEMPn   │ │   KMEMD   │
│ │          │ │          │ │          │ │          │ │          │ │   │            │ │           │
│ └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘ │   └────────────┘ └───────────┘
└──────────────────────────────────────────────────────────────────┘                               
*** highly simplified                                                                              
```

At a high level the kernel provides two primary facilities for allocation
memory: the zone allocator and the kmem allocator. 

The zone allocator is a slab allocator which vends relatively small, fixed size
tracts of memory. Each “zone” allocates VAs from either the zone data submap (in
the case of data-only allocations) or is randomly assigned on boot to allocate
VA from either the left or the right of one of the four general purpose zone
submaps. The kernel’s kalloc_type allocator builds on top of the zone allocator
by, on boot, binding each type signature to one of a number of per-size class
zones at random. This, in effect, means that each type is drawn from exactly one
of the submap fronts on each boot, though which particular front is
unpredictable. Additional information about the zone allocator and kalloc_type
can be found in the kalloc_type post on the Apple Security Blog [^1].

The kmem allocator is used for making large, page-aligned allocations. VAs for
these allocations are drawn from one of several kmem ranges [^2] depending on the
request. A large data-only allocation will, for example, be made in the
kmem_data range. On the other hand, a large allocation which contains pointers
will be drawn from one of the kmem_ptr_range, though which particular range and
whether it uses the left or right front depends on its per-boot random type
hash.

[^1]: https://security.apple.com/blog/towards-the-next-generation-of-xnu-memory-safety/

[^2]: See `vm_map_range_id_t` for a list of kmem ranges.

### Design
The core idea of the mitigation, similar to VUSec’s TDI [^3], is to limit
out-of-bounds accesses to only reading or writing data within the same submap
front on both the architectural and speculative path. By isolating accesses to
only those in the same allocation “region”, we are able to leverage our wide
variety of existing, mature kernel heap mitigations to either deterministically
or probabilistically render many Spectre gadgets and traditional arbitrary
offset out-of-bounds memory corruption bugs unusable.


```
 Without Apple Speculative Hardening                                                               
┌──────────────────────────────────────────────────────────────────┐                               
│                           Zone Submaps                           │                               
│                                                                  │                               
│ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │   ┌────────────┐ ┌───────────┐
│ │          │ │          │ │          │ │          │ │          │ │   │            │ │           │
│ │   GEN0   │ │   GEN1   │ │   GEN2   │ │   GEN3   │ │   DATA   │ │...│   KMEMPn   │ │   KMEMD   │
│ │          │ │          │ │          │ │          │ │          │ │   │            │ │           │
│ └──────┬──▲┘ └┬────────▲┘ └──────────┘ └─────────▲┘ └┬────────┬┘ │   └▲───────────┘ └───────────┘
└────────┼──┼───┼────────┼─────────────────────────┼───┼────────┼──┘    │                          
         └──┘   └────────┘                         └───┘        └───────┘                          
   Same Front  Cross Front                            Cross Region                                 
       ✓            ✓                                      ✓                                       
                                                                                                   
                                                                                                   
 With Apple Speculative Hardening                                                                  
┌──────────────────────────────────────────────────────────────────┐                               
│                           Zone Submaps                           │                               
│                                                                  │                               
│ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │   ┌────────────┐ ┌───────────┐
│ │          │ │          │ │          │ │          │ │          │ │   │            │ │           │
│ │   GEN0   │ │   GEN1   │ │   GEN2   │ │   GEN3   │ │   DATA   │ │...│   KMEMPn   │ │   KMEMD   │
│ │          │ │          │ │          │ │          │ │          │ │   │            │ │           │
│ └──────┬──▲┘ └┬────────▲┘ └──────────┘ └─────────▲┘ └┬────────┬┘ │   └▲───────────┘ └───────────┘
└────────┼──┼───┼────────┼─────────────────────────┼───┼────────┼──┘    │                          
         └──┘   └x─x─x─x─┘                         └─x─┘        └x─x─x─x┘                          
   Same Front  Cross Front                            Cross Region                                 
       ✓            ✗                                      ✗                                       
```

[^3]: [A. Milburn, E. Van Der Kouwe and C. Giuffrida, "Mitigating Information
Leakage Vulnerabilities with Type-based Data Isolation," *2022 IEEE Symposium on
Security and Privacy (SP)*, San Francisco, CA, USA, 2022, pp. 1049-1065, doi:
10.1109/SP46214.2022.9833675.](http://doi.org/10.1109/SP46214.2022.9833675)

#### Deterministic protection
When allocations are guaranteed be in separate allocation regions (i.e.
kalloc_type and kalloc_data, kalloc_type/kalloc_data and kernel stacks, etc.),
ASH provides deterministic isolation. To understand the power of this isolation,
let’s consider two forms of a hypothetical Spectre gadget which originates from
a buffer allocated in a data-only regions.

In the first case, we consider a speculative out-of-bounds read gadget which
loads a value potentially off the end of our data buffer `buf` and ends up
leaking it into the data cache through a dependent load:

```
/* Speculative out-of-bounds read gadget */
if (idx < len) {
    // Speculatively load a value out-of-bounds
    // `buf` is one of the data-only regions
    uint8_t value = buf[idx];
    // Leak it into the micro-architecture (contrived)
    return table[value << 6];
}
```

On an unmitigated kernel, such a code sequence could be used to leak the
contents of arbitrary kernel memory into side channels. This includes, for
example, any MTE tags included on pointers stored in the general submaps or in
globals, which would allow an attacker to reliably defeat MTE in a variety of
cases.

With the isolation provided by ASH, however, the out-of-bounds access would not
be able read memory outside of the data submap. While an attacker could still
leak information from the data submap, the kernel’s type segregation ensures
that they would not be able to use this gadget to attack MTE tags because the
data submap definitionally must not contain pointers (and any cases where this
is not true are already serious, patchable security issues).

Next, let’s consider a speculative out-of-bounds write gadget which stores an
attacker controlled value potentially off the end of our data buffer `buf`:

```
/* Speculative out-of-bounds write gadget */
if (idx < len) {
    // Speculatively store a value out-of-bounds
    buf[idx] = value;
    ...
    // Load LR from the stack and return to it
    return;
}
```

On an unmitigated kernel, this is even more severe than a read gadget as an
attacker could leverage it to speculatively overwrite the return address on the
kernel stack and cause a later return to branch to an arbitrary location [^4].
This is very useful for attackers as it enables them to use speculative ROP to
construct more powerful and favorable gadgets. The isolation provided by ASH and
the kernel’s segregation between pure data allocations and kernel stacks,
however, deterministically mitigates this attack as the direct cross-region
write is prevented from leading to control over the return address.

[^4]: ARM CPUs which implement FEAT_FPACC_SPEC do not enforce PAC on the
speculative path, and so an attacker does not need to know the signature in
order to generate a valid return address. Enforcing PAC on the speculative path
is problematic as it allows attackers to bruteforce signatures through
micro-architectural side channels (see: [PACMAN: attacking ARM pointer
authentication with speculative
execution](https://doi.org/10.1145/3470496.3527429)).


#### Probabilistic protection
Although allocations of any two particular types from kalloc_type are not
generally guaranteed to be in different regions, ASH’s ability to cage accesses
into a submap front still allows it to provide a host of probabilistic isolation
guarantees.

The first and strongest isolation guarantee that ASH provides for kalloc_type
allocations applies whenever a potential Spectre gadget and the target object
are of different allocation size classes. Since the zone allocator is a slab
allocator, kalloc_type always places types of different size classes into
distinct zones. This is a very useful property in light of the fact that the
kernel randomly binds each zone to one of the general submap fronts on boot.
Since ASH’s isolation guarantees that an out-of-bounds operation can only reach
other locations within the same submap front, a gadget allocated in one zone can
only reach a particular victim type allocated in another zone if both zones
happen to be randomly bound to the same submap front. 

Assuming a uniformly random distribution of zones to fronts, the probability an
attacker is able to successfully leak the contents of a given type when
leveraging `k` Spectre gadgets of all distinct size classes on a kernel with `n`
submap fronts is given as:

$$ Pr[\textrm{At least one zone with a gadget is colocated with the victim zone}] = 1 - \left( \frac{n - 1}{n}\right)^k $$

As of writing, the kernel has `n=8` submap fronts, and so the probability of
success given `k` gadgets can be summarized as:

|k	|Probability of Success	|
|---|-----------------------|
|1  |0.125                  |
|5  |0.48709                |
|10 |0.73692                |
|15 |0.86507                |
|20 |0.93079                |
|25 |0.9645                 |
|30 |0.98179                |
|35 |0.99066                |
|40 |0.99521                |

In this way, attackers would need to implement an enormous number of gadgets
(>25) and dynamically switch between them in order to achieve an acceptably
reliable attack when using gadgets of different size classes than the victim.

As cross-size class attacks are very difficult, attackers may instead attempt to
leverage gadgets which have the same size class as the target object but with
differing type signature groups. Such an attack is complicated by the fact that,
under kalloc_type, allocations of the same size class but with different type
signature groups are randomly assigned to one a handful per-size-class zones.
These zones are, again, then individually bound to one of the general submap
fronts at random. The number of zones available to each size class—and thus the
degree of placement randomization available between equally sized types—is
decided based on the number of type signature groups in each size class. Each
size class will have at least two zones but many of the more populous size
classes have upwards of a dozen zones.

As before, ASH guarantees that an out-of-bounds operation can only reach other
locations on the same front, and so unless one of the zones containing a gadget
is allocated to the same front as the target object, the attack fails. While the
probability of success is highly dependent on the number of zones allocated to a
size class, in general we expect that attacks between allocations of the same
size class have a weaker lower bound. For example, in the worst case scenario
where a size class is allocated only two zones, the probability an attacker is
able to successfully leak the contents of the same-sized target allocation using
`k` gadgets on a kernel with `n` submap fronts is given as:

$$ Pr[\textrm{The victim zone contains zero gadgets and the other zone is not colocated with it}] = 1 - \left( \frac{1}{2}\right)^k \cdot \frac{n-1}{n} $$

With `n=8` submap fronts, the probability of success given `k` gadgets can be
summarized as:

|k  |Probability of Success	|
|---|-----------------------|
|1  |0.5625	                |
|5  |0.97266                |
|10 |0.99915                |
|15 |0.99997                |
|20 |1                      |
|25 |1                      |
|30 |1                      |
|35 |1                      |
|40 |1                      |

While an attacker would still need to leverage an inconvenient number of gadgets
(~5) in order to achieve an acceptably reliable attack, it is within the realm
of practicality. This is mitigated, however, by the fact that finding several
gadgets within a size class with so few types that it received the minimum
permissible zone allocation is also likely to be quite challenging as there are
simply fewer chances to find a practically exploitable gadget. Additionally,
since the probability of success is heavily influenced by the number zones
available for randomization within a size class, those which have more than the
worst case presented here will require many more gadgets in order to achieve an
acceptably reliable attack. As a general benchmark, iOS 18-era kernels have both
a median and a mean of 9 zones per kalloc_type size class, and so most attacks
of this style will be significantly more difficult.

Finally, given the difficulty of leveraging cross-size class and same-size
cross-type signature group attacks gadgets, attackers may instead attempt to use
gadgets which have both the same size and type signature group as the target
allocation. Since kalloc_type randomization operates only at a granularity of
type signature groups, this means that allocations of the same size and type
signature group will always be allocated in the same zone. As such, any gadget
an attacker can find which satisfies this condition will always be on the same
submap front as the target object and thus can be exploited deterministically. 

Despite being a deterministic defeat of ASH, we contend that finding such a
gadget is likely be sufficiently difficult as to not be of serious concern. This
is because, as discussed in the kalloc_type blog post [^1], nearly 71% of type
signature groups contain only a single type signature and the average type
signature group has only 2.4 signatures associated with it. Thus, performing a
same type signature attack against an interesting victim will, on average,
require finding a practically exploitable gadget within a very small set of
types. Further, even if an attacker is able to find a gadget which satisfies
this condition, it only grants them access to one particular target allocation
type and so the gadget is only likely to be useful for a single kernel exploit.
This is a useful property as it requires attackers to find and exploit new
gadgets in each chain, which limits the scalability of such an exploitation
technique.

In short, ASH forces attackers to leverage the greatest number of gadgets when
they have the greatest opportunity to find exploitable gadgets. Conversely, in
cases where ASH cannot force attackers to use a large number of gadgets,
attackers have the least opportunity to find exploitable gadgets. In this way,
ASH provides well-balanced probabilistic protection regardless of how an
attacker chooses gadgets. 

#### Architectural protection
As mentioned earlier, ASH applies on both the speculative and architectural
path. While ASH is primarily intended as a Spectre mitigation, this design
feature means ASH also serves as a quite powerful mitigation against
architectural arbitrary offset out-of-bounds memory corruption bugs because ASH
limits the effective reach of such bugs to only the submap or submap front which
contains the underlying allocation.

This is felt most severely when considering the fact that ASH isolates an
architectural arbitrary offset out-of-bounds write on any of the data heaps to
only targeting that same data heap. As the data heap should not contain pointers
or other pointer-like types (unchecked index or length values, physical
addresses, etc.), an attacker should not be able to escape the data heap and
cause memory corruption elsewhere. Consequently, not only is the original memory
corruption bug itself a patchable security issue but so too is any *technique*
which allows an attacker to meaningfully exploit it.

## Mitigation Implementation
To efficiently implement this design and provide speculative isolation, we had
to make changes across our entire platform.

### XNU
To lay the foundation for this mitigation, we make a few significant changes to
core XNU.

First, we impose a global `(2GB - PAGE_SIZE)` size limit on virtually contiguous
mappings in the kernel [^7]. While seemingly draconian at first blush, such a
limit is fairly reasonable as kernel developers cannot generally rely on such
large contiguous mappings to succeed on a booted system due to heavy kernel VA
fragmentation. Instead, kernel drivers which need to address large amounts of
memory must use the `IOMemoryDescriptor` facility which has functions for
accessing large amounts of potentially non-contiguous memory.

Some core kernel components, however, do rely on potentially very large
contiguous VA regions (vm_page array, zalloc internals, IOMemoryDescriptor
internals, etc.). To retain compatibility and avoid invasive changes, we
introduce an XNU private family of allocator flags (`{Z, KMA, KMS,
KMEM}_NOSOFTLIMIT`) which allow a mapping or allocation request to override the
allocation limit. Overriding the allocation limit, however, brings with it
certain usage restrictions as all non-constant pointer arithmetic with such
allocations must be performed through the `vm_far.h` helper functions. As will
be explained in the compilers section, failure to use these wrappers may cause
valid pointer arithmetic operations to produce corrupted or otherwise surprising
results. 

Second, we modify the kernel memory layout to inject 4GB [^8] of unmapped
padding between each kernel submap and grow the zone submaps such that the two
fronts, under normal conditions, are always more than 4GBs away [^9]. We
implement this padding in vm_kern.c and zalloc.c through the `VM_RANGE_BOXING`
feature. Since this consumes substantially more VA, however, we can only inject
this padding on `ARM_LARGE_MEMORY` configurations which have 41-bits of usable
kernel VA space.  While we have this limitation, it is largely moot as all MTE
supporting kernels use the `ARM_LARGE_MEMORY` configuration, and so ASH is
available where it matters most.

Taking all of these changes together, we are left with a few powerful
invariants. First, except in a handful of well known, kernel internal cases, any
memory access with an offset greater in magnitude than `(2GB - PAGE_SIZE)` is
guaranteed to be illegal. Second, an access with an offset from any in-bounds
pointer of a magnitude less than 4GB will not leave the submap or submap front
which contains that pointer. While we exploit these invariants in much greater
detail in the following sections, this allows us to, for example, safely limit
the offset of all kernel pointer arithmetic to either ±2GB or ±4GB and be
guaranteed that any out-of-bounds access from a legitimate, in-bounds pointer
will not cross into another allocation region. 

[^7]: See `vm_map_kernel_max_simple_mappable_size` and `vm_map_is_map_size_valid`.

[^8]: While these numbers may seem somewhat arbitrary, they were chosen to make
the compiler instrumentation more efficient. This is discussed in the next
section.

[^9]: Although it is possible for the fronts of a submap to come within 4GB of
each other through spraying, given the size of the zone and kmem_ptr submaps and
the difficulty/threat to system stability of spraying >10GB of wired memory
(especially on systems with relatively little DRAM), we leave this attack out of
scope. 

### LLVM
We introduce a new late stage LLVM IR pass which automatically and
efficiently limits the offset of potentially speculatively hazardous pointer
arithmetic operations such that if the operation’s base pointer is legitimate
and in-bounds, the resulting pointer will not cross into another allocation
region. 

To understand the highlights of the pass, its optimizations, and its method of
bounding accesses, let’s consider how the following highly contrived C function
would be processed by the pass:

```
uint64_t *
hella_oob(uint64_t *buf, uint64_t rmw_idx, uint64_t ret_idx) {
    // Double the value at masked rmw_idx
    uint64_t masked_rmw_idx = rmw_idx & 0xff;
    uint64_t x = buf[masked_rmw_idx];
    x *= 2;
    buf[masked_rmw_idx] = x;
    
    // Double the value at rmw_idx
    uint64_t y = buf[rmw_idx];
    y *= 2;
    buf[rmw_idx] = y;

    // Return a new pointer with an offset retrieved from ret_idx
    uint64_t new_offset = buf[ret_idx];
    return buf + new_offset;
}
```

While this function does not itself contain any branches or attempts at bounds
checking, ASH does not rely on or otherwise attempt to leverage the existence of
bounds checks in the original program. The reason for this is two fold. First,
correctly identifying bounds checks is very difficult due to the limits of
static analysis. This is because bounds checks can take many hard to recognize
forms and can often span function boundaries, which even LTO cannot adequately
cope with in all cases. Second, even if bounds checks could be perfectly
identified, ensuring that bounds checks execute *correctly* requires that the
operands of the bounds checks (and, inductively, all the data values which led
to their production) must be correct, and so enforcing the program’s bounds
checks often requires waiting for all previous instructions to complete (at
great performance cost).

Since bounds checks in the original program cannot be relied upon, ASH instead
opts to materialize its own *approximate* bounds checks on every pointer
arithmetic operation which may need it. It does this by bounding the magnitude
of all pointer arithmetic operations to less than 4GB unless the pass can prove
that such hardening is unnecessary. To see this in action, let’s consider this
function’s (slightly cleaned up) LLVM IR as seen by the pass:

```
define dso_local ptr @hella_oob(ptr noundef %buf, i64 noundef %rmw_idx,
    i64 noundef %ret_idx) local_unnamed_addr {
entry:
  // Double the value at masked rmw_idx
  %masked_rmw_idx = and i64 %rmw_idx, 255
  %arrayidx1 = getelementptr inbounds i64, ptr %buf, i64 %masked_rmw_idx
  %x = load i64, ptr %arrayidx1, align 8
  %x_doubled = shl i64 %x, 1
  store i64 %x_doubled, ptr %arrayidx1, align 8
  
  // Double the value at rmw_idx
  %arrayidx2 = getelementptr inbounds i64, ptr %buf, i64 %rmw_idx
  %y = load i64, ptr %arrayidx2, align 8
  %y_doubled = shl i64 %y, 1
  store i64 %y_doubled, ptr %arrayidx2, align 8
  
  // Return a new pointer with an offset retrieved from ret_idx
  %arrayidx3 = getelementptr inbounds i64, ptr %buf, i64 %ret_idx
  %new_offset = load i64, ptr %arrayidx3, align 8
  %new_ptr = getelementptr inbounds i64, ptr %buf, i64 %new_offset
  ret ptr %new_ptr
}
```

To begin, the pass searches for all `GetElementPtr` instructions in the
function. In this case, it would identify `%arrayindex1`, `%arrayindex2`,
`%arrayindex3`, and `%new_ptr`.

For each GEP, it then utilizes LLVM’s Scalar Evolution analysis pass to
determine whether the total absolute offset of each GEP is guaranteed to be
smaller than 4GB [^10]:

```
entry:
  %masked_rmw_idx = and i64 %rmw_idx, 255
  // SCEV: Min offset = 0, max offset = 2040
  %arrayidx1 = getelementptr inbounds i64, ptr %buf, i64 %masked_rmw_idx
  ...
   
  // SCEV: Min offset = INT64_MIN, max offset = INT64_MAX
  %arrayidx2 = getelementptr inbounds i64, ptr %buf, i64 %rmw_idx
  ...
    
  // SCEV: Min offset = INT64_MIN, max offset = INT64_MAX
  %arrayidx3 = getelementptr inbounds i64, ptr %buf, i64 %ret_idx
  ...
  // SCEV: Min offset = INT64_MIN, max offset = INT64_MAX
  %new_ptr = getelementptr inbounds i64, ptr %buf, i64 %new_offset
  ...
```

Since `%arrayidx` uses `%masked_rmw_idx` which has a maximum value of 255 due to
the masking operation, the offset of this GEP is guaranteed to never cross into
another allocation region and so it can be safely ignored. Though not shown
here, SCEV would also allow us to ignore GEPs which use small data types for
indexes (i.e. a `uint16_t` on a pointer with an 8 byte stride) or use all
constant indices. While this optimization generally eliminates a large number of
hardening sites in most normal programs, since SCEV can draw no meaningful
conclusions about the offsets of the other GEPs, the pass assumes they are all
potentially far out-of-bounds and so analysis on them continues.

Next, the pass performs a forward taint analysis on each of the remaining GEPs
to identify data flows originating from each GEP to a variety of potentially
speculatively dangerous sinks. While the complete rule set of this taint
operation is somewhat complex, the policy can generally be summarized as
follows:

* If a GEP eventually flows into a load operation, the data flow is “armed” by
  the load and is assumed to now be carrying far out-of-bounds data. This
  activates the following sub-set of tainting rules:
    * If the flow reaches another load, a branch, a select (as the predicate),
      or a store instruction (either data or address) [^11], the flow terminates
      and it is assumed that out-of-bounds data is leaking into the
      micro-architecture. Hardening is required.
    * If out-of-bounds data flows into a function call as an argument, as the
      target of an indirect call, or is returned from the current function, the
      data flow terminates and it is assumed that the caller or callee will leak
      the far out-of-bounds data. Hardening is required.
    * Otherwise, tainting continues until the data flow ends. If none of the
      above conditions occur, hardening is not required. Although out-of-bounds
      data may have been loaded and operated on, we do not expect the data will
      leak in an exploitable manner.
* If a GEP eventually flows into the address of a store, we assume a far
  out-of-bounds store is occurring and thus the data flow terminates. Hardening
  is required.
* If a GEP eventually flows into a function call as an argument or is returned,
 the data flow terminates and it is assumed that a far out-of-bounds pointer is
 leaving the function. Hardening is required. This is done to allow
 callers/callees to implicitly trust that the pointers passed to them are not
 far out-of-bounds.
* Otherwise, no hardening is necessary.


When applying this tainting to our function, the pass yields the following
analysis of the remaining GEPs:

```
entry:
  ...
  // arrayidx2 is potentially far OOB and initiates a forward taint 
  %arrayidx2 = getelementptr inbounds i64, ptr %buf, i64 %rmw_idx
  // y holds potentially far OOB data due to arrayidx2, arming the flow
  %y = load i64, ptr %arrayidx2, align 8
  // y_doubled holds potentially far OOB data due to y but does not itself leak
  // anything as shl is not expected to leak information into the
  // micro-architecture
  %y_doubled = shl i64 %y, 1
  // y_doubled is stored to memory, which is assumed to leak the contents of
  // y_doubled and so arrayidx2 requires hardening.
  // Additionally, the store writes to a location specified by arrayidx2 which
  // is a potentially far OOB location, and so arrayidx2 must be hardened.
  store i64 %y_doubled, ptr %arrayidx2, align 8
  
  // arrayidx3 is potentially far OOB and initiates a forward taint 
  %arrayidx3 = getelementptr inbounds i64, ptr %buf, i64 %ret_idx
  // new_offset holds potentially far OOB data due to arrayidx3, arming the flow
  %new_offset = load i64, ptr %arrayidx3, align 8
  // new_ptr is potentially far OOB and initiates its own a forward taint.
  // Additionally, it is implicated in arrayidx3's taint due to its use of
  // new_offset. GEPIs are not expected to leak but new_ptr is assumed to now
  // contain far OOB data.
  %new_ptr = getelementptr inbounds i64, ptr %buf, i64 %new_offset
  // new_ptr is returned, which causes the pass to assume that our caller will
  // both leak its value and may dereference it. Since new_ptr is tainted with
  // new_offset's load, arrayidx3 must be hardened. Additionally, since new_ptr
  // is itself potentially pointing to a far OOB location, new_ptr must be
  // hardened as well.
  ret ptr %new_ptr
```

This analysis ultimately determines that `%arrayidx2`, `%arrayidx3`, and
`%new_ptr` must be hardened.

With these potentially hazardous sites in hand, the pass is finally ready to
actually harden the function. To do this, ASH uses two strategies when
hardening. 

The simplest and most efficient hardening mechanism is the “extension”
technique. This technique simply sign or zero extends the accumulated offset of
the GEP such that downstream loads and stores cannot access offsets further than
±2GB (in case of a sign extend) or +4GB (in case of a zero extend). We draw this
distinction and prefer to zero extend offsets which are known to be non-negative
for non-side channel security reasons. Specifically, sign extending unsigned
offsets would make architectural out-of-bound accesses with unsigned offsets
more easily exploitable by allowing them to target locations *before the buffer*
rather than only after it. By zero extending the unsigned offsets, we avoid
making bugs more exploitable.

In either case, this is both highly compact and performant as it typically
requires no more than one extra instruction per hardening site. At a machine
code level, this extension based hardening generally looks as follows:

```
// x0 is a uint64_t ptr, x1 is an int64_t index
// Original, unhardened codegen:
ldr     x0, [x0, x1, LSL #3]

// New, hardened codegen:
lsl     w8, w1, #3
ldr     x0, [x0, w8, SXTW]
```

Such a transformation is legal for any `inbounds` GEP as an observable
difference occurs only with an offset of a magnitude greater than 2GB. Since XNU
will not generally allocate a contiguous allocation of more than `(2GB -
PAGE_SIZE)`, any such offset is guaranteed to be more than one-past-the-end.
Since an inbounds GEP which exceeds one-past-the-end takes the poison value/is
permitted to take any value, extending the offset and corrupting the resulting
pointer is legal. Allocations which override the soft limit may have larger
in-bounds sizes but, with correct usage of vm_far use pointer arithmetic, ASH
will not harden such operations.

That said, while it is *permissible* to corrupt pointers which exceed
one-past-the-end by wrapping the offsets, such a scheme has the downside that
illegal pointer arithmetic will produce silent and hard to understand pointer
corruption. This poses a usability and debuggability problem as pointer misuse
can produce corrupted pointers which are “wrong” but still have a vaguely
plausible value.

At first glance, a reasonable alternative may be to implement a mode where
attempting to add an illegally far out-of-bounds offset to a pointer generates a
trap. Unfortunately, however, this is not an option for two major reasons.
First, despite it having an undefined behavior, idiomatic C/C++ code tends to
assume that pointer arithmetic does not have side effects. This leads to, for
example, programmers computing illegally far out-of-bounds pointers but not
dereferencing them if they are actually out-of-bounds. Second, and most
pressingly, illegally out-of-bounds pointer arithmetic is only allowed to take
any *value* in LLVM. That is, while this has an undefined behavior in C/C++ and
thus would permit trapping, LLVM is more restrictive and does not permit
trapping. Abiding by this rule is important because LLVM, for example, considers
GEPs to be safely software speculatable [^12] and so can hoist them out of a
guarding if-statement.

Instead, we can lean into the fact that LLVM permits illegal pointer arithmetic
to take any value by producing a pointer which will fault when it is
dereferenced. We call this the “poisoning” hardening mode. At a machine code
level, this looks as follows:

```
// x0 is a uint64_t ptr, x1 is an int64_t index
// Original, unhardened codegen:
ldr     x0, [x0, x1, LSL #3]

// New, hardened codegen:
// Produce shifted offset
lsl     x8, x1, #3
// Is offset in range ±2GB?
cmp     x8, w8, SXTW
// Produce extension hardened pointer
add     x9, x0, w8, SXTW
// Produce a pointer with the non-extended offset but poison the top 16-bits
// with 0x2BAD
add     x16, x0, x8
movk    x16, #0x2BAD, LSL #48
// If the offset is in ±2GB, the extension hardened pointer is valid.
// Otherwise, select the poisoned pointer.
csel    x9, x9, x16, eq
ldr     x0, [x9]
```

This hardening produces either the expected pointer value or a pointer with the
expected low 48-bits and a poison value in the top 16-bits which will trigger a
non-canonical address fault if it is dereferenced. While it costs many more
instructions, this addresses the usability issue of the extension scheme as this
hardening scheme ensures that out-of-bounds accesses either behave as expected
or trap.

In either mode, however, we are able to perform pointer arithmetic such that
memory operations cannot access far out-of-bounds offsets under speculation. If
we apply the extension hardening mode to the original program, we are left with
the following machine code:

```
_hella_oob:
    // x0 is buf, x1 rmw_idx, x2 is ret_idx
    // Double the value at masked rmw_idx
    // This sequence did not require any hardening as it cannot be far OOB
    and    x8, x1, #0xff
    ldr    x9, [x0, x8, LSL #3]
    lsl    x9, x9, #1
    str    x9, [x0, x8, LSL #3]

    // Double the value at rmw_idx
    lsl    x8, x1, #3
    ldr    x9, [x0, w8, SXTW]
    lsl    x9, x9, #1
    str    x9, [x0, w8, SXTW]

    // Return a new pointer with an offset retrieved from ret_idx
    lsl    x8, x2, #3
    ldr    x8, [x0, w8, SXTW]
    lsl    x8, x8, #3
    add    x0, x0, w8, SXTW
    
    ret
```

With these changes, the function now enforces the desired isolation properties
even under speculation.

[^10]: Scalar Evolution presently does not take control flow into account. As
such, we expect that—in practice—its bounds are valid *even under speculation*. 

[^11]: Though storing data does not generally leak the contents of the store, we
consider it “leaked” as we cannot reliably taint across stores to potential
loads. It is possible, for example, that secret data may be speculatively stored
to a non-far location, forwarded to a subsequent load (whether correctly or
otherwise), and leaked. We also apply this rule as blocking cross-region
out-of-bound stores is generally useful from an architectural security
perspective.

[^12]: Software speculation is distinct from hardware speculation. Compilers
will use software speculation to perform operations without knowing for certain
that the results are useful in an effort to increase parallelism and break
long-latency dependency chains.
