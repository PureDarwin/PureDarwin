# wld bmake 

.include "config.mk"

PREFIX?=        /usr
LIBDIR?=        ${PREFIX}/lib
INCLUDEDIR?=    ${PREFIX}/include
PKGCONFIGDIR?=  ${LIBDIR}/pkgconfig

PKG_CONFIG?=    pkg-config
WAYLAND_SCANNER?=wayland-scanner
INSTALL?=       install
AR?=            ar
CC?=            cc

VERSION_MAJOR=0
VERSION_MINOR=0
VERSION=${VERSION_MAJOR}.${VERSION_MINOR}

WLD_LIB_LINK=libwld.so
WLD_LIB_SONAME=${WLD_LIB_LINK}.${VERSION_MAJOR}
WLD_LIB=${WLD_LIB_LINK}.${VERSION}

WLD_REQUIRES=fontconfig pixman-1
WLD_REQUIRES_PRIVATE=freetype2

WLD_SOURCES= \
    buffer.c \
    buffered_surface.c \
    color.c \
    context.c \
    font.c \
    renderer.c \
    surface.c

WLD_HEADERS=wld.h

.if ${ENABLE_DRM} == 1
WLD_REQUIRES_PRIVATE+=libdrm
WLD_SOURCES+=drm.c dumb.c
WLD_HEADERS+=drm.h

.if !empty(DRM_DRIVERS:Mintel)
WLD_REQUIRES_PRIVATE+=libdrm_intel
WLD_SOURCES+=intel.c intel/batch.c
WLD_CPPFLAGS+=-DWITH_DRM_INTEL=1
.endif

.if !empty(DRM_DRIVERS:Mnouveau)
WLD_REQUIRES_PRIVATE+=libdrm_nouveau
WLD_SOURCES+=nouveau.c
WLD_CPPFLAGS+=-DWITH_DRM_NOUVEAU=1
.endif
.endif

.if ${ENABLE_PIXMAN} == 1
WLD_SOURCES+=pixman.c
WLD_HEADERS+=pixman.h
.endif

.if ${ENABLE_WAYLAND} == 1
WLD_REQUIRES_PRIVATE+=wayland-client
WLD_SOURCES+=wayland.c
WLD_HEADERS+=wayland.h

.if !empty(WAYLAND_INTERFACES:Mshm)
WLD_SOURCES+=wayland-shm.c
WLD_CPPFLAGS+=-DWITH_WAYLAND_SHM=1
.endif

.if !empty(WAYLAND_INTERFACES:Mdrm)
WLD_SOURCES+=wayland-drm.c protocol/wayland-drm-protocol.c
WLD_CPPFLAGS+=-DWITH_WAYLAND_DRM=1
WAYLAND_DRM_XML=protocol/wayland-drm.xml
.endif
.endif

WLD_PACKAGES=${WLD_REQUIRES} ${WLD_REQUIRES_PRIVATE}
WLD_PKG_CFLAGS!=${PKG_CONFIG} --cflags ${WLD_PACKAGES}
WLD_PKG_LIBS!=${PKG_CONFIG} --libs ${WLD_PACKAGES}

.if ${.MAKE.OS} == "OpenBSD"
WLD_PKG_LIBS+=-lc
.endif

CPPFLAGS+=${WLD_PKG_CFLAGS} ${WLD_CPPFLAGS}

CFLAGS+=-fvisibility=hidden -std=c99 -Wvla
CFLAGS+=-Wall -Werror=implicit-function-declaration \
        -Werror=implicit-int -Werror=pointer-sign \
        -Werror=pointer-arith

.if ${.MAKE.OS} == "NetBSD"
CPPFLAGS+=-D_NETBSD_SOURCE
.endif

.if ${.MAKE.OS} == "Linux"
CPPFLAGS+=-D_POSIX_C_SOURCE=200809L
.endif

.if ${ENABLE_DEBUG} == 1
CPPFLAGS+=-DENABLE_DEBUG=1
CFLAGS+=-g
.else
CPPFLAGS+=-DNDEBUG
.endif

STATIC_OBJECTS=${WLD_SOURCES:R:S/$/.o/g}
SHARED_OBJECTS=${WLD_SOURCES:R:S/$/.lo/g}

DEPFLAGS=-MMD -MP -MF .deps/${.TARGET:T:R}.d

.SUFFIXES: .c .o .lo

.deps:
	@mkdir -p $@

.c.o: .deps
	@echo "  CC  $@"
	@${CC} ${CPPFLAGS} ${CFLAGS} ${DEPFLAGS} -c $< -o $@

.c.lo: .deps
	@echo "  CC  $@"
	@${CC} ${CPPFLAGS} ${CFLAGS} -fPIC ${DEPFLAGS} -c $< -o $@

.if defined(WAYLAND_DRM_XML)
protocol/wayland-drm-protocol.c: ${WAYLAND_DRM_XML}
	@echo "  GEN $@"
	@${WAYLAND_SCANNER:Uwayland-scanner} private-code ${.ALLSRC} $@

protocol/wayland-drm-client-protocol.h: ${WAYLAND_DRM_XML}
	@echo "  GEN $@"
	@${WAYLAND_SCANNER:Uwayland-scanner} client-header ${.ALLSRC} $@

wayland-drm.o wayland-drm.lo: protocol/wayland-drm-client-protocol.h
.endif

.if ${ENABLE_STATIC} == 1
libwld.a: ${STATIC_OBJECTS}
	@echo "  AR  $@"
	@${AR} rc $@ ${.ALLSRC}
	@ranlib $@
.endif

.if ${ENABLE_SHARED} == 1
${WLD_LIB}: ${SHARED_OBJECTS}
	@echo "  LINK $@"
	@${CC} -shared -Wl,-soname,${WLD_LIB_SONAME} \
	    -o $@ ${.ALLSRC} ${WLD_PKG_LIBS}

${WLD_LIB_SONAME}: ${WLD_LIB}
	ln -sf ${WLD_LIB} $@

${WLD_LIB_LINK}: ${WLD_LIB_SONAME}
	ln -sf ${WLD_LIB_SONAME} $@
.endif

wld.pc: wld.pc.in
	@echo "  GEN $@"
	@sed -e "s:@VERSION@:${VERSION}:" \
	     -e "s:@PREFIX@:${PREFIX}:" \
	     -e "s:@LIBDIR@:${LIBDIR}:" \
	     -e "s:@INCLUDEDIR@:${INCLUDEDIR}:" \
	     -e "s:@WLD_REQUIRES@:${WLD_REQUIRES}:" \
	     -e "s:@WLD_REQUIRES_PRIVATE@:${WLD_REQUIRES_PRIVATE}:" \
	     ${.ALLSRC} > $@

.MAIN: all
.PHONY: all
all: wld.pc
.if ${ENABLE_STATIC} == 1
all: libwld.a
.endif
.if ${ENABLE_SHARED} == 1
all: ${WLD_LIB} ${WLD_LIB_SONAME} ${WLD_LIB_LINK}
.endif

.PHONY: install
install: all
	${INSTALL} -d ${DESTDIR}${LIBDIR}
	${INSTALL} -d ${DESTDIR}${INCLUDEDIR}/wld
	${INSTALL} -d ${DESTDIR}${PKGCONFIGDIR}

	${INSTALL} -m644 wld.pc ${DESTDIR}${PKGCONFIGDIR}
	${INSTALL} -m644 ${WLD_HEADERS} ${DESTDIR}${INCLUDEDIR}/wld

.if ${ENABLE_STATIC} == 1
	${INSTALL} -m644 libwld.a ${DESTDIR}${LIBDIR}
.endif

.if ${ENABLE_SHARED} == 1
	${INSTALL} -m755 ${WLD_LIB} ${DESTDIR}${LIBDIR}
	cd ${DESTDIR}${LIBDIR} && ln -sf ${WLD_LIB} ${WLD_LIB_SONAME}
	cd ${DESTDIR}${LIBDIR} && ln -sf ${WLD_LIB_SONAME} ${WLD_LIB_LINK}
.endif

.PHONY: clean
clean:
	rm -f ${STATIC_OBJECTS} ${SHARED_OBJECTS} \
	    libwld.a ${WLD_LIB} ${WLD_LIB_LINK} \
	    ${WLD_LIB_SONAME} wld.pc
	rm -rf .deps protocol/wayland-drm-protocol.c \
	    protocol/wayland-drm-client-protocol.h

.sinclude ".deps/*.d"
