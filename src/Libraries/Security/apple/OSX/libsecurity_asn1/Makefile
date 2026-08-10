
CONFIG ?= DEBUG_EMU
CFLAGS_DEBUG_ARM = -O0 -gdwarf-2 -arch armv6
CFLAGS_RELEASE_ARM = -arch armv6 -Os -gdwarf-2 -Wuninitialized  -DNDEBUG
CFLAGS_DEBUG_EMU = -O0 -gdwarf-2 -DDEBUG
CFLAGS_RELEASE_EMU = -gdwarf-2 -Os -Wuninitialized -DNDEBUG

WFLAGS = -Wno-trigraphs -Werror -Wunused-function \
	 -Wunused-label -Wunused-variable -Wunknown-pragmas \
	 -Wmost -Wno-four-char-constants -Wno-unknown-pragmas \
	 -Wmissing-prototypes 

CFLAGS = --std=c99 -pipe \
	 -fpascal-strings -fasm-blocks -fmessage-length=0 \
	 $(WFLAGS) $(CFLAGS_$(CONFIG))

LDFLAGS := -Wl,-dylib -Wl,-dead_strip -Wl,-exported_symbols_list lib/security_asn1.exp



HEADER_PATHS := . .. ../libDER
CFLAGS += $(addprefix -I$(include),$(HEADER_PATHS))

SOURCES = \
        lib/oidsattr.c \
	lib/oidsalg.c \
	lib/SecAsn1Coder.c \
	lib/SecAsn1Templates.c \
 \
	lib/X509Templates.c \
	lib/certExtensionTemplates.c \
	lib/csrTemplates.c \
	lib/keyTemplates.c \
	lib/nameTemplates.c \
	lib/ocspTemplates.c \
	lib/osKeyTemplates.c \
        lib/pkcs12Templates.c \
        lib/pkcs7Templates.c \
 \
	lib/nsprPortX.c \
	lib/nssUtils.c \
	lib/plarena.c \
	lib/secErrorStr.c \
 \
	lib/secasn1d.c \
	lib/secasn1e.c \
	lib/secasn1u.c \
	lib/secport.c

PUBLIC_HEADERS = \
	lib/SecAsn1Coder.h \
	lib/SecAsn1Templates.h \
	lib/SecAsn1Types.h \
        lib/oidsattr.h \
	lib/oidsalg.h \
        lib/oidsbase.h

PRIVATE_HEADERS = \
	lib/secasn1t.h \
	lib/X509Templates.h \
	lib/asn1Templates.h \
	lib/certExtensionTemplates.h \
	lib/csrTemplates.h \
	lib/keyTemplates.h \
	lib/nameTemplates.h \
        lib/nssUtils.h \
	lib/nssilckt.h \
	lib/nssilock.h \
	lib/nsslocks.h \
	lib/ocspTemplates.h \
	lib/osKeyTemplates.h \
	lib/pkcs7Templates.h \
	lib/pkcs12Templates.h \
	lib/plarena.h \
	lib/prinit.h

INTERNAL_HEADERS = lib/plarenas.h lib/plstr.h lib/prbit.h lib/prcpucfg.h lib/prcvar.h lib/prenv.h \
	lib/prerr.h lib/prerror.h lib/prinrval.h lib/prlock.h lib/prlog.h lib/prlong.h lib/prmem.h lib/prmon.h \
	lib/protypes.h lib/prthread.h lib/prtime.h lib/prtypes.h lib/prvrsion.h lib/secasn1.h lib/seccomon.h \
	lib/secerr.h lib/secport.h

OBJECTS := $(SOURCES:.c=.o)

all:	install-headers build

install-headers: $(PUBLIC_HEADERS) $(PRIVATE_HEADERS) $(INTERNAL_HEADERS)
	@install -d Security
	@install -d security_asn1
	@install -m 0444 $(PUBLIC_HEADERS) $(PRIVATE_HEADERS) Security
	@install -m 0444 $(INTERNAL_HEADERS) security_asn1

build: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -dynamiclib -dynamic -compatibility_version 0 -current_version 0 -install_name libasn1.dylib -o libasn1.dylib $(OBJECTS)

clean:
	@rm -f $(OBJECTS) $(PROGNAME)

tags: $(SOURCES) $(PROG)
	etags $+


