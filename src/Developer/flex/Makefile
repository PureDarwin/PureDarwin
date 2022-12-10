##
# Makefile for Flex
##

# Project info
Project           = flex
UserType          = Developer
ToolType          = Commands
GnuAfterInstall   = after_install
Extra_CC_Flags    = -mdynamic-no-pic
Extra_Environment = STRIP_LIB_FLAGS="-S" lib_LIBRARIES="libfl.a"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 2.5.35
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)

#
# Update $(Project).plist when changing AEP_Patches
#
AEP_Patches    = scanEOF.diff filter-stdin.diff Makefile.in.diff \
	libmain.c.diff main.c.diff Wall.diff W64-32.diff \
	Wsign-compare.diff

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

ifneq ($(INSTALL_LOCATION),)
Install_Prefix=$(INSTALL_LOCATION)/usr
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		printf "Applying $$patchfile\n"; \
		patch -d $(SRCROOT)/$(Project) -p0 -F0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
	# Avoid calling help2man
	printf "1d\nw\nq\n" | ed -s $(SRCROOT)/$(Project)/doc/flex.1
	$(RM) -f $(SRCROOT)/$(Project)/skel.c #regenerated in $(OBJROOT)
endif

after_install::
ifneq ($(INSTALL_LOCATION),)
	# not wanted
	$(RM) -rf $(DSTROOT)/usr $(DSTROOT)/$(INSTALL_LOCATION)/$(USRLIBDIR) 
	$(INSTALL) lex.sh $(DSTROOT)/$(INSTALL_LOCATION)$(USRBINDIR)/lex
	$(LN) -fs flex $(DSTROOT)/$(INSTALL_LOCATION)$(USRBINDIR)/flex++
	$(RM) -rf 
else
	$(LN) -fs flex $(DSTROOT)$(USRBINDIR)/flex++
	$(LN) -f $(DSTROOT)/usr/share/man/man1/flex.1 $(DSTROOT)/usr/share/man/man1/flex++.1
	$(LN) -f $(DSTROOT)/usr/share/man/man1/flex.1 $(DSTROOT)/usr/share/man/man1/lex.1
	$(INSTALL) lex.sh $(DSTROOT)$(USRBINDIR)/lex
	$(LN) -fs libfl.a $(DSTROOT)$(USRLIBDIR)/libl.a
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) "$(SRCROOT)/$(Project).plist" $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
	$(RM) -f "$(DSTROOT)/usr/share/info/dir"
endif
