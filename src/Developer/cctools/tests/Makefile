INSTALL_FILES = Makefile bin include run-tests test-cases
EXECUTABLES_FILES = run-tests bin/exit-non-zero-pass.pl			      \
		    bin/fail-if-exit-non-zero.pl bin/fail-if-exit-zero.pl     \
		    bin/fail-if-no-stdin.pl bin/fail-if-stdin.pl	      \
		    bin/fail-iff-exit-zero.pl bin/pass-iff-exit-non-zero.pl   \
		    bin/pass-iff-exit-zero.pl bin/pass-iff-no-stdin.pl        \
		    bin/pass-iff-stdin.pl bin/verify-align.pl 

MKDIRS  = /bin/mkdir -p
SRCROOT = .

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod -R u+rwX,go+rX $(SRCROOT)/*
	cd $(SRCROOT); chmod a+x $(EXECUTABLES_FILES)
	#cd $(SRCROOT); chmod a-w `find . -type f -print`
	find $(SRCROOT) -type f -exec chmod a-w '{}' \;
