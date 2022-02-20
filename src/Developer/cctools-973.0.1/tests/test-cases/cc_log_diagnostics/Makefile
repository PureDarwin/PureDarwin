# PLATFORM: MACOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

ifneq ("$(wildcard ${CCTOOLS_ROOT})","")
  DIAGTEST = $(CCTOOLS_ROOT)/usr/local/bin/diagtest
else
  DIAGTEST = `xcrun --sdk $(SDKROOT) -f diagtest`
endif

.PHONY: all clean

all:
	# Note, piping the test invocation to cat in order to silence the error
	# coming from the diagtest tool. Ordinarily one would use make's '-'
	# command prefix to ignore the non-zero exit code, but make insists on
	# printing a message about it to stderr, which will cause the test
	# harness to fail the test.
	CC_LOG_DIAGNOSTICS=1 CC_LOG_DIAGNOSTICS_FILE=log.txt		     \
	  $(DIAGTEST) 2>&1 | cat
	CC_LOG_DIAGNOSTICS=1 CC_LOG_DIAGNOSTICS_FILE=log.txt		     \
	  $(DIAGTEST) -fatal 2>&1 | cat
	CC_LOG_DIAGNOSTICS=1 CC_LOG_DIAGNOSTICS_FILE=log.txt		     \
	  $(DIAGTEST) -system_fatal 2>&1 | cat
	CC_LOG_DIAGNOSTICS=1 CC_LOG_DIAGNOSTICS_FILE=log.txt		     \
	  $(DIAGTEST) -mach_fatal 2>&1 | cat
	CC_LOG_DIAGNOSTICS=1 CC_LOG_DIAGNOSTICS_FILE=log.txt		     \
	  $(DIAGTEST) -archive 2>&1 | cat
	$(CHECK) -i log.txt
# CHECK: <dict>
# CHECK:   <key>tool</key>
# CHECK:   <string>.*/diagtest</string>
# CHECK:   <key>diagnostics</key>
# CHECK:   <array>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>warning</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is an example warning.</string>
# CHECK:     </dict>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>error</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is an error that wil cause a non-zero exit.</string>
# CHECK:     </dict>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>error</string>
# CHECK:       <key>message</key>
# CHECK:       <string>for architecture: ppc this is error causes a non-zero exit.</string>
# CHECK:     </dict>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>error</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is a file-not-found error. (No such file or directory)</string>
# CHECK:     </dict>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>error</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is a mach error. ((os/kern) code signing error)</string>
# CHECK:     </dict>
# CHECK:   </array>
# CHECK: </dict>
# CHECK: <dict>
# CHECK:   <key>tool</key>
# CHECK:   <string>.*/diagtest</string>
# CHECK:   <key>diagnostics</key>
# CHECK:   <array>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>fatal error</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is a fatal error.</string>
# CHECK:     </dict>
# CHECK:   </array>
# CHECK: </dict>
# CHECK: <dict>
# CHECK:   <key>tool</key>
# CHECK:   <string>.*/diagtest</string>
# CHECK:   <key>diagnostics</key>
# CHECK:   <array>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>fatal error</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is a fatal system error. (No such file or directory)</string>
# CHECK:     </dict>
# CHECK:   </array>
# CHECK: </dict>
# CHECK: <dict>
# CHECK:   <key>tool</key>
# CHECK:   <string>.*/diagtest</string>
# CHECK:   <key>diagnostics</key>
# CHECK:   <array>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>fatal error</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is a fatal mach error. ((os/kern) code signing error)</string>
# CHECK:     </dict>
# CHECK:   </array>
# CHECK: </dict>
# CHECK: <dict>
# CHECK:   <key>tool</key>
# CHECK:   <string>.*/diagtest</string>
# CHECK:   <key>diagnostics</key>
# CHECK:   <array>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>warning</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is an arch warning: test.a (for architecture arm64e)</string>
# CHECK:     </dict>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>warning</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is an arch member warning: test.a(test.o) (for architecture arm64e)</string>
# CHECK:     </dict>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>error</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is an arch error: test.a (for architecture arm64e)</string>
# CHECK:     </dict>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>error</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is an arch member error: test.a(test.o) (for architecture arm64e)</string>
# CHECK:     </dict>
# CHECK:     <dict>
# CHECK:       <key>level</key>
# CHECK:       <string>fatal error</string>
# CHECK:       <key>message</key>
# CHECK:       <string>this is a fatal arch error: test.a(test.o) (for architecture arm64e)</string>
# CHECK:     </dict>
# CHECK:   </array>
# CHECK: </dict>

	echo PASS

clean:
	rm -f log.txt

