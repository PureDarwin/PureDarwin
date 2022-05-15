##############################################################################
# Top-level targets executed by XBS for the internal build of libc++ and libc++abi
##############################################################################

# Declare 'install' target first to make it default.
install:

# Eventually we'll also want llvm/cmake and pieces, but for now keep this
# standalone.
installsrc-paths := libcxx libcxxabi llvm
include apple-xbs-support/helpers/installsrc.mk

.PHONY: installsrc
installsrc: installsrc-helper

.PHONY: install
install:
	@echo "Installing libc++.dylib and libc++abi.dylib"
	"${SRCROOT}/libcxx/utils/ci/apple-install-libcxx.sh"	\
			--llvm-root "${SRCROOT}"						\
			--build-dir "${OBJROOT}"						\
			--install-dir "${DSTROOT}"						\
			--symbols-dir "${SYMROOT}"						\
			--sdk $(shell /usr/libexec/PlistBuddy -c "Print :CanonicalName string" "${SDKROOT}/SDKSettings.plist") \
			--architectures "${RC_ARCHS}"					\
			--version "${RC_ProjectSourceVersion}"			\
			--cache "${SRCROOT}/libcxx/cmake/caches/Apple.cmake"

.PHONY: libcxx_dyld
libcxx_dyld:
	@echo "Installing the various libc++abi-static.a for dyld"
	"${SRCROOT}/libcxx/utils/ci/apple-install-libcxxabi-dyld.sh"	\
			--llvm-root "${SRCROOT}"								\
			--build-dir "${OBJROOT}"								\
			--install-dir "${DSTROOT}"								\
			--sdk $(shell /usr/libexec/PlistBuddy -c "Print :CanonicalName string" "${SDKROOT}/SDKSettings.plist") \
			--architectures "${RC_ARCHS}"

.PHONY: libcxx_driverkit
libcxx_driverkit:
	@echo "Installing DriverKit libc++.dylib and libc++abi.dylib"
	"${SRCROOT}/libcxx/utils/ci/apple-install-libcxx.sh"	\
			--llvm-root "${SRCROOT}"						\
			--build-dir "${OBJROOT}"						\
			--install-dir "${DSTROOT}/System/DriverKit"		\
			--symbols-dir "${SYMROOT}"						\
			--sdk $(shell /usr/libexec/PlistBuddy -c "Print :CanonicalName string" "${SDKROOT}/SDKSettings.plist") \
			--architectures "${RC_ARCHS}"					\
			--version "${RC_ProjectSourceVersion}"			\
			--cache "${SRCROOT}/libcxx/cmake/caches/AppleDriverKit.cmake"

.PHONY: installhdrs
installhdrs: install
	rm -r "${DSTROOT}/usr/lib" "${DSTROOT}/usr/local"

.PHONY: installhdrs_dyld
installhdrs_dyld:
	@echo "There are no headers to install for dyld's libc++abi"

.PHONY: installhdrs_driverkit
installhdrs_driverkit: libcxx_driverkit
	rm -r "${DSTROOT}/System/DriverKit/usr/lib" "${DSTROOT}/System/DriverKit/Runtime/usr/local"

.PHONY: clean
clean:
	@echo "Nothing to clean"
