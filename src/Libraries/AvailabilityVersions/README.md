# AvailabilityVersions

## Intro

AvailabilityVersions provides functionality related to the versions numbers of various platforms. These take several forms including:

* Macros for API availability annotation (`API_AVAILABLE()`, etc)
* Version defines used for dyld version checks
* Mappings for aligned OSes and platforms (version sets)

Most of this is programmatically generated with data sourced in the `availability_dsl`. The files in the that folder are read in order.

## Operation

All of the content defined in the dsl is accessed via the `/usr/local/libexec/availability` script. It provides several options for outputing information about versions:

* `--macos`, `--ios`, etc: Outputs a list of versions for the platform
* `--sets`: Outputs YAML describing all the aligned versions of each platforms as version sets
* `--preprocess`: Undocumented flag that parse through an input file and replaces macros in it with contnet derived from the DSL

Build:

The build system is implemented in `CMake`, but a `Makefile` is provided to support B&I builds, and `make install` the recommended way of invoking the build.  The build process is as follows:

1. `$SRCROOT/availability` is invoked to preprocess itself and create `$OBJROOT/availability` that embeds the DSL content directly into the script
2. `$OBJROOT/availability` is called on all the files in `templates` to preprocess their macros and store the results in `$OBJROOT`
3. The install phase then copies all the processed files into the `DSTROOT`

<!--
-->
