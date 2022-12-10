##
# Makefile for make
##

# Project info
Project               = make
ProjectName           = gnumake
UserType              = Developer
ToolType              = Commands
Extra_Configure_Flags = --program-prefix="gnu" --disable-nls
Extra_CC_Flags        = -mdynamic-no-pic -DUSE_POSIX_SPAWN
GnuAfterInstall       = install-html install-links install-plist strip-binary

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

install-html:
	$(MAKE) -C $(BuildDirectory) html
	$(INSTALL_DIRECTORY) $(RC_Install_HTML)
	$(INSTALL_FILE) $(BuildDirectory)/doc/make/*.html $(RC_Install_HTML)

install-links:
	@echo "Installing make symlink"
	$(LN) -fs gnumake $(DSTROOT)$(USRBINDIR)/make
	$(LN) -fs gnumake.1 $(DSTROOT)/usr/share/man/man1/make.1
	$(RM) $(DSTROOT)/usr/share/info/dir

strip-binary:
	$(CP) $(DSTROOT)/usr/bin/gnumake $(SYMROOT)
	$(STRIP) $(DSTROOT)/usr/bin/gnumake
	

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 3.81
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = patch-Makefile.in \
                 patch-default.c \
                 patch-expand.c \
                 patch-file.c \
                 patch-filedef.h \
                 patch-implicit.c \
                 patch-job.c \
                 patch-main.c \
                 patch-make.h \
                 patch-next.c \
                 patch-read.c \
                 patch-remake.c \
                 patch-variable.c \
                 patch-variable.h \
                 PR-3849799.patch \
                 PR-4339040.patch \
                 PR-4482353.patch \
		 PR-5071266.patch \
                 PR-6280514.patch \
                 PR-10516611.patch

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	@echo "== applying patches in $(SRCROOT)/$(Project) ==" && cd $(SRCROOT)/$(Project) && \
	for patchfile in $(AEP_Patches); do \
		echo "patch -p0 -F0 -i $(SRCROOT)/patches/$$patchfile"; \
		patch -p0 -F0 -i $(SRCROOT)/patches/$$patchfile || exit 1; \
	done

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(OSV)/$(ProjectName).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(ProjectName).txt
