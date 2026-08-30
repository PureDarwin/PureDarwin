# Pageout Scan

The design of Mach VM's paging algorithm (implemented in `vm_pageout_scan()`).

## Start/Stop Conditions

When a thread needs a free page it calls `vm_page_grab[_options]()`. If the
system is running low on free pages for use (i.e. 
`vm_page_free_count < vm_page_free_reserved`), the faulting thread will block in
`vm_page_wait()`. A subset of privileged (`TH_OPT_VMPRIV`) VM threads may
continue continue grabbing "reserved" pages without blocking.

Whenever a page is grabbed and the free page count is nearing its floor
(`vm_page_free_count < vm_page_free_min`), a wakeup is immediately issued to
the pageout thread (`VM_pageout_scan`) is woken. `VM_pageout_scan` is
responsible for freeing clean pages and choosing dirty pages to evict so that
incoming page demand can be satisfied. 

The pageout thread will continue scanning for pages to evict until all of the
following conditions are met:
1. The free page count has reached its target (`vm_page_free_count >=
vm_page_free_target`)\*
2. there are no privileged threads waiting for pages (indicated by
`vm_page_free_wanted_privileged`)
3. there are no unprivileged threads waiting for pages (indicated by
`vm_page_free_wanted`)

\*Invariant: `vm_page_free_target > vm_page_free_min > vm_page_free_reserved`

## A Note on Complexity
The state machine is complex and can be difficult to predict. This document
serves as a high-level overview of the algorithm. Even seemingly minor
changes to tuning can result in drastic behavioral differences when
the system is pushed to the extreme.

## Contribution Guidelines (Internal)

1. The `return_from_scan` label is the only spot where `vm_pageout_scan()`
will stop. A single exit path makes for readability and understandability. Try
to keep it that way.
2. Try to reduce the use of backwards `goto`s. Great care has been taken to
remove these patterns. Don't regress readability! A to-be-completed
[refactor](https://stashweb.sd.apple.com/projects/COREOS/repos/xnu/pull-requests/21219/overview)
removes the remaining backwards `goto`s.
3. Be wary of 2nd order effects. For example:
  - How might a bias towards paging anonymous memory affect jetsam? Too many
  file backed pages may preclude jetsam from running and leave the system
  unresponsive because of constant pageout/compressor activity
  - How will varying compression ratios change the effectiveness of the
  pageout algorithm? A bias towards anonymous pages may result in quicker
  exhaustion of the compressor pool and increased memory pressure from the
  resident compressed pages.

It is critical that the pageout thread not block except as dictated by its
state machine (e.g. to yield VM locks, to wait until the free page pool is
depleted). Be very wary of introducing any new synchronization dependencies
outside of the VM.

## The Pageout Algorithm
This section documents xnu's page eviction algorithm (`VM_pageout_scan`). It is broken into 5 "phases."

### Phase 1 - Initialization & Rapid Reclamation
* Initialize the relevant page targets that will guide the algorithm
(`vps_init_page_targets()`). This determines how much anonymous memory and
speculative memory to keep around. Look at the refactor #2 for a more cohesive
collection of all the target page threshold calculations.
* Initialize the Flow Control machine to its default state (`FCS_IDLE`).
* Reclaim "cheap" memory from any other subsystems. These must be fast and non-blocking.
  - `pmap_release_pages_fast()`

**Note**: Phase 2 - 5 comprise the "FOR" loop in PageoutScan. The PageQ lock
(`vm_page_queue_lock`) is held for most of this loop.

### Phase 2
Check to see if we need to drop the PageQ lock:
- We have been holding for quite some time. The compressor/compactor
  may need it.
- Drop the lock, free any pages we might have accumulated (usually
  after a few iterations through the loop)
- Wake up the compactor and try to retake the lock. If the compactor
  needed it, it would have grabbed it and we might block.
- We need a vm-object lock but another thread is holding it. That thread
  may also need the PageQ lock.
- Drop the PageQ lock for 10us and try again.
- Another thread (usually the NVMe driver) is waiting for the PageQ lock so
  it can free some pages back to the VM. Yield the PageQ lock and see if that
  helps.

General Page Q management:
1. Check for overflow secluded pages (secluded count > secluded target) to push
   to the active queue.
2. Deactivate a single page. This deactivated page should "balance" the reactivated
   or reclaimed page that we remove from one of the inactive/anonymous queues below.
3. Are we done? (`return_from_scan`)?
4. Check for:
  - "ripe" purgeable vm-object.
  - a speculative queue to age
  - a vm-object in the object cache to evict
5. If we found any actions to take in step 4, repeat Phase 2. Else, continue
   to Phase 3.

### Phase 3
The following page queues are eligible to be reclaimed from:
- Inactive Queue: deactivated file-backed pages
- Speculative Queue: file-backed pages which have never been activated. These
  are generally generated by read-ahead.
- Anonymous Queue: deactivated anonymous pages
- Cleaned Queue: File backed pages that have been "cleaned" by writing their
  contents back to disk and are now reclaimable. This queue is no longer used.
 
1. Update the file cache targets. (TODO: how?)
2. Check the Flow Control state machine to evaluate if we should block to
   allow the rest of the system to make forward progress.
   - If the queues of interest are all empty, block for 50ms. There is nothing
     `pageout_scan` can do, but the other VM threads may be able to make progress.
   - If we have evaluated a significant number of pages without making *any*
     progress (reactivations or frees), block for 1ms.
   - If the compressor queues are full ("throttled"):
     - `FCS_IDLE`: There are plenty of file-backed pages, bias the loop towards reclaiming these 
     - `FCS_DELAYED`: If the deadlock-detection period has elapsed then wakeup
       the garbage collector, increase the reclamation target by 100, and
       change state to `FCS_DEADLOCK_DETECTED`. Else, block.
    - `FCS_DEADLOCK_DETECTED`: If the reclamation target is met, change state
      back to `FCS_DELAYED`. Else, restart from Phase 2.

### Phase 4
We must now choose a "victim" page to attempt to reclaim. If a candidate page
has been referenced since deactivation, it will be reactivated (barring
certain "force-reclaim" conditions).

1. Look for clean or speculative pages (unless we specifically want an
   anonymous one).
2. On non-app-swap systems (macOS), look for a "self-donated" page.
3. Look for a background page. On Intel systems, we heavily bias towards
   background pages during dark-wake mode to ensure background tasks (e.g.
   Software Update) do not disrupt the user's normal working set.
4. Look for 2 anonymous pages for every 1 file-backed page.\* This ratio comes
   from the days of spinning disks and software compression, where re-faulting a
   file-backed page was roughly twice as costly as an anonymous one.
5. If steps 1-4 could not find an unreferenced page, restart from Phase 2.

\* Certain extreme conditions may cause the 2:1 ratio to be ignored:
  - The file-cache has fallen below its minimum size -> choose anonymous
  - The number of inactive file-backed pages is less than 50% of all
    file-backed pages -> choose anonymous
  - The free page count is dangerously low (compression may require free pages
    to compress into) -> choose file-backed

### Phase 5
We have found a victim page, and will now attempt to reclaim it. "Freed" pages
are placed on a thread-local free queue to be freed to the global free queue
in batches during Phase 2.

1. Pull the page off of its current queue.
2. *Try* to take the vm-object lock corresponding to the victim page. Note
   that this is an inversion of the typical lock ordering (vm-object ->
   page-queues). As such, `pageout_scan` cannot block if the lock is currently
   held by another thread. If it cannot take the vm-object lock, then identify
   another potential victim page via Phase 4 and tell the system that a
   "privileged" thread wants its vm-object lock (precluding other threads
   from taking the lock until the privileged thread has had an opportunity
   to take it), drop the PageQ lock, pause for 10µs, and restart from Phase 2.
3. Evaluate the page's current state:
  - `busy`: this page is being transiently operated on by another thread,
    place it back on its queue and restart from Phase 2.
  - `free_when_done`/`cleaning`: this page is about to be freed by another
    thread. Skip it and restart from Phase 2.
  - `error`/`absent`/`pager==NULL`/`object==NULL`: this page can be freed
    without any cleaning. Free the page.
  - `purgeable(empty)`: object has already been purged, free the page.
  - `purgeable(volatile)`: We'll purge this object wholesale once it is ripe,
    so compressing it now isn't worth the work. Skip this page and restart
    from Phase 2.
4. Check (with the pmap) if the page has been modified or referenced.
5. If the page has been referenced since we identified it as a victim, consider
   reactivating it. If we have consecutively re-activated a sufficient number
   of pages, then reclaim the page anyway to ensure forward progress is made.\*
   On embedded systems, a sufficient number of these forced reclamations will
   trigger jetsams. Pages which were first faulted by real-time threads are
   exempted from these forced reclamations to prevent audio glitches.
6. Disconnect the page from all page-table and virtual mappings. If it is
   anonymous, leave a breadcrumb in the page table entry for memory accounting
   purposes.
7. If the page is clean, free it.
8. Otherwise, the page is dirty and needs to be "cleaned" before it can be reclaimed.
   Place it on the relevant pageout queue (i.e. compressor for anonymous and external
   for file-backed) and wakeup the relevant VM thread.
9. Restart from Phase 2.

\* This can happen when the working set turns over rapidly or the system is
seriously overcommited. In such cases, we can't rely on the LRU approximation
to identified "good" victims and need to reclaim whatever we can find.

## Historical Experiments

### Latency-based Jetsam
By placing a "fake" page in the active page queue with an associated timestamp,
we can track the rate of paging by measuring how long it takes for the page to
be identified as a victim by `pageout_scan`. A rapid paging rate indicates
that the system cannot keep up with memory demand via paging alone. In such
cases, jetsams would be invoked directly by `pageout_scan` to free larger
amounts of memory and reduce demand.

Experiments with this implementation highlighted that many iterations of
`pageout_scan` are required before the latency-detection mechanism will
trigger. The delay imposed by these LPF-characteristics was often larger than
the existing page-shortage mechanism and regressed use cases like Camera launch.
Further, performing kills directly on the pageout thread added significant
latency.

Re-introducing the paging-rate measurement without the jetsam-trigger may be
worthwhile for diagnosing system health.

### Dynamic Scheduling Priority
In theory, a misbehaving low-priority thread can generate lots of page demand,
invoking `pageout_scan` to run at a very high priority (91). Thus, the low-priority
thread can effectively preempt higher-priority user threads and starve them of
the core(s) used by the VM thread(s). This can be mitigated by using
propagating the priority of threads waiting on free pages to `pageout_scan`,
allowing `pageout_scan` to only run at a priority as high as its highest waiter.

This approach was enabled on low core-count devices (i.e. watches) for 1-2
years. However, it eventually appeared to contribute to audio glitches and had
to be disabled.

In general, *any* page-wait (even short ones) can be catastrophic for latency
sensitive/real-time threads, especially if those threads will also have to
wait for an I/O to complete after the page-wait. By slowing the preemptive
paging done without any waiters (at `pageout_scan`'s now low base priority),
the likelihood of page-waits increases.

