# PLATFORM: IOS

PLATFORM = IOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

REWRITE = ${TESTROOT}/bin/rewrite-macho.pl

.PHONY: all clean

all:
	# make an arm64e binary
	${CC} -arch arm64e $(TESTROOT)/src/hello.c -o hello

	# Force the PAC version to 0.
	#
	# This is for arm64e_2018, used by 1st party userspace and kernel code.
	$(REWRITE) hello -o hello2 -v -c :$$((0x00000002))
	$(OTOOLC) -h hello2 | $(CHECK) -p CAPS0
# CAPS0: 0xfeedfacf\s* 16777228\s* 2\s* 0x00 .*
	$(OTOOLC) -hv hello2 | $(CHECK) -p CAPS0V
# CAPS0V: MH_MAGIC_64\s* ARM64\s* E\s* 0x00 .*

	# Force the PAC version to 15.
	#
	# This is for arm64e_2018 w/ 2nd party capability flags set to disable
	# all pointer authentication keys. This feature was never used, binaries
	# are not known to exist. Tool support was added for this feature
	# "just in case."
	$(REWRITE) hello -o hello2 -v -c :$$((0x0F000002))
	$(OTOOLC) -h hello2 | $(CHECK) -p CAPS15
# CAPS15: 0xfeedfacf\s* 16777228\s* 2\s* 0x0f .*
	$(OTOOLC) -hv hello2 | $(CHECK) -p CAPS15V
# CAPS15V: MH_MAGIC_64\s* ARM64\s* E\s* PAC15 .*

	# Set the arm64e versioned ABI flag for userspace.
	#
	# This is for arm64e_2020 and later. The ABI version is not expected
	# to change. But this is another one of those "just in case" things.
	$(REWRITE) hello -o hello2 -v -c :$$((0x80000002))
	$(OTOOLC) -h hello2 | $(CHECK) -p USR0
# USR0: 0xfeedfacf\s* 16777228\s* 2\s* 0x80 .*
	$(OTOOLC) -hv hello2 | $(CHECK) -p USR0V
# USR0V: MH_MAGIC_64\s* ARM64\s* E\s* USR00 .*
	$(REWRITE) hello -o hello2 -v -c :$$((0xBF000002))
	$(OTOOLC) -h hello2 | $(CHECK) -p USR63
# USR63: 0xfeedfacf\s* 16777228\s* 2\s* 0xbf .*
	$(OTOOLC) -hv hello2 | $(CHECK) -p USR63V
# USR63V: MH_MAGIC_64\s* ARM64\s* E\s* USR63 .*

	# Set the arm64e versioned ABI flag for kernelspace.
	#
	# This is for arm64e_2020 and later. The ABI version might change for
	# kexts, assuming the kernel can build a compatibility mechanism they
	# are happy with. Yet another "just in case" file format feature...
	$(REWRITE) hello -o hello2 -v -c :$$((0xC0000002))
	$(OTOOLC) -h hello2 | $(CHECK) -p KER0
# KER0: 0xfeedfacf\s* 16777228\s* 2\s* 0xc0 .*
	$(OTOOLC) -hv hello2 | $(CHECK) -p KER0V
# KER0V: MH_MAGIC_64\s* ARM64\s* E\s* KER00 .*
	$(REWRITE) hello -o hello2 -v -c :$$((0xFF000002))
	$(OTOOLC) -h hello2 | $(CHECK) -p KER63
# KER63: 0xfeedfacf\s* 16777228\s* 2\s* 0xff .*
	$(OTOOLC) -hv hello2 | $(CHECK) -p KER63V
# KER63V: MH_MAGIC_64\s* ARM64\s* E\s* KER63 .*

	echo PASS

clean:
	rm -f hello hello2

