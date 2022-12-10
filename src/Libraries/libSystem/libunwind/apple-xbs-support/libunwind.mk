##############################################################################
# Top-level targets executed by XBS for the internal build of libunwind
##############################################################################

# Declare 'install' target first to make it default.
install:

# Eventually we'll also want llvm/cmake and pieces, but for now keep this
# standalone.
installsrc-paths := libunwind
include apple-xbs-support/helpers/installsrc.mk

.PHONY: installsrc
installsrc: installsrc-helper

.PHONY: install
install:
	@echo "Installing libunwind.dylib"
	"${SRCROOT}/libunwind/apple-install-libunwind.sh" $(INSTALLFLAGS)

.PHONY: installapi
installapi:
	@echo "We don't currently perform an installapi step here"

.PHONY: installhdrs
installhdrs:
	@echo "We don't currently install the libunwind headers here"

.PHONY: clean
clean:
	@echo "Nothing to clean"
