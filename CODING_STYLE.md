# PureDarwin Coding Style

This document is a work in progress. Please follow it at all times.

If you are editing anything that does not already conform
to the below standards, please follow the formatting that is in use there instead.
Feel free to clean up a file that you are editing; just make sure that your functional changes are in a separate commit.
Cleanup-only PRs are also welcome.

An editorconfig file is provided to help contributors to adhere to these rules.

## All files

* No trailing whitespace, including on blank lines. If your editor is programmed to automatically strip trailing whitespace when you save, please commit the whitespace changes separately from the functional changes for ease of review.
* All files should always end with one (and only one) newline. No blank lines at the end of a file.

## The Build

* Final PRs as-merged must use the CMake build system only. The system is not fully implemented yet, but is designed to be a straight port from Xcode once you know what to put in CMakeLists.txt, while still abiding by standard CMake idioms.
* As a corollary to the above: No Xcode projects in `main` branch. These are perfectly OK in topic branches and PRs, but please remove them before merging. This helps prevent noise in future diffs due to Xcode rewriting project files at its whim. The gitignore file will help to enforce this; note that you can easily override it using `git add -f`.
* Contributions to PureDarwin that have dependencies not present in this repo are welcome, but are still subject to the above rules. The `USE_HOST_SDK` parameter can be used to enable use of the active Apple SDK if a file is not found in the local repo. Please use it in all targets unless otherwise specified.

## CMake

* Four-space indent; the editorconfig file should help enforce this.
* Use single blank lines where appropriate to aid readability.
* `add_subdirectory()` calls in CMakeLists.txt files should be sorted alphabetically, with uppercase letters coming before lowercase letters.
* Always name targets after the name of their output binary. If you are building a Darwin component target, do include “lib” if it is present in the output filename; CMake will detect this and set the `PREFIX` property accordingly to prevent “lib” appearing twice in the filename. This will _not_ automatically occur for host tool targets, so omit “lib” from target names there.
* When adding compiler definitions, keep them alphabetized unless it breaks something.
* Try to keep include directories alphabetized in the CMakeLists file, but there are places where doing so will break the build.
* Separate build rules for separate binaries with two blank lines. Put `install()` and `add_subdirectory()` calls together in their own sections at the end of the file, separated by two blank lines from the others.
* Look in the source files in the scripts/cmake folder for documentation about what each function or macro does.
* Binaries should be installed using the standard CMake `install()` command, using the BaseSystem component. Headers should be installed in DeveloperTools. Private headers use the DeveloperInternal component. Manpages go in either BaseSystem or DeveloperTools, depending on what they document.

## Kernel Extensions

When modifying kernel-mode C++ code keep your use of newer C++ features as minimal as possible, in line with the design of existing kext code. Remember that there is no STL or libcxxabi in kernel mode. Some more points:
* `OSSharedPtr` is there for a reason; use it!
* Passing objects by reference (`something_t &`) is OK, in implementation declarations only. Do not create declarations (parameters and return values) that use `OSSharedPtr` or by-reference arguments in the signature. This is not ABI-stable.
* Do not bother to imitate any strange formatting you might see when editing existing files. Keep the indentation the same (incl. tabs-vs-spaces), but feel free to use whatever you believe to be good formatting is when writing new code or editing existing code.
* We use C++11 to compile for all kexts, so use of features from that standard are OK. (And recommended! The use of newer C++ syntax is a good development pratice.)
