##############################################################################
# Top-level targets executed by XBS for internal build of libcompiler_rt.dylib
##############################################################################

MKDIR        := /bin/mkdir -p -m 0755


# Note: cannot use helpers/installsrc.mk because Libcompiler_rt is
# xcode project based and needs a different layout.

.PHONY: installsrc
installsrc:
	@echo "Installing source..."
	$(_v) $(MKDIR) "$(SRCROOT)"
	$(_v) cp -r compiler-rt/Libcompiler_rt.xcodeproj "$(SRCROOT)"
	$(_v) $(MKDIR) "$(SRCROOT)/lib"
	$(_v) cp -r compiler-rt/lib/builtins "$(SRCROOT)/lib/"



