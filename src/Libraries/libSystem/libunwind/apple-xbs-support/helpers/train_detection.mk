# Defines a list of known OS toolchains (i.e. toolchains submitted to an OS train).
# The Xcode default toolchain SHOULD NOT BE LISTED HERE.
KNOWN_OS_TOOLCHAINS := OSX iOS WatchOS AppleTVOS BridgeOS


# $(call toolchains-are-for-os,toolchains...)
#
# Check if all listed toolchains are OS toolchains
# - args[1]: space-seperated list of toolchains in the form <name> or
#   <name>-<major>-<minor>.
# Example: $(call toolchains-are-for-os,OSX) => 1
# Example: $(call toolchains-are-for-os,iOS) => 1
# Example: $(call toolchains-are-for-os,iOS-7.0) => 1
# Example: $(call toolchains-are-for-os,XcodeDefault) => <empty>
define toolchains-are-for-os
$(strip \
$(if $(strip $(1)),
$(if \
  $(filter 0,\
    $(foreach x,$(1),
      $(words \
        $(filter $(word 1,$(subst -, ,$(x))),$(KNOWN_OS_TOOLCHAINS))))),\
  ,1),)\
)
endef

define invert
$(if $(strip $(1)),,1)
endef

# $(call toolchain-version-is-at-least,version-to-check,minimum-versions...)
#
# Check matching toolchain version is big enough.  Expects one hyphen, between
# the toolchain name and version, then the major, a dot, and the minor.
#  - args[1]: version to check, in <name>-<major>.<minor> form.
#  - args[2]: space-separated list of versions that are big enough.
# Example: $(call toolchain-version-is-at-least,OSX-10.15,OSX-10.15 iOS-7.0) => 1
# Example: $(call toolchain-version-is-at-least,iOS-6.2,OSX-10.15 iOS-7.0)   => <empty>
define toolchain-version-is-at-least
$(strip \
$(if $(strip $(filter-out 2,$(foreach x,$(1) $(2),$(words $(subst -, ,$(x)))))),\
  $(error expected one hyphen in each version, got $(1) $(2)),\
$(if $(strip $(filter-out 2,$(foreach x,$(1) $(2),$(words $(subst ., ,$(x)))))),\
  $(error expected one dot in each version, got $(1) $(2)),\
$(if $(call invert,$(call toolchains-are-for-os,$(1) $(2))),\
  $(error Unknown toolchain in "$(1) $(2)". Known toolchains are "$(KNOWN_OS_TOOLCHAINS)"),\
$(if \
  $(filter $(1),\
    $(word 2,\
      $(shell printf "%s\n" $(1) \
                            $(filter $(word 1,$(subst -, ,$(1)))-%,$(2)) \
        | sort --version-sort))),\
  1,))))\
)
endef

# $(call toolchain-name-version,path/to/toolchain)
#
# Convert "path/to/Train1.2.3.4.xctoolchain" to "Train-1.2.3.4".
define toolchain-name-version
$(shell printf "%s" "$(basename $(notdir $(1)))" | sed -e 's,[0-9],-&,')
endef

# $(call os-train-version-is-at-least,minimum-versions...)
#
# Check that the OS train being submitted to is at least the minimum version.
# - args[1]: space-separated list of versions of the form <train>-<major>.<minor>.
#
# Examples:
# $(call os-train-version-is-at-least,OSX-10.15 iOS-7.0 WatchOS-6.0 AppleTVOS-7.0 BridgeOS-4.0)
define os-train-version-is-at-least
$(strip \
$(if $(DT_TOOLCHAIN_DIR),\
  $(call toolchain-version-is-at-least,\
    $(call toolchain-name-version,$(DT_TOOLCHAIN_DIR)),\
    $(1)),\
  $(error DT_TOOLCHAIN_DIR cannot be empty))\
)
endef
