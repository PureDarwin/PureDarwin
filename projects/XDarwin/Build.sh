#!/bin/sh

X_DIR=/Users/Guillaume/Downloads/xorg-server-1.12.2.901i386
#X_DIR=/Users/Guillaume/Downloads/xorg-server-1.12.2.901x86_64
SRC_DIR="/Users/Guillaume/Desktop/XDarwin"
X_INSTALLED_DIR=/opt/X11

cd $X_DIR
export ACLOCAL="aclocal -I$X_INSTALLED_DIR/share/aclocal"
export PKG_CONFIG_PATH='$X_INSTALLED_DIR/share/pkgconfig:$X_INSTALLED_DIR/lib/pkgconfig'
export PKG_CONFIG='/opt/local/bin/pkg-config'
#./autogen.sh --disable-xquartz CC="clang -arch i386" --prefix=/opt/X11 #prefix is to force xkb using XQuartz's xkb dir
./autogen.sh --disable-xquartz CC="clang -arch x86_64" --prefix=/opt/X11
make
cd $SRC_DIR

XDARWIN_LIBS="""$X_DIR/composite/.libs/libcomposite.a \
                $X_DIR/config/.libs/libconfig.a \
                $X_DIR/damageext/.libs/libdamageext.a \
                $X_DIR/dbe/.libs/libdbe.a \
                $X_DIR/fb/.libs/libfb.a \
                $X_DIR/glx/.libs/libglx.a \
                $X_DIR/miext/damage/.libs/libdamage.a \
                $X_DIR/miext/shadow/.libs/libshadow.a \
                $X_DIR/miext/sync/.libs/libsync.a \
                $X_DIR/randr/.libs/librandr.a \
                $X_DIR/record/.libs/librecord.a  \
                $X_DIR/render/.libs/librender.a \
                $X_DIR/Xext/.libs/libXext.a \
                $X_DIR/xfixes/.libs/libxfixes.a \
                $X_DIR/Xi/.libs/libXi.a \
                $X_DIR/xkb/.libs/libxkb.a \
                $X_DIR/xkb/.libs/libxkbstubs.a"""


XDARWIN_MAIN_LIBS="""$X_DIR/dix/.libs/libdix.a \
                     $X_DIR/dix/.libs/libmain.a \
                     $X_DIR/mi/.libs/libmi.a \
                     $X_DIR/os/.libs/libos.a"""

XSERVER_LIBS="-L$X_INSTALLED_DIR/lib -lXfont -lXau -lfontenc -lpixman-1 -lXdmcp -lm"

LIBS="$XDARWIN_LIBS $XDARWIN_MAIN_LIBS $XSERVER_LIBS"

INCLUDES="""-I$X_INSTALLED_DIR/include \
            -I$X_INSTALLED_DIR/include/pixman-1 \
            -I$X_DIR/include \
            -I$X_DIR/composite \
            -I$X_DIR/damageext \
            -I$X_DIR/fb \
            -I$X_DIR/mi \
            -I$X_DIR/miext/damage \
            -I$X_DIR/miext/shadow \
            -I$X_DIR/randr \
            -I$X_DIR/render \
            -I$X_DIR/Xext \
            -I$X_DIR/xfixes \
            -I$X_DIR/Xi"""

CFLAGS="$INCLUDES -DHAVE_DIX_CONFIG_H -O2 -pipe -Wall"

#GLX_DEFINES='-DGLX_USE_TLS -DPTHREADS'
#GL_CFLAGS='-I$X_INSTALLED_DIR/include  '
#GL_LIBS='-L$X_INSTALLED_DIR/lib -lGL  '

XDARWIN_SRC=""" $SRC_DIR/darwinXinput.c \
                $SRC_DIR/quartzKeyboard.c \
                $SRC_DIR/keysym2ucs.c \
                $SRC_DIR/darwinEvents.c \
                $SRC_DIR/darwin.c \
                $SRC_DIR/xfIOKit.c \
                $SRC_DIR/xfIOKitCursor.c """

XDARWIN_MAIN_SRC="""$X_DIR/Xext/dpmsstubs.c \
                    $X_DIR/mi/miinitext.c \
                    $X_DIR/fb/fbcmap_mi.c"""

SRC="$XDARWIN_SRC $XDARWIN_MAIN_SRC"

clang $SRC $CFLAGS $LIBS -arch i386 -framework IOKit -o XDarwin         -framework CoreFoundation -framework Carbon
#clang $SRC $CFLAGS $LIBS -arch x86_64 -framework IOKit -o XDarwin         -framework CoreFoundation -framework Carbon
