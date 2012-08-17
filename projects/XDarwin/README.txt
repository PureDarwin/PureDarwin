To get it working:
-copy darwin.c, darwin.h, darwinEvents.c, darwinEvents.h, darwinfb.h and darwinXinput.c (and keysym2ucs.c, keysym2ucs.h, quartzKeyboard.c, quartzKeyboard.h, sanitizedCarbon.h for keyboard) from the xquartz dir in the latest xorg version.
-patch them like that:
 -darwin.c:
   -comment #include "quartz.h"
   -add the variables "int darwinDesiredRefresh = -1; unsigned int darwinDesiredWidth = 0, darwinDesiredHeight = 0;"
   -replace "XQuartz starting:\n" with "XDarwin starting:\n", in DarwinPrintBanner() (only for the name…)
   -replace function calls:
     -QuartzAddScreen with DarwinModeAddScreen
     -QuartzSetupScreen with DarwinModeSetupScreen
     -QuartzInitInput with DarwinModeInitInput
     -QuartzInitOutput with DarwinModeInitOutput
   -replace NoopDDA in DarwinMouseProc() with DarwinModeChangePointerControl
   -replace "xquartz virtual" with "xdarwin virtual", in InitInput() (only for the name…)
   -replace bundle_id_prefix with "XDarwin" in OsVendorInit()
 -darwinEvents.c:
   -comment #include "sanitizedCarbon.h", #include "quartz.h", #include "quartzRandR.h", #include <X11/extensions/applewmconst.h>, #include "applewmExt.h"
   -add #include <IOKit/hidsystem/ev_keymap.h>
   -add the variable "static InternalEvent* darwinEvents = NULL;"
   -comment declaration of QuartzModeEventHandler(), thread stuff (from "#define FD_ADD_MAX 128" to "DarwinPressModifierKey"), functions from DarwinEventHandler() to DarwinEQInit() and function DarwinSendDDXEvent()
   -remove mieqSetHandler(), pthread_cond_broadcast() and "fd_add_tid = …" in DarwinEQInit() plus remove all occurrences of "darwinEvents_lock();" and "darwinEvents_unlock();"
 -quartzKeyboard.c:
   -comment #include "quartz.h" and #include "X11Application.h"
   -add variable "Bool XQuartzOptionSendsAlt = FALSE;"
   -add in DarwinKeyboardInit():
	    if (!darwinParamConnect) (=>this is for "assert(darwinParamConnect = NXOpenEventStatus());" (because of NXOpenEventStatus() in xfIOKit.c))
	    if (!QuartsResyncKeymap(FALSE)) {
        ErrorF("X11ApplicationMain: Could not build a valid keymap.\n");
    	}
   -comment the two occurrences of "X11ApplicationLaunchClient(cmd);" and "if(sendDDXEvent) DarwinSendDDXEvent(kXquartzReloadKeymap, 0);" in QuartsResyncKeymap()
 -darwinfb.h:
   -add #include <IOKit/graphics/IOFramebufferShared.h> and #include "mipointer.h"
   -add this in the DarwinFramebufferRec struct:

CloseScreenProcPtr  CloseScreen;
    
    //From XFIOKitScreenRec
    io_connect_t        fbService;
    StdFBShmem_t       *cursorShmem;
    unsigned char      *IOKitframebuffer;
    unsigned char      *shadowPtr;
    
    //From XFIOKitCursorScreenRec
    Bool                    canHWCursor;
    short                   cursorMode;
    RecolorCursorProcPtr    RecolorCursor;
    InstallColormapProcPtr  InstallColormap;
    QueryBestSizeProcPtr    QueryBestSize;
    miPointerSpriteFuncPtr  spriteFuncs;
    ColormapPtr             pInstalledMap;
    
#ifndef IOFB_ARBITRARY_SIZE_CURSOR
    union {
        struct bm12Cursor bw;
        struct bm18Cursor bw8;
        struct bm34Cursor rgb;
        struct bm38Cursor rgb24;
    } cursor;
#else  /* IOFB_ARBITRARY_SIZE_CURSOR */
    unsigned char cursor[0];
#endif /* IOFB_ARBITRARY_SIZE_CURSOR */
 -darwin.h:
   -add "extern unsigned int     darwinDesiredWidth, darwinDesiredHeight; extern int              darwinDesiredRefresh;" plus:

//From xfIOKit.c
#include <assert.h>
#undef assert
#define assert(x) { if ((x) == 0) \
FatalError("assert failed on line %d of %s!\n", __LINE__, __FILE__); }
#define kern_assert(x) { if ((x) != KERN_SUCCESS) \
FatalError("assert failed on line %d of %s with kernel return 0x%x!\n", __LINE__, __FILE__, x); }
extern io_connect_t xfIOKitInputConnect;

Bool DarwinModeAddScreen(int index, ScreenPtr pScreen);
Bool DarwinModeSetupScreen(int index, ScreenPtr pScreen);
void DarwinModeInitOutput(int argc,char **argv);
void DarwinModeInitInput(int argc, char **argv);
void DarwinModeProcessEvent(xEvent *xe);
void DarwinModeGiveUp(void);
void DarwinModeBell(int volume, DeviceIntPtr pDevice, pointer ctrl, int class);
void DarwinModeChangePointerControl(DeviceIntPtr device, PtrCtrl *ctrl);

Bool XFIOKitInitCursor(ScreenPtr pScreen);

-then install Xquartz (to get all dependencies in /opt)
-then open Build.sh and fill X_DIR with the path of the source of xorg and SRC_DIR with the path of the source of XDarwin (the current dir)
-execute Build.sh. This will configure build the xorg static libs and produce the XDarwin executable (you can replace i386 with x86_64 to build a 64 bit executable)
-if the compilation went well, you can install gnome from macports and log in with ">console", cd XDarwin, ./Gnome.sh