# apple-xbs-support/helpers/installsrc.mk

ifeq "$(installsrc-paths)" ""
$(error installsrc-paths unset; should be paths to include)
endif

# Add defaults to any excludes that are manually
# specified.
installsrc-exclude :=     \
    $(installsrc-exclude) \
    .git .svn .DS_Store   \
    .python_env           \
    '*~' '.*~' '.*.sw?'

MKDIR        := /bin/mkdir -p -m 0755

# The individual B&I project makefiles should set their `installsrc` target
# to be `installsrc-helper`.
#
# We install the source using two tars piped together.
# We take particular care to:
#  - Exclude any source control files.
#  - Exclude any editor or OS cruft.
installsrc-helper:
	@echo "Installing source..."
	$(_v) $(MKDIR) "$(SRCROOT)"
	$(_v) tar cf -                         \
        $(foreach p,$(installsrc-exclude), \
        --exclude "$(p)")                  \
        Makefile                           \
        apple-xbs-support                  \
		$(installsrc-paths)                \
	| time tar xf - -C "$(SRCROOT)"
