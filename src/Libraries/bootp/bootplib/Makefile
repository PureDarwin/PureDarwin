# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
CORETELEPHONY=-framework CoreTelephony
ARCHS=armv7
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
ARCHS=x86_64
CORETELEPHONY=
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
ARCHS=i386 x86_64
CORETELEPHONY=
CC = cc
SYSROOT = /
PF_INC = -F/System/Library/PrivateFrameworks
else
# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
endif

ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))

SYSTEM_PRIVATE = -I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

SC_PRIV = -DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS 
BLIB = ../bootplib

none:
	@echo "make what?"

gen: genoptionfiles
	rm -f gen_dhcp*.h
	cc -Wall -g -o genoptionfiles genoptionfiles.c
	./genoptionfiles -table > gen_dhcp_parse_table.h
	./genoptionfiles -dhcptag > gen_dhcp_tags.h
	./genoptionfiles -dhcptype > gen_dhcp_types.h
	chmod 444 gen_dhcp*.h

IPv4ClasslessRoute: IPv4ClasslessRoute.c util.c cfutil.c IPConfigurationLog.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -framework CoreFoundation -framework SystemConfiguration -DTEST_IPV4ROUTE $(SYSTEM_PRIVATE) $(SC_PRIV) -g -o $@ $^

inetroute: inetroute.c
	$(CC) -DTEST_INETROUTE  -isysroot $(SYSROOT) $(ARCH_FLAGS) -framework CoreFoundation dynarray.c ptrlist.c util.c cfutil.c -g -o $@ $^

sharepoints:
	cc -Wno-four-char-constants -o sharepoints $(OTHER_CFLAGS) -DTEST_SHAREPOINTS sharepoints.c -F$(NEXT_ROOT)/System/Library/PrivateFrameworks -framework ServerControl -framework ServerPrefix -framework AFPDefines

nilist: NICache.c NICache.h
	cc -Wall -g -o nilist -DNICACHE_TEST NICache.c dynarray.c ptrlist.c netinfo.c host_identifier.c util.c NIDomain.c

afpusers:
	cc -Wall -g -o afpusers -DTEST_AFPUSERS AFPUsers.c NICache.c dynarray.c ptrlist.c netinfo.c host_identifier.c util.c NIDomain.c

readtest:
	cc -Wall -g -o readtest -DREAD_TEST NICache.c dynarray.c ptrlist.c netinfo.c host_identifier.c util.c

arp: arp.c arp.h
	cc -Wall -g -o arp -DMAIN arp.c -I/System/Library/Frameworks/System.framework/PrivateHeaders

nihosts:
	cc -Wall -g -o nihosts -DTEST_NIHOSTS NIHosts.m util.c NIDomain.c dynarray.c ptrlist.c netinfo.c host_identifier.c

interfaces: interfaces.c IPConfigurationLog.c ptrlist.c dynarray.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -framework CoreFoundation -framework SystemConfiguration -DTEST_INTERFACES $(SYSTEM_PRIVATE) $(SC_PRIV) -g -o $@ $^

interfaces-no-sc: interfaces.c IPConfigurationLog.c ptrlist.c dynarray.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -framework CoreFoundation -DNO_SYSTEMCONFIGURATION=1 -DTEST_INTERFACES $(SYSTEM_PRIVATE) $(SC_PRIV) -g -o $@ $^

dhcpopt: dhcp_options.c cfutil.c util.c DNSNameList.c IPv4ClasslessRoute.c ptrlist.c IPConfigurationLog.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -Wall -g -DTEST_DHCP_OPTIONS -o $@ $^ -framework SystemConfiguration -framework CoreFoundation

dhcpopt-no-sc: dhcp_options.c cfutil.c util.c DNSNameList.c IPv4ClasslessRoute.c ptrlist.c IPConfigurationLog.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -DNO_SYSTEMCONFIGURATION=1 -Wall -g -DTEST_DHCP_OPTIONS -o $@ $^ -framework CoreFoundation

macncopt: macnc_options.c
	cc -Wall -g -DTEST_MACNC_OPTIONS -o macncopt macnc_options.c dhcp_options.c ptrlist.c util.c DNSNameList.c

nbsp: nbsp.c nbsp.h
	cc -Wall -g -DTEST_NBSP -o nbsp nbsp.c ptrlist.c dynarray.c

hfsvols: hfsvols.c hfsvols.h
	cc -Wall -g -DTEST_HFSVOLS -o hfsvols hfsvols.c ptrlist.c dynarray.c

nbimages: nbimages.c nbimages.h cfutil.c
	$(CC) -Wall -g -DTEST_NBIMAGES -o nbimages nbimages.c util.c nbsp.c ptrlist.c dynarray.c cfutil.c -framework CoreFoundation -I$(NET_ROOT)$(SYSTEM_LIBRARY_DIR)/Frameworks/System.framework/PrivateHeaders -framework SystemConfiguration

dnsnamelist: DNSNameList.c util.c cfutil.c
	$(CC) -Wall -g -isysroot $(SYSROOT) $(ARCH_FLAGS) -DTEST_DNSNAMELIST -framework CoreFoundation -framework SystemConfiguration -o $@ $^

subnets: IPv4ClasslessRoute.c subnets.c cfutil.c DNSNameList.c util.c ptrlist.c dynarray.c dhcp_options.c IPConfigurationLog.c
	$(CC) -DNO_SYSTEMCONFIGURATION -DTEST_SUBNETS -isysroot $(SYSROOT) $(ARCH_FLAGS) -framework CoreFoundation -framework SystemConfiguration -DTEST_INTERFACES $(SYSTEM_PRIVATE) $(SC_PRIV) -g -o $@ $^

test-dhcpv6-options: DHCPv6Options.c DHCPv6.c DHCPDUID.c ptrlist.c util.c DNSNameList.c cfutil.c
	$(CC) -DTEST_DHCPV6_OPTIONS -isysroot $(SYSROOT) $(ARCH_FLAGS) -framework CoreFoundation -framework SystemConfiguration -g -o $@ $^

clean:
	rm -f afpusers arp dhcpopt dnsnamelist genoptionfiles hfsvols interfaces nbimages nbsp nihosts nilist readtest sharepoints subnets test-dhcpv6-options IPv4ClasslessRoute inetroute interfaces-no-sc
	rm -rf *.dSYM/
