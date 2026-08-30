# Coalitions

A look at coalitions and how they're implemented in XNU.


## Overview

A coalition is a group of related processes.

Currently there are two types of coalition: *resource coalitions* and *jetsam coalitions*. Each process on the system is a member of one of each.

Coalition membership is preserved across `fork`, `exec` and `posix_spawn`, and cannot be changed after a process is created.

`launchd` is responsible for creating, destroying and reaping coalitions (via `coalition_create`, `coalition_terminate` and `coalition_reap`). It's also responsible for spawning process into specific coalitions via `posix_spawn`+`posix_spawnattr_setcoalition_np`. These interfaces are for `launchd`'s use only (or unit testing XNU itself). Fundamentally, a coalition is intended to represent a `launchd` job.


Each coalition has a 64-bit ID (both resource and jetsam coalitions share the same ID space). ID 1 includes the `kernel_task` and `launchd`.

Coalition IDs are not re-used (they are assigned monotonically by the kernel). However, since empty coalitions are valid, `launchd` tries to keep a single coalition per loaded service and spawn into an existing coalition rather than create a new one each time the service restarts. Apps usually get a fresh coalition on each launch.

Each process occupies a *role* in its coalition: one process is tagged as the `COALITION_TASKROLE_LEADER`, XPC services have `COALITION_TASKROLE_XPC`, and app extensions have `COALITION_TASKROLE_EXT`, etc.

`launchd` additionally stores a name string for each coalition; this can be obtained via XPC. This comes from the `Label` plist key / bundle ID / App name depending on the type of process. Jetsam coalitions, unlike resource coalitions, have a short/pretty name passed into `DEVELOPMENT` kernels. This is for quick debugging of thread groups and must not be used for any decisions on device.

## Resource Coalitions

Resource coalitions are used by things like Activity Monitor to aggregate CPU usage, energy consumption, I/O etc. The idea is we can make statements like 'Safari is using 50% CPU' even if Safari has 10 different processes using 5% CPU each.

Every few minutes, powerlog samples the resource usage of each coalition on the system. This data ultimately feeds into the Battery UI that's displayed to users.

We also use resource coalitions to drive the 'significant energy usage' report in macOS.

Unlike jetsam coalitions, App extensions (which usually have a different vendor to their host app) are spawned into their own resource coalition for separate tracking.

To query resources used by a given coalition, call `coalition_info_resource_usage`. From the command line you can use `coalitionctl show`.

### Ledgers

A ledger is a lightweight key-value store that we use to track various metrics. Ledgers are created from a template, which determines the set of keys (which we call 'entries'). For example, each task has a ledger containing entries like `cpu_time` and `wired_mem`.

Entries are essentially 64-bit counters that increment monotonically (which is done via `ledger_credit*`). However some fields like `wired_mem` can logically decrease over time; this is implemented by incrementing a second 64-bit counter called 'debit' (via `ledger_debit*`) so that the overall `wired_mem` usage at a moment in time can be computed via `credit - debit` (sometimes called the 'balance').

Since ledgers are fundamentally lock-free, it's possible that readers may see bogus values. For example, if one thread writes entry A and then entry B, a reader thread might only see the update to A. Therefore computing metrics from multiple entries should be done with care.

Ledgers also help us implement resource limits like 'this process should use no more than 10 seconds of CPU time in a 20 second period'. This is implemented via timers that 'refill' ledger debit periodically (see `ledger_refill`). Each credit/debit operation checks if the new balance exceeds the desired limit and invokes appropriate callbacks (e.g to kill the task).

Each thread, task, and resource coalition has its own separate ledger. Thread ledgers currently *only* store `cpu_time`. Task ledgers have many other entries like memory usage, I/O, etc. Resource coalitions have a ledger that's instantiated from the task ledger template. Confusingly there's also a `resource_monitor_ledger` attached to each coalition which has a single `logical_writes` entry.

When `cpu_time` is updated (at context switch), both the task and thread ledger is incremented. Therefore the `cpu_time` in the task's ledger is always equal to the sum of its threads, plus that of any threads that have exited. On the other hand, the coalition's ledger is only updated when a task dies. At this point, all entries in the task's ledger are added into the coalition's ledger (see the `ledger_rollup` call in `i_coal_resource_remove_task`).

### Gathering resource usage

Calculating the current resource usage of a coalition is a surprisingly tricky process, since data is stored in a number of places. The logic for this lives in `coalition_resource_usage_internal`.

Fundamentally the goal is to sum the resources used by all tasks in the coalition, plus that of any dead tasks.

We create a temporary ledger, into which we sum the coalition's ledger (which holds resources from dead tasks), then iterate alive tasks and sum their ledgers too.

Some data like energy usage is tracked by `recount` (see [recount.md](doc/observability/recount.md)) rather than ledgers, so we sum this information in a similar manner.

This is all done while holding the `coalition_lock`, which ensures we don't double count if a task dies while we iterate. Nothing stops a task updating its own ledger while we iterate, and while this could result in bogus data, increments are generally small enough that this is benign.

### CPU time and energy billing

Through the magic of Mach vouchers, XNU can track CPU time and energy consumed *on behalf of* other processes.

For example, suppose a task sends an XPC request to `launchd`, which does some computation and sends a response. We would like to track the computation between request and response as done by the task itself rather than `launchd`.

This information is surfaced in the following members of `struct coalition_resource_usage`:

* `{cpu_time,energy}_billed_to_others` stores the amount of `cpu_time`/`energy` that *we* performed *on behalf of* other coalitions.
* `{cpu_time,energy}_billed_to_me` stores the amount of `cpu_time`/`energy` that *other* coalitions performed for us.
* `cpu_time`/`energy` stores the 'raw' amount of time/energy that was consumed by our processes.

Therefore, the logical amount of `cpu_time`/`energy` that a coalition has consumed can be computed by `cpu_time + cpu_time_billed_to_me - cpu_time_billed_to_others` (and likewise for `energy`).

Note that these fields are summed per-task, so in the case where `x` amount of `cpu_time` is billed between two tasks in the same coalition, one task's `cpu_time_billed_to_me` will increment by `x`, and the other task's `cpu_time_billed_to_others` will *also* increment by `x`. Therefore the coalition's billing-related fields will increment despite no work being done outside of the coalition itself. In other words, both `*_billed_to_{me,others}` must be jointly considered for accurate accounting (as is done by the expression above).

We also surface `gpu_energy`/`gpu_energy_billed_to_{me,others}` but this is updated by the GPU kext rather than XNU. We expose the following KPIs for this purpose:

* `current_energy_id`: returns a unique 'energy ID' for the current task's resource coalition (exposed as an opaque ID)
* `task_id_token_to_energy_id`: produces an energy ID from a particular task port name
* `energy_id_report_energy`: looks up the resource coalition from energy ID, and increments `gpu_energy`/`gpu_energy_billed_to_{me,others}` as appropriate.

ANE billing information is not yet captured: [rdar://122812962](rdar://122812962) (Capture ANE energy billed to me and others per coalition).

### Bank objects

The above fields are tracked by 'bank' objects attached to vouchers, which are sent along with Mach IPC messages. A full treatment of vouchers is beyond the scope of this document, but using our previous 'task doing XPC to `launchd`' example, the mechanism can be summarized as follows:

When `launchd` receives the message, it 'adopts' the voucher which binds the bank to its thread (see `thread->ith_voucher`). When `launchd` is done, the voucher is un-bound. Note that voucher tracking is implemented in libxpc/libdispatch so anyone using those libraries gets this automatically.

Bank objects are split into two halves:

A *bank account* is essentially just a small ledger with entries for `cpu_time` and `energy`. When a thread adopts a voucher with a bank attribute, we point to this ledger in `thread->t_bankledger` (see `bank_swap_thread_bank_ledger`). Then, any updates to the task's `cpu_time` or `energy` are *also* added onto `thread->t_bankledger`.

A *bank task* is an object that provides linkage between a task and its associated *bank accounts*. For example, if we do an IPC to 3 other tasks, we'd like to link ourselves to those 3 separate bank accounts. The `bank_task->bt_accounts_to_{pay,charge}` lists track precisely this information.

Each task is essentially 1-1 with a `bank_task`. However, a `bank_task` may outlive its associated task in case the task dies but is still being billed by someone else.

When a *bank account* is deallocated (due to vouchers getting released, meaning tasks have completed their work), we rollup/sum the accumulated `cpu_time`/`energy` from the bank account as follows:

* The 'bank merchant' (returning to the previous example, this would be `launchd`) has its `*billed_to_others` task ledger entry incremented by the amount stored in the bank's ledger
* The 'bank holder' (this would be the other task) has its `*billed_to_me` task ledger entry incremented by the amount stored in the bank's ledger.

The logic for this lives in `bank_account_dealloc_with_sync`/`bank_rollup_chit_to_task`.

### Bugs

As described above, on-behalf-of accounting is only added into to the task ledgers when bank accounts/vouchers are deallocated. If the voucher has been open for a long time, this may be a large amount of `cpu_time`/`energy` in one instant.

Since `coalition_info_resource_usage` does not try to iterate outstanding bank accounts, this means callers may observe very large increments in `{cpu_time,energy}_billed_to_{me,others}`, especially in the presence of voucher leaks.

On the other hand, the raw `cpu_time`/`energy` values increment pretty much continously. As a result, the logical consumption `cpu_time + cpu_time_billed_to_me - cpu_time_billed_to_others` may appear to go backwards when `*_others` is incremented by a large amount (and likewise for `energy`).

In other words, a process/coalition may appear to *bill out* more cpu_time/energy than it actually consumed in a given period: rdar://92275084 (In coalition_resource_usage, energy_billed_to_others > energy, resulting in a negative number for 'billed_energy'). That'd be like going on a business trip and expensing that fancy dinner you had with your friends two months ago...

Note that the inequality `cpu_time_billed_to_others <= cpu_time` still holds at any given instant (as reported by `coalition_info_resource_usage`), but this is *not* true when looking at deltas between two samples. And likewise, each field increments monotonically, but the overall quantity `cpu_time + cpu_time_billed_to_others - cpu_time_billed_to_others` does not.

Another 'bug' is that if a task dies, and is then subsequently billed for some work by another task, this information is dropped on the floor. This is NTBF but in an ideal world we would track this on the coalition.

## Jetsam Coalitions

Each process is also a member of a jetsam coalition.

This is designed to encapsulate 'an app and all its subprocesses'. For example, App extensions are spawned into separate *resource* coalitions from their host app, but inherit the host app's *jetsam* coalition. XPC services/App extensions can opt out of this via the `_AbandonCoalition` key in `Info.plist`.

The primary function of jetsam coalitions is to aggregate memory usage across entire applications (app level footprint). When jetsam needs to reclaim memory, it tries to kill processes associated with the most memory-intensive visible app.

The scheduler and CLPC also look at jetsam coalitions to determine which processes are P-core eligible. In particular, the thread group of a jetsam coalition led by a P-core capable process will be allowed to use P-cores. This could have been its own coalition type but the rules matched the existing jetsam coalition.



When in Game Mode, jetsam coalitions are used to throttle non-game apps.
