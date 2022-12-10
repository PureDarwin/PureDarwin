# Monorepo makefile: redirect to project-specific .mk for B&I logic.

ifeq "$(RC_ProjectName)" ""
define NEWLINE


endef
projects := $(sort $(patsubst apple-xbs-support/%.mk,%, \
                     $(wildcard apple-xbs-support/*.mk)))
$(error "RC_ProjectName not set, try one of:"$(NEWLINE)$(NEWLINE) \
  $(foreach p,$(projects),$$ make RC_ProjectName=$p$(NEWLINE))    \
  $(NEWLINE))
endif

ifeq "$(SRCROOT)" ""
$(error "SRCROOT not set")
endif

# Note: APPLE_XBS_SUPPORT_MK is a lazy variable ('=' instead of ':=') that
# tracks the value of APPLE_XBS_SUPPORT_COMPUTED_RC_ProjectName.
APPLE_XBS_SUPPORT_COMPUTED_RC_ProjectName := $(RC_ProjectName)
APPLE_XBS_SUPPORT_MK = \
	apple-xbs-support/$(APPLE_XBS_SUPPORT_COMPUTED_RC_ProjectName).mk

# Note: APPLE_XBS_SUPPORT_VARIANT_PREFIX is a lazy variable ('=' instead of
# ':=') that tracks the value of APPLE_XBS_SUPPORT_VARIANT.
APPLE_XBS_SUPPORT_VARIANT :=
APPLE_XBS_SUPPORT_VARIANT_PREFIX = \
	$(if $(APPLE_XBS_SUPPORT_VARIANT),$(APPLE_XBS_SUPPORT_VARIANT)_,)

# Check if there is a .mk file for this project name.
ifeq "$(shell stat $(APPLE_XBS_SUPPORT_MK) 2>/dev/null)" ""

# Not found.  If there's an underscore, try dropping the prefix.
ifneq "$(words $(subst _, ,$(RC_ProjectName)))" "1"
APPLE_XBS_SUPPORT_VARIANT := $(word 1,$(subst _, ,$(RC_ProjectName)))
APPLE_XBS_SUPPORT_COMPUTED_RC_ProjectName := \
	$(subst ^$(APPLE_XBS_SUPPORT_VARIANT_PREFIX),,^$(RC_ProjectName))

ifeq "$(shell stat $(APPLE_XBS_SUPPORT_MK) 2>/dev/null)" ""
# Still not found... revert to original to avoid bad error messages.
APPLE_XBS_SUPPORT_COMPUTED_RC_ProjectName := $(RC_ProjectName)
APPLE_XBS_SUPPORT_VARIANT :=
endif
endif
endif

$(info $(RC_ProjectName) => $(APPLE_XBS_SUPPORT_MK))
include $(APPLE_XBS_SUPPORT_MK)
