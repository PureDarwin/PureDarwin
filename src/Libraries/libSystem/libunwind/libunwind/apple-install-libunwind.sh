#!/usr/bin/env bash

set -e

if [ ! "${APPLE_XBS_SUPPORT_PRINT_ARGS_AND_EXIT:-0}" = 0 ]; then
    printf "%q" "$0"
    while [ $# -gt 0 ]; do
        printf " %q" "$1"
        shift
    done
    printf "\n"
    exit 0
fi

error() {
    print "error: %s\n" "$*" >&2
    exit 1
}

# Default values.
IS_SIM_BUILD=false
DYLD=false

# Parse command-line.
while [ $# -gt 0 ]; do
    case "$1" in
        --sim) IS_SIM_BUILD=true; shift;;
        --dyld) DYLD=true; shift;;
        -*) error "unrecognized flag '$1'";;
        *) error "unexpected positional argument '$1'";;
    esac
done

run() {
    local arg
    {
        printf "#"
        for arg in "$@"; do
            printf " %q" "$arg"
        done
        printf "\n"
    } >&2

    "$@"
}

function step() {
    separator="$(printf "%0.s-" $(seq 1 ${#1}))"
    echo
    echo "${separator}"
    echo "${1}"
    echo "${separator}"
}

function build() {
    sysroot="${1}"
    arch="${2}"
    config="${3}"
    current_version="${4}"
    build_platform="${5}"

    # Compute additional #defines based on the environment variables set by XBS.
    defines=""
    for var in $(env); do
        if [[ "${var}" =~ RC_(SHOW|HIDE)_.+ ]]; then
            defines+="-D${var};"
        fi
    done
    if [ "${config}" = "static" ]; then
      defines+="-DFOR_DYLD=1;-fno-stack-check"
    else
      defines+="-flto;" # Use LTO for shared library
    fi

    NINJA=`xcrun -f ninja`
    CLANG=`xcrun -sdk ${sysroot} -f clang`
    CLANGXX=`xcrun -sdk ${sysroot} -f clang++`
    GENERATOR=Ninja
    VERBOSE="-v"

    # Configure libunwind.
    # We hijack LD_TRACE_FILE here to avoid writing non-dependences into
    # LD_TRACE_FILE, otherwise there will be lots of false dependency cycles.
    run mkdir -p "${OBJROOT}/${arch}-${config}"
    run pushd "${OBJROOT}/${arch}-${config}"
    run env LD_TRACE_FILE="${OBJROOT}/${arch}-${config}/LD_TRACE" \
        xcrun cmake "${SRCROOT}"/libunwind \
            -G"${GENERATOR}" \
            -C "${SRCROOT}/libunwind/cmake/caches/Apple-${config}.cmake" \
            -DCMAKE_MAKE_PROGRAM=${NINJA} \
            -DCMAKE_C_COMPILER=${CLANG} \
            -DCMAKE_CXX_COMPILER=${CLANGXX} \
            -DCMAKE_INSTALL_PREFIX="${OBJROOT}/${arch}-${config}-install" \
            -DCMAKE_OSX_SYSROOT="${sysroot}" \
            -DCMAKE_OSX_ARCHITECTURES="${arch}" \
            -DLIBUNWIND_COMPILE_FLAGS="${defines}" \
            -DLIBUNWIND_APPLE_PLATFORM="${build_platform}" \
            -DLIBUNWIND_VERSION=${current_version}
    run popd

    run xcrun cmake --build "${OBJROOT}/${arch}-${config}" \
        --target install-unwind -- ${VERBOSE}
}

SYSROOT="${SDKROOT}"
FINAL_DSTROOT="${DSTROOT}"
FINAL_SYMROOT="${SYMROOT}"
CURRENT_VERSION=${RC_ProjectSourceVersion}
BUILD_PLATFORM=${RC_PROJECT_COMPILATION_PLATFORM}

#
# Build the dylib for each architecture and create a universal dylib
#
for arch in ${RC_ARCHS}; do
    if $DYLD; then
        step "Building libunwind.a for architecture ${arch}"
        build "${SYSROOT}" "${arch}" "static" "${CURRENT_VERSION}" "${BUILD_PLATFORM}"
    else
        step "Building libunwind.dylib for architecture ${arch}"
        build "${SYSROOT}" "${arch}" "shared" "${CURRENT_VERSION}" "${BUILD_PLATFORM}"
    fi
done

if ! $DYLD; then
    step "Creating a universal dylib from the dylibs for all architectures"
    input_dylibs=$(for arch in ${RC_ARCHS}; do
        echo "${OBJROOT}/${arch}-shared-install/lib/libunwind.dylib"
    done)
    run xcrun lipo -create ${input_dylibs} -output "${OBJROOT}/libunwind.dylib"

    step "Installing the universal dylib to ${FINAL_DSTROOT}/usr/lib/system"
    run mkdir -p "${FINAL_DSTROOT}/usr/lib/system"
    run cp "${OBJROOT}/libunwind.dylib" "${FINAL_DSTROOT}/usr/lib/system/libunwind.dylib"

    step "Producing dSYM bundle for the universal dylib"
    run xcrun dsymutil "${FINAL_DSTROOT}/usr/lib/system/libunwind.dylib" \
        -o "${FINAL_SYMROOT}/libunwind.dSYM"

    step "Stripping the universal dylib from all debug symbols"
    run xcrun strip -S "${FINAL_DSTROOT}/usr/lib/system/libunwind.dylib"

    # Install headers.
    #
    step "Copy headers"
    run mkdir -p "${FINAL_DSTROOT}/usr/include"
    run cp -r "${SRCROOT}/libunwind/include" "${FINAL_DSTROOT}/usr"
else
    step "Creating universal static library"
    input_static_libs=$(for arch in ${RC_ARCHS}; do
        echo "${OBJROOT}/${arch}-static-install/lib/libunwind.a"
    done)
    run xcrun lipo -create ${input_static_libs} -output "${OBJROOT}/libunwind.a"

    run mkdir -p "${FINAL_DSTROOT}/usr/local/lib/dyld"
    run cp "${OBJROOT}/libunwind.a" "${FINAL_DSTROOT}/usr/local/lib/dyld/libunwind.a"
fi
