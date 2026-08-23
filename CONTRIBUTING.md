# Contributing to PureDarwin

Thanks for wanting to help. This document covers what we accept, from whom, and under
what conditions. Read it before opening a pull request; some categories of change are
closed without review, and it is better that you know that now than after the work.

Those categories, and the reasoning behind each, are:

* [Kernel and system-library changes from first-time contributors](#what-new-contributors-may-work-on),
  with an [exemption for additive libSystem integration](#the-libsystem-exemption).
* [Code whose licensing is unknown or unassured](#provenance-and-licensing).
* [Changes the submitter cannot explain or defend in review](#how-this-is-enforced),
  whether or not a model was involved.

`CODING_STYLE.md` covers how code should look. This document covers everything else.

## Branches

Development happens on `next`. Each release is frozen onto `main`.

Open pull requests against `next`. `main` is the last frozen release and is not a
development branch. When the tree is in the shape a release needs, `next` is merged to
`main` and frozen there, so cleanup and stabilisation work also goes to `next`.

## What new contributors may work on

**If this is your first contribution, work on Nix packages.**

Packaging is where new contributors are genuinely useful immediately: porting software,
fixing a build, adding an arm64 variant, getting something onto an image. It is also
where a mistake is cheap and easy to review.

**First-time contributors may not modify existing kernel or system-library
implementations. Changes in those areas will be closed without review.**

This is not a judgement of your ability. It is about blast radius. These components all
depend on each other, and a one-line change in libSystem can take out the entire
system: eight bugs at once, in eight places that look unrelated to what you touched,
each of them a pain to trace back. That is the normal outcome, not the worst case, and
it is often only visible on real hardware, weeks later. Reviewing such a change
properly costs more than writing it.

The way in is the same as it is anywhere: do the packaging work, be around, and the
restriction stops applying.

### How the restriction lifts

It lifts on maintainer judgement, not on a counter. There is no number of merged pull
requests that earns access to the kernel, and one merged package is not a
qualification for XNU surgery. It lifts once we know how you work:

* you have shipped a few changes;
* your patches do what you said they do;
* you responded to review;
* you know the tree well enough to know what is dangerous in it.

It also lifts by area rather than all at once. Someone trusted in the graphics stack is
not thereby trusted in the VM subsystem, and neither are we, which is why this is a
conversation rather than a permission bit.

If you think you are ready, or you have a change in a restricted area you believe is
worth making, raise it first: open an issue or bring it up in Discord and describe what
you want to change and why.

**Asking first is not an approval ceremony.**
Restricted areas have unusually high review costs and often contain constraints that
are not obvious from the code. A short discussion can establish whether the approach is
viable before either you or a maintainer spends significant time on it. You are not
being asked to request permission to have an idea, and nobody is going to hold a
well-explained patch against you for existing.

### The libSystem exemption

Additive libSystem integration work is exempt from the restriction on kernel and
system-library changes.

Adding an existing library or component to the libSystem build, to its exports, or to
the integration machinery around it is permitted, **provided the contribution does not
modify the underlying library's behaviour.**

The distinction matters. Wiring up a component that already exists, or exporting a
symbol that is already implemented, is integration work: it is mechanical, it is
reviewable, and missing exports are one of our most common recurring bugs. Changing
what that component does is an implementation change, and the restriction applies.

If you are unsure which side of the line your change falls on, ask before you write it.

## Provenance and licensing

This is the part we are strict about, and the reason is legal rather than technical.

**Every contribution must be yours to give, under a licence compatible with the
project.** If you cannot say where a piece of code came from and under what terms, we
cannot take it. That is not negotiable, and it is not something a maintainer can waive
on your behalf.

**Code of unknown or unassured licensing cannot be accepted, and cannot be looked at.**
If there is any doubt about the origin of something, do not paste it into an issue, a
pull request or a chat channel. Once a maintainer has seen it, the question of whether
the project's own implementation was influenced by it becomes a real one, and that is a
problem no amount of later cleanup solves. Leaked Apple source, decompiled Apple
binaries, or code derived from proprietary Apple implementation material falls squarely
in this category. Do not provide it to project maintainers.

When you vendor third-party code, say where it came from and include its licence.

### Clean-room work

Clean-room implementation is permitted, and a good deal of this project depends on it.
Drivers written from a manufacturer's datasheet, code written against a published
standard, and work based on observing how real hardware behaves are all legitimate.

The line is **implementation material versus interface knowledge.** Learning what a
register does, what a protocol looks like on the wire, what a structure layout must be
for something to interoperate, or how a device responds to a given sequence, is
interface knowledge, and you may write from it. Reading someone's proprietary
implementation and writing code from what you saw is derivation, whatever route you
took to see it.

Because the distinction is easy to state and easy to get wrong under pressure, be
careful in practice:

* **Work from sources you can name.** Datasheets, published specifications, header
  files under a licence that allows it, official documentation, your own experiments
  against hardware you own.
* **Say what you used.** A clean-room contribution should cite its sources in the
  commit message or a comment: document number, revision, specification section. The
  LAN78xx driver naming its datasheet is the pattern to follow.
* **Do not go looking at proprietary implementations "just to check".** If you have
  already seen one, say so before you write the code, not after. That is a
  conversation, not an accusation, and it decides whether someone else should write
  that particular piece.
* **Ask when you are unsure.** If you cannot tell whether what you have is interface
  knowledge or derivation, say so in the pull request description: name the sources you
  worked from and the specific parts you are unsure about. An issue or a Discord
  message works too if you would rather settle it before writing anything. Either way
  it belongs somewhere the whole project can see, not in a private message about one
  line of code. Getting this wrong is expensive for everyone, not just for your patch.

## AI-assisted contributions

We are not opposed to AI-assisted, LLM-generated or otherwise machine-written code. The
tool you used is your business.

The question we care about is the same one we ask of every contribution, assisted or
not:

**Can you stand behind this as your own work, licensed appropriately, and have you
reviewed it yourself?**

If yes, submit it. If no, we want nothing to do with it, because what you are handing
us is an unreviewed change of unclear provenance, and that is a legal mess we are not
willing to inherit.

Concretely, if you used a model:

* **You are responsible for the contribution.** For purposes of review, there is no
  distinction between code you wrote unaided and code produced with assistance from a
  model. You must understand it, defend it, and take responsibility for what you
  submit. "I do not know, the model wrote it" ends the review.
* **You have reviewed the output line by line.** Not skimmed it, not "the tests passed".
* **You are confident it is not reproducing licensed code from elsewhere.** If you
  cannot be confident of that, see the provenance section: we cannot accept it, and we
  would rather not have seen it.
* **For non-trivial changes to existing code, inspect the history of the code you are
  changing and understand why the current implementation exists. This applies both to
  you and to any model assisting you.** A great deal of this tree looks wrong until you
  know why it is that way: most of the odd-looking code is a fix for a specific bug,
  and the commit that introduced it says so. A change that reverts one of those,
  confidently and with a tidy explanation, is the single most common failure mode of
  assisted contributions. Read `git log` and `git blame` on what you are touching.

### How this is enforced

There is no detector and we are not going to pretend otherwise. What happens instead is
simple: **you will be asked questions about your patch, and if you cannot answer them,
the pull request is closed as-is.**

Not trick questions. Why this approach over the obvious alternative, what happens in
some edge case, why a particular line is there. The kind of thing anyone who
understands the change should be able to answer without preparation, and the kind of
thing someone merely submitting generated or copied code usually cannot.

None of this is aimed at people who use models. It is the standard for everyone. A
person who hand-writes a patch they do not really understand, or who copies one from a
forum post and files it, is in exactly the same position, and gets exactly the same
answer.

## Testing your change

Build it and boot it before you send it:

```
nix build .#image-minimal
nix run .#vm
```

**If you touched anything shared between architectures, build and boot x86_64.** Most
of the kernel and kext tree is shared, and ARM work leaking arm-only code into shared
files is the most common way the primary target breaks.

Testing under both KVM (`nix run .#kvm`) and plain QEMU with TCG (`nix run .#vm`) is
recommended, but not required. The two exercise different paths, and something that
works under one can fail under the other. In practice the lead maintainer does not test
TCG routinely, on the grounds of not working on the things that tend to break it, so a
TCG regression can sit unnoticed for a while. If your change is in an area where the
difference plausibly matters, running both is a real contribution.

Note that the flake reads the tree through `git+file://`. Files you have not staged are
invisible to the build, so stage your work first or you will test the old tree.

Nix is the build system. There is no supported way to build this tree by invoking
`cmake` yourself, and bug reports against a hand-rolled CMake configure will not be
actioned.

## Pull requests

* One logical change per pull request.
* Keep formatting and cleanup commits separate from functional ones.
* Say what you tested, on which target, and how.
* If the change is hardware-specific, say which machine.

## Reporting bugs

Include the target, the image, whether it was QEMU or real hardware, the boot-args in
use, and the console output as text where you can get it. A report of a boot that stops
with no output is very hard to act on.

## Conduct

`CODE_OF_CONDUCT.md` applies to every space the project uses.
