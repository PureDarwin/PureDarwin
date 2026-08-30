Mach IPC Security concepts
==========================

This documentation aims at documenting various security concepts in this
subsystem. Each section covers a single concept, and will cover topics such as
motivation, design of the feature, and implementation details that are
important.


## IPC space policy

### Motivation and design

Over time our IPC policies have grown in complexity and depend on several
parameters, to name a few: being a simulated process, a platform binary, or
having browser entitlements.

As a result, a notion of IPC space policy exists that projects all various
system policies into a single enum per IPC space. This policy is an inherent
immutable property of an IPC space which allows to query its value without
holding any locks.


### Implementation details

The source of truth for IPC policies is the `struct ipc_space::is_policy` field,
which can be accessed with the `ipc_space_policy()` accessor.

This field is computed when a task IPC space is enabled (in
`ipc_task_enable()`), and is immutable for the lifetime of this space. In
addition to that, the field is dPACed in order to be resilient to early memory
corruption primitives.

For conveniency reasons, the policy bits of a space are injected in other enums
(such as `mach_msg_options64_t`).  The `IPC_SPACE_POLICY_BASE()` macro helps
forming types that extend the space policy.


## Pinned Entries

### Motivation and design

Certain kinds of send rights have a well understood lifecycle on the system,
during which there must always be an extent send right alive for the port.
Obvious examples of this are task or thread control ports which must have
a live send right in their corresponding IPC space while the task or thread
they reference is alive.

In order to catch port management issues that could lead to various confused
deputies issues, the Mach IPC subsystem provides a notion of pinned send rights.
Pinned send rights is a concept of the Mach IPC Entry, which denotes that this
entry must always have at least one extent send right alive.

Pinning can be undone in two ways:

- when a port receive right is destroyed, pinning is no longer effective,
  and entries will be automatically unpinned as part of the dead-name check;
- unpinning can be explicitly requested by the kernel.


### When and how to used pinned rights?

Pinned rights were designed to protect `mach_task_self()` and
`_pthread_mach_thread_self_direct()` which can lead to grave security bugs when
port lifecycle management mistakes are made. The bracketing there is very
simple:

- task ports are never unpinned;
- thread ports are unpinned when the thread terminates.


There might be other ports on the system which can use this facility, however
they must have the right shape: either the port dying (the receive right being
destroyed) is an adequate way to unpin the entry, or there must be a clearly
identified kernel path that can unpin the entry without any confusion with other
ports.

Adding unpinning paths that can't verify that the port being unpinned is
"theirs" would lead to weakening this feature and would reintroduce avenues
to confuse the system due to port mismanagement bugs.


### Implementation details

Pinning is denoted by the `IE_BITS_PINNED_SEND` bit
of the `struct ipc_entry::ie_bits` field.

IPC entries gain this bit the first time the kernel calls
`ipc_port_copyout_send_pinned()` for a given port and IPC space.

When the `IE_BITS_PINNED_SEND` is set, then the `MACH_PORT_TYPE_SEND` bit must
be set too, with the `IE_BITS_UREFS()` for this entry being at least 1.

In order to respect that pinning is ignored immediately when a port becomes
dead, enforcing `IE_BITS_PINNED_SEND` semantics must be done under the space
lock, either right after a dead-name conversion check happened
(`ipc_right_check()` has been called) or by checking explicitly that the port
is still active (`ip_active()` returns true) when a dead-name conversion isn't
desirable.


### Usage and enforcement

Task and thread control ports are pinned for all processes within the
owning IPC space of the task in question, for all processes on the system.

The `ipc_control_port_options` boot-arg determines the reaction of the system to
violations of pinning:

- hardened processes and above have hard enforcement of pinning rules (violating
  the rules terminates the process);
- other processes have a soft enforcement: violating pinning rules returns a
  `KERN_INVALID_CAPABILITY` error and generates a non fatal guard exception.



