/**************************************************************
 *
 * IOKit support for the Darwin X Server
 *
 * HISTORY:
 * Original port to Mac OS X Server by John Carmack
 * Port to Darwin 1.0 by Dave Zarzycki
 * Significantly rewritten for XFree86 4.0.1 by Torrey Lyons
 * Updated by Guillaume Verdeau (2012) to work with actual XOrg:
   - changes in XFIOKitHIDThread()
     FIXME: need to find a way to get the delta in float (see case NX_SCROLLWHEELMOVED)
   - added DarwinModeCloseScreen()
   - added DarwinModeChangePointerControl()
   - plus some little changes...
 *
 **************************************************************/
/*
 * Copyright (c) 2001-2004 Torrey T. Lyons. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <X11/X.h>
#include <X11/Xproto.h>
#include "os.h"
#include "servermd.h"
#include "inputstr.h"
#include "scrnintstr.h"
#include "mi.h"
#include "mibstore.h"
#include "mipointer.h"
#include "micmap.h"
#include "shadow.h"

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <mach/mach_interface.h>

#define NO_CFPLUGIN
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/ev_keymap.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#import <IOKit/hidsystem/event_status_driver.h>

// Define this to work around bugs in the display drivers for
// older PowerBook G3's. If the X server starts without this
// #define, you don't need it.
#undef OLD_POWERBOOK_G3

#include "darwin.h"
#include "darwinEvents.h"

// Globals
io_connect_t    xfIOKitInputConnect = 0;

static pthread_t                inputThread;
static EvGlobals *              evg;
static mach_port_t              masterPort;
static mach_port_t              notificationPort;
static IONotificationPortRef    NotificationPortRef;
static mach_port_t              pmNotificationPort;
static io_iterator_t            fbIter;

/*
 * XFIOKitStoreColors
 * This is a callback from X to change the hardware colormap
 * when using PsuedoColor.
 */
static void XFIOKitStoreColors(ColormapPtr pmap, int numEntries, xColorItem *pdefs)
{
    kern_return_t   kr;
    int             i;
    IOColorEntry    *newColors;
    ScreenPtr       pScreen = pmap->pScreen;
    
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);
    
    assert( newColors = (IOColorEntry *)
                malloc( numEntries*sizeof(IOColorEntry) ));

    // Convert xColorItem values to IOColorEntry
    // assume the colormap is PsuedoColor
    // as we do not support DirectColor
    for (i = 0; i < numEntries; i++) {
        newColors[i].index = pdefs[i].pixel;
        newColors[i].red =   pdefs[i].red;
        newColors[i].green = pdefs[i].green;
        newColors[i].blue =  pdefs[i].blue;
    }
    
    kr = IOFBSetCLUT( dfb->fbService, 0, numEntries,
                     kSetCLUTByValue, newColors );
    kern_assert( kr );

    free( newColors );
}


void
DDXRingBell(int volume,              // volume is % of max
            int pitch,               // pitch is Hz
            int duration)            // duration is milliseconds
{
}
///*
// * DarwinModeBell
// *  FIXME
// */
//void DarwinModeBell(
//    int             loud,
//    DeviceIntPtr    pDevice,
//    pointer         ctrl,
//    int             fbclass)
//{
//}


/*
 * DarwinModeGiveUp
 *  Closes the connections to IOKit services
 */
void DarwinModeGiveUp( void )
{
    int i;
 
    // we must close the HID System first
    // because it is a client of the framebuffer
    NXCloseEventStatus( darwinParamConnect );
    IOServiceClose( xfIOKitInputConnect );
    for (i = 0; i < screenInfo.numScreens; i++) {
        DarwinFramebufferPtr dfb = SCREEN_PRIV(screenInfo.screens[i]);
        IOServiceClose( dfb->fbService );
    }
}


/*
 * ClearEvent
 *  Clear an event from the HID System event queue
 */
static void ClearEvent(NXEvent * ep)
{
    static NXEvent nullEvent = {NX_NULLEVENT, {0, 0 }, 0, -1, 0 };

    *ep = nullEvent;
    ep->data.compound.subType = ep->data.compound.misc.L[0] = ep->data.compound.misc.L[1] = 0;
}


/*
 * XFIOKitHIDThread
 *  Read the HID System event queue, translate it to an X event,
 *  and queue it for processing.
 */
static void CorrectSrcolling(double *XScroll, double *YScroll)
{
    double deltaX = *XScroll;
    double deltaY = *YScroll;
    
//    static double lastScrollTime = 0.0;
    static int lastScrollTime = 0;
    
    /* These store how much extra we have already scrolled.
     * ie, this is how much we ignore on the next event.
     */
    static double deficit_x = 0.0;
    static double deficit_y = 0.0;
    
    /* If we have past a second since the last scroll, wipe the slate
     * clean
     */
//    if (GetTimeInMillis() - lastScrollTime > 1.0) {
    if (GetTimeInMillis() - lastScrollTime > 1000) {
        deficit_x = deficit_y = 0.0;
    }
    lastScrollTime = GetTimeInMillis();
    
    if (deltaX != 0.0) {
        /* If we changed directions, wipe the slate clean */
        if ((deficit_x < 0.0 && deltaX > 0.0) ||
            (deficit_x > 0.0 && deltaX < 0.0)) {
            deficit_x = 0.0;
        }
        
        /* Eat up the deficit, but ensure that something is
         * always sent 
         */
        if (fabs(deltaX) > fabs(deficit_x)) {
            deltaX -= deficit_x;
            
            if (deltaX > 0.0) {
                deficit_x = ceil(deltaX) - deltaX;
                deltaX = ceil(deltaX);
            } else {
                deficit_x = floor(deltaX) - deltaX;
                deltaX = floor(deltaX);
            }
        } else {
            deficit_x -= deltaX;
            
            if (deltaX > 0.0) {
                deltaX = 1.0;
            } else {
                deltaX = -1.0;
            }
            
            deficit_x += deltaX;
        }
    }
    
    if (deltaY != 0.0) {
        /* If we changed directions, wipe the slate clean */
        if ((deficit_y < 0.0 && deltaY > 0.0) ||
            (deficit_y > 0.0 && deltaY < 0.0)) {
            deficit_y = 0.0;
        }
        
        /* Eat up the deficit, but ensure that something is
         * always sent 
         */
        if (fabs(deltaY) > fabs(deficit_y)) {
            deltaY -= deficit_y;
            
            if (deltaY > 0.0) {
                deficit_y = ceil(deltaY) - deltaY;
                deltaY = ceil(deltaY);
            } else {
                deficit_y = floor(deltaY) - deltaY;
                deltaY = floor(deltaY);
            }
        } else {
            deficit_y -= deltaY;
            
            if (deltaY > 0.0) {
                deltaY = 1.0;
            } else {
                deltaY = -1.0;
            }
            
            deficit_y += deltaY;
        }
    }
    *XScroll = deltaX;
    *YScroll = deltaY;
}
static void *XFIOKitHIDThread(void *unused)
{
    for (;;) {
        NXEQElement             *oldHead;
        mach_msg_return_t       kr;
        mach_msg_empty_rcv_t    msg;
        
        kr = mach_msg((mach_msg_header_t*) &msg, MACH_RCV_MSG, 0, sizeof(msg), notificationPort, 0, MACH_PORT_NULL);
        kern_assert(kr);
        
        while (evg->LLEHead != evg->LLETail) {
            NXEvent ev;
            int ev_button, ev_type;
            
            // Extract the next event from the kernel queue
            oldHead = (NXEQElement*)&evg->lleq[evg->LLEHead];
//ev_lock(&oldHead->sema);
            ev = oldHead->event;
            ClearEvent(&oldHead->event);
            evg->LLEHead = oldHead->next;
//ev_unlock(&oldHead->sema);
            
            switch( ev.type ) {
                case NX_MOUSEMOVED:
                    ev_button = 0;
                    ev_type = MotionNotify;
                    goto handle_mouse;
                    
                case NX_LMOUSEDOWN:
                    ev_button = 1;
                    ev_type = ButtonPress;
                    goto handle_mouse;
                    
                case NX_LMOUSEUP:
                    ev_button = 1;
                    ev_type = ButtonRelease;
                    goto handle_mouse;
                    
                handle_mouse:
                    DarwinSendPointerEvents(darwinPointer, ev_type, ev_button,
                                            (double)ev.location.x,
                                            (double)ev.location.y,
                                            0.0, 0.0);
                    break;

                case NX_KEYDOWN:
                case NX_KEYUP:
                    ev_type = (ev.type == NX_KEYDOWN) ? KeyPress : KeyRelease;
                    DarwinSendKeyboardEvents(ev_type, ev.data.key.keyCode);
                    break;
                    
                case NX_FLAGSCHANGED:
                    DarwinUpdateModKeys(ev.flags);
                    break;
                    
                case NX_SYSDEFINED:
                    if (ev.data.compound.subType == 7)
                    {
                        long hwDelta = ev.data.compound.misc.L[0];
                        long hwButtons = ev.data.compound.misc.L[1];
                        int i;
                        
                        for (i = 1; i < 5; i++) {
                            if (hwDelta & (1 << i)) {
                                // IOKit and X have different numbering for the
                                // middle and right mouse buttons.
                                if (i == 1)
                                    ev_button = 3;
                                else if (i == 2)
                                    ev_button = 2;
                                else
                                    ev_button = i + 1;
                                
                                if (hwButtons & (1 << i))
                                    ev_type = ButtonPress;
                                else
                                    ev_type = ButtonRelease;
                                
                                DarwinSendPointerEvents(darwinPointer,
                                                        ev_type, ev_button,
                                                        (double)ev.location.x,
                                                        (double)ev.location.y,
                                                        0.0, 0.0);
                            }
                        }
                    }
                    else  continue;
                    break;
                    
                case NX_SCROLLWHEELMOVED:
                {
                    double deltaX = (double) ev.data.scrollWheel.deltaAxis2;
                    double deltaY = (double) ev.data.scrollWheel.deltaAxis1;
int reversedLionScrolling=1;
                    if (reversedLionScrolling) {
                        deltaX *= -1;
                        deltaY *= -1;
                    }
ErrorF("deltaX=%f, deltaY=%f\n", deltaX, deltaY);
//TODO: isContinuous (see -sendX11NSEvent, X11Application.m)
//How to do the same thing with IOKit?
//int isContinuous = CGEventGetIntegerValueField(cge, kCGScrollWheelEventIsContinuous);
int isContinuous = 0;
                    if (!isContinuous)
                        CorrectSrcolling(&deltaX, &deltaY);
ErrorF("deltaX=%f, deltaY=%f\n", deltaX, deltaY);
                    DarwinSendScrollEvents(deltaX, deltaY);
                }
                    break;
                    
                default:
                    ErrorF("Unknown IOHID event caught: %ld\n", (long) ev.type);
                    continue;
            }
        }
    }
    
    return NULL;
}


/*
 * XFIOKitPMThread
 *  Handle power state notifications
 */
static void *XFIOKitPMThread(void *arg)
{
    ScreenPtr pScreen = (ScreenPtr)arg;
    
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);

    for (;;) {
        mach_msg_return_t       kr;
        mach_msg_empty_rcv_t    msg;

        kr = mach_msg((mach_msg_header_t*) &msg, MACH_RCV_MSG, 0,
                      sizeof(msg), pmNotificationPort, 0, MACH_PORT_NULL);
        kern_assert(kr);

        // display is powering down
        if (msg.header.msgh_id == 0) {
            IOFBAcknowledgePM( dfb->fbService );
            //xf86SetRootClip(pScreen, FALSE);
            SetRootClip(pScreen, FALSE);
        }
        // display just woke up
        else if (msg.header.msgh_id == 1) {
            //xf86SetRootClip(pScreen, TRUE);
            SetRootClip(pScreen, TRUE);
        }
    }
    return NULL;
}


/*
 * SetupFBandHID
 *  Setup an IOFramebuffer service and connect the HID system to it.
 */
static Bool SetupFBandHID(
                          int                    index,
                          DarwinFramebufferPtr   dfb)
{
    kern_return_t           kr;
    io_service_t            service;
    io_connect_t            fbService;
#if !__LP64__ || defined(IOCONNECT_MAPMEMORY_10_6)
    vm_address_t       vram;
    vm_size_t          shmemSize;
#else
    mach_vm_address_t       vram;
    mach_vm_size_t          shmemSize;
#endif
    int                     i;
    UInt32                  numModes;
    IODisplayModeInformation modeInfo;
    IODisplayModeID         displayMode, *allModes;
    IOIndex                 displayDepth;
    IOFramebufferInformation fbInfo;
    IOPixelInformation      pixelInfo;
    StdFBShmem_t            *cshmem;

    // find and open the IOFrameBuffer service
    service = IOIteratorNext(fbIter);
    IOObjectRelease( fbIter );
    if (service == 0)
        return FALSE;
    
    kr = IOServiceOpen( service, mach_task_self(), kIOFBServerConnectType, &dfb->fbService );
    IOObjectRelease( service );
    if (kr != KERN_SUCCESS) {
        ErrorF("Failed to connect as window server to screen %i.\n", index);
        return FALSE;
    }
    fbService = dfb->fbService;

    // create the slice of shared memory containing cursor state data
    kr = IOFBCreateSharedCursor( fbService,
                                 kIOFBCurrentShmemVersion,
                                 32, 32 );
    if (kr != KERN_SUCCESS)
        return FALSE;

    // Register for power management events for the framebuffer's device
    kr = IOCreateReceivePort(kOSNotificationMessageID, &pmNotificationPort);
    kern_assert(kr);
    kr = IOConnectSetNotificationPort( fbService, 0,
                                       pmNotificationPort, 0 );
    if (kr != KERN_SUCCESS) {
        ErrorF("Power management registration failed.\n");
    }

    // SET THE SCREEN PARAMETERS
    // get the current screen resolution, refresh rate and depth
    kr = IOFBGetCurrentDisplayModeAndDepth( fbService,
                                            &displayMode,
                                            &displayDepth );
    if (kr != KERN_SUCCESS)
        return FALSE;

    //transform darwinDesiredDepth into an IOIndex
    if (darwinDesiredDepth == 8)
        darwinDesiredDepth = 0;
    else if (darwinDesiredDepth == 15)
        darwinDesiredDepth = 1;
    else if (darwinDesiredDepth == 24)
        darwinDesiredDepth = 2;
 
    // use the current screen resolution if the user
    // only wants to change the refresh rate
    if (darwinDesiredRefresh != -1 && darwinDesiredWidth == 0) {
        kr = IOFBGetDisplayModeInformation( fbService,
                                            displayMode,
                                            &modeInfo );
        if (kr != KERN_SUCCESS)
            return FALSE;
        darwinDesiredWidth = modeInfo.nominalWidth;
        darwinDesiredHeight = modeInfo.nominalHeight;
    }

    // use the current resolution and refresh rate
    // if the user doesn't have a preference
    if (darwinDesiredWidth == 0) {

        // change the pixel depth if desired
        if (darwinDesiredDepth != -1) {
            kr = IOFBGetDisplayModeInformation( fbService,
                                                displayMode,
                                                &modeInfo );
            if (kr != KERN_SUCCESS)
                return FALSE;

            if (modeInfo.maxDepthIndex < darwinDesiredDepth) {
                ErrorF("Discarding screen %i:\n", index);
                ErrorF("Current screen resolution does not support desired pixel depth.\n");
                return FALSE;
            }

            displayDepth = darwinDesiredDepth;
            kr = IOFBSetDisplayModeAndDepth( fbService, displayMode,
                                             displayDepth );
            if (kr != KERN_SUCCESS)
                return FALSE;
        }

    // look for display mode with correct resolution and refresh rate
    } else {

        // get an array of all supported display modes
        kr = IOFBGetDisplayModeCount( fbService, &numModes );
        if (kr != KERN_SUCCESS)
            return FALSE;
        assert(allModes = (IODisplayModeID *)
                malloc( numModes * sizeof(IODisplayModeID) ));
        kr = IOFBGetDisplayModes( fbService, numModes, allModes );
        if (kr != KERN_SUCCESS)
            return FALSE;

        for (i = 0; i < numModes; i++) {
            kr = IOFBGetDisplayModeInformation( fbService, allModes[i],
                                                &modeInfo );
            if (kr != KERN_SUCCESS)
                return FALSE;

            if (modeInfo.flags & kDisplayModeValidFlag &&
                modeInfo.nominalWidth == darwinDesiredWidth &&
                modeInfo.nominalHeight == darwinDesiredHeight) {

                if (darwinDesiredDepth == -1)
                    darwinDesiredDepth = modeInfo.maxDepthIndex;
                if (modeInfo.maxDepthIndex < darwinDesiredDepth) {
                    ErrorF("Discarding screen %i:\n", index);
                    ErrorF("Desired screen resolution does not support desired pixel depth.\n");
                    return FALSE;
                }

                if ((darwinDesiredRefresh == -1 ||
                    (darwinDesiredRefresh << 16) == modeInfo.refreshRate)) {
                    displayMode = allModes[i];
                    displayDepth = darwinDesiredDepth;
                    kr = IOFBSetDisplayModeAndDepth(fbService,
                                                    displayMode,
                                                    displayDepth);
                    if (kr != KERN_SUCCESS)
                        return FALSE;
                    break;
                }
            }
        }

        free( allModes );
        if (i >= numModes) {
            ErrorF("Discarding screen %i:\n", index);
            ErrorF("Desired screen resolution or refresh rate is not supported.\n");
            return FALSE;
        }
    }

    kr = IOFBGetPixelInformation( fbService, displayMode, displayDepth,
                                  kIOFBSystemAperture, &pixelInfo );
    if (kr != KERN_SUCCESS)
        return FALSE;

#ifdef __i386__
    /* x86 in 8bit mode currently needs fixed color map... */
    if (pixelInfo.bitsPerComponent == 8 &&
        pixelInfo.componentCount == 1)
    {
        pixelInfo.pixelType = kIOFixedCLUTPixels;
    }
#endif

#ifdef OLD_POWERBOOK_G3
    if (pixelInfo.pixelType == kIOCLUTPixels)
        pixelInfo.pixelType = kIOFixedCLUTPixels;
#endif

    kr = IOFBGetFramebufferInformationForAperture( fbService,
                                                   kIOFBSystemAperture,
                                                   &fbInfo );
    if (kr != KERN_SUCCESS)
        return FALSE;

    // FIXME: 1x1 IOFramebuffers are sometimes used to indicate video
    // outputs without a monitor connected to them. Since IOKit Xinerama
    // does not really work, this often causes problems on PowerBooks.
    // For now we explicitly check and ignore these screens.
    if (fbInfo.activeWidth <= 1 || fbInfo.activeHeight <= 1) {
        ErrorF("Discarding screen %i:\n", index);
        ErrorF("Invalid width or height.\n");
        return FALSE;
    }

    kr = IOConnectMapMemory( fbService, kIOFBCursorMemory,
#if !__LP64__ || defined(IOCONNECT_MAPMEMORY_10_6)
                            mach_task_self(), (vm_address_t *) &cshmem,
#else
                            mach_task_self(), (mach_vm_address_t *) &cshmem,
#endif
                             &shmemSize, kIOMapAnywhere );
    if (kr != KERN_SUCCESS)
        return FALSE;
    dfb->cursorShmem = cshmem;

    kr = IOConnectMapMemory( fbService, kIOFBSystemAperture,
                             mach_task_self(), &vram, &shmemSize,
                             kIOMapAnywhere );
    if (kr != KERN_SUCCESS)
        return FALSE;

    dfb->IOKitframebuffer = (void*)vram;
    dfb->x = cshmem->screenBounds.minx;
    dfb->y = cshmem->screenBounds.miny;
    dfb->width = fbInfo.activeWidth;
    dfb->height = fbInfo.activeHeight;
    dfb->pitch = fbInfo.bytesPerRow;
    dfb->bitsPerPixel = fbInfo.bitsPerPixel;
    dfb->depth = pixelInfo.componentCount * pixelInfo.bitsPerComponent;
    dfb->bitsPerRGB = pixelInfo.bitsPerComponent;
    
    // allocate shadow framebuffer
    dfb->shadowPtr = malloc(dfb->pitch * dfb->height);
//dfb->framebuffer = dfb->shadowPtr;
dfb->framebuffer = (void*)vram;

    // Note: Darwin kIORGBDirectPixels = X TrueColor, not DirectColor
    if (pixelInfo.pixelType == kIORGBDirectPixels) {
        dfb->preferredCVC = TrueColor;
        dfb->visuals = TrueColorMask;
    } else if (pixelInfo.pixelType == kIOCLUTPixels) {
        dfb->preferredCVC = PseudoColorMask;
        dfb->visuals = TrueColorMask;
    } else if (pixelInfo.pixelType == kIOFixedCLUTPixels) {
        dfb->preferredCVC = StaticColor;
        dfb->visuals = StaticColorMask;
    }
    // TODO: Make PseudoColor visuals not suck in TrueColor mode  
    //    if(dfb->depth == 8) {
    //        dfb->redMask = 0;
    //        dfb->greenMask = 0;
    //        dfb->blueMask = 0;
    //    }
    if(dfb->depth == 15) {
        dfb->redMask = RM_ARGB(0, 5, 5, 5);
        dfb->greenMask = GM_ARGB(0, 5, 5, 5);
        dfb->blueMask = BM_ARGB(0, 5, 5, 5);
    }
    if(dfb->depth == 24) {
        dfb->redMask = RM_ARGB(0, 8, 8, 8);
        dfb->greenMask = GM_ARGB(0, 8, 8, 8);
        dfb->blueMask = BM_ARGB(0, 8, 8, 8);        
    }

    // Inform the HID system that the framebuffer is also connected to it.
    kr = IOConnectAddClient( xfIOKitInputConnect, fbService );
    kern_assert( kr );

    // We have to have added at least one screen
    // before we can enable the cursor.
    kr = IOHIDSetCursorEnable(xfIOKitInputConnect, TRUE);
    kern_assert( kr );
    
    return TRUE;
}


/*
 * DarwinModeCloseScreen
 *  Closes the connections to IOKit services
 */
Bool DarwinModeCloseScreen(int index, ScreenPtr pScreen)
{
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);

    IOServiceClose( dfb->fbService );
    
    return (*dfb->CloseScreen) (index, pScreen);
}


/*
 * DarwinModeAddScreen
 *  IOKit specific initialization for each screen.
 */
Bool DarwinModeAddScreen(int index, ScreenPtr pScreen)
{
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);

    // setup hardware framebuffer
    dfb->fbService = 0;
    if (! SetupFBandHID(index, dfb))
    {
        if (dfb->fbService)  IOServiceClose(dfb->fbService);
        return FALSE;
    }
    
    return TRUE;
}


///*
// * XFIOKitShadowUpdate
// *  Update the damaged regions of the shadow framebuffer on the screen.
// */
//static void XFIOKitShadowUpdate(ScreenPtr pScreen,
//                                shadowBufPtr pBuf)
//{
//ErrorF("In XFIOKitShadowUpdate()\n");
//    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);
//    
//ErrorF("In XFIOKitShadowUpdate()2\n");
//
//    RegionPtr damage = &pBuf->damage;
//    int numBox = REGION_NUM_RECTS(damage);
//    BoxPtr pBox = REGION_RECTS(damage);
//    int pitch = dfb->pitch;
//    int bpp = dfb->bitsPerPixel/8;
//
//ErrorF("In XFIOKitShadowUpdate()3\n");
//
//    // Loop through all the damaged boxes
//    while (numBox--) {
//        int width, height, offset;
//        unsigned char *src, *dst;
//
//        width = (pBox->x2 - pBox->x1) * bpp;
//        height = pBox->y2 - pBox->y1;
//        offset = (pBox->y1 * pitch) + (pBox->x1 * bpp);
//        src = dfb->shadowPtr + offset;
//        dst = dfb->IOKitframebuffer + offset;
//
//        while (height--) {
//            memcpy(dst, src, width);
//            dst += pitch;
//            src += pitch;
//        }
//
//        // Get the next box
//        pBox++;
//    }
//ErrorF("Finished XFIOKitShadowUpdate()\n");
//}


/*
 * DarwinModeSetupScreen
 *  Finalize IOKit specific initialization of each screen.
 */
Bool DarwinModeSetupScreen(
    int index,
    ScreenPtr pScreen)
{
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);
    pthread_t pmThread;

    // initalize cursor support
    if (! XFIOKitInitCursor(pScreen)) {
        return FALSE;
    }

//if (!shadowSetup(pScreen))
//    return FALSE;

    // initialize shadow framebuffer support
//    if (! shadowInit(pScreen, XFIOKitShadowUpdate, NULL)) {
//        ErrorF("Failed to initalize shadow framebuffer for screen %i.\n",
//               index);
//        return FALSE;
//    }

    // initialize colormap handling as needed
    if (dfb->preferredCVC == PseudoColor) {
        pScreen->StoreColors = XFIOKitStoreColors;
    }

    // initialize power manager handling
    pthread_create( &pmThread, NULL, XFIOKitPMThread,
                    (void *) pScreen );
    
    dfb->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = DarwinModeCloseScreen;
    
    return TRUE;
}


/*
 * DarwinModeInitOutput
 *  One-time initialization of IOKit output support.
 */
void DarwinModeInitOutput(
    int argc,
    char **argv)
{
    if (serverGeneration > 1) {
        // close IOKit connections before reopening them
        // we must close the HID System first
        // because it is a client of the framebuffer
        NXCloseEventStatus( darwinParamConnect );
        IOServiceClose( xfIOKitInputConnect );
    }
    
    kern_return_t           kr;
    io_iterator_t           iter;
    io_service_t            service;
#if !__LP64__ || defined(IOCONNECT_MAPMEMORY_10_6)
    vm_address_t            shmem;
    vm_size_t               shmemSize;
#else
    mach_vm_address_t       shmem;
    mach_vm_size_t          shmemSize;
#endif
    
    ErrorF("Display mode: IOKit\n");

    kr = IOMasterPort(bootstrap_port, &masterPort);
    kern_assert( kr );

    // Find and open the HID System Service
    // Do this now to be sure the Mac OS X window server is not running.
    kr = IOServiceGetMatchingServices( masterPort,
                                       IOServiceMatching( kIOHIDSystemClass ),
                                       &iter );
    kern_assert( kr );

    assert( service = IOIteratorNext( iter ) );
    
    kr = IOServiceOpen( service, mach_task_self(), kIOHIDServerConnectType,
                        &xfIOKitInputConnect );
    if (kr != KERN_SUCCESS) {
        ErrorF("Failed to connect to the HID System as the window server!\n");
        FatalError("Make sure you have quit the Mac OS X window server.\n");
    }

    IOObjectRelease( service );
    IOObjectRelease( iter );

    // Setup the event queue in memory shared by the kernel and X server
    kr = IOHIDCreateSharedMemory( xfIOKitInputConnect,
#if defined kIOHIDLastCompatibleShmemVersion //we are on Lion
                                 kIOHIDLastCompatibleShmemVersion );
#else
                                 kIOHIDCurrentShmemVersion ); //It became a #define in 10.7
#endif
    kern_assert( kr );

    kr = IOConnectMapMemory( xfIOKitInputConnect, kIOHIDGlobalMemory,
                             mach_task_self(), &shmem, &shmemSize,
                             kIOMapAnywhere );
    kern_assert( kr );

    evg = (EvGlobals *)(shmem + ((EvOffsets *)shmem)->evGlobalsOffset);

    assert(sizeof(EvGlobals) == evg->structSize);

    NotificationPortRef = IONotificationPortCreate( masterPort );

    notificationPort = IONotificationPortGetMachPort(NotificationPortRef);

    kr = IOConnectSetNotificationPort( xfIOKitInputConnect,
                                       kIOHIDEventNotification,
                                       notificationPort, 0 );
    kern_assert( kr );

    evg->movedMask |= NX_MOUSEMOVEDMASK;

    // find number of framebuffers
    kr = IOServiceGetMatchingServices( masterPort,
                        IOServiceMatching( IOFRAMEBUFFER_CONFORMSTO ),
                        &fbIter );
    kern_assert( kr );

    darwinScreensFound = 0;
    while ((service = IOIteratorNext(fbIter))) {
        IOObjectRelease( service );
        darwinScreensFound++;
    }

    IOIteratorReset(fbIter);
}


/*
 * DarwinModeInitInput
 *  One-time initialization of IOKit input support.
 */
void DarwinModeInitInput(
    int argc,
    char **argv)
{
    kern_return_t           kr;
    int                     fd[2];

    kr = IOHIDSetEventsEnable(xfIOKitInputConnect, TRUE);
    kern_assert( kr );

    // Start event passing thread
    assert( pipe(fd) == 0 );
    darwinEventReadFD = fd[0];
    darwinEventWriteFD = fd[1];
    fcntl(darwinEventReadFD, F_SETFL, O_NONBLOCK);
    pthread_create(&inputThread, NULL,
                   XFIOKitHIDThread, NULL);
}


/*
 * DarwinModeSetRootClip
 *  Enable or disable rendering to the X screen.
 */
void DarwinModeSetRootClip(
    BOOL enable)
{
    int i;
    
    for (i = 0; i < screenInfo.numScreens; i++) {
        if (screenInfo.screens[i]) {
            SetRootClip(screenInfo.screens[i], enable);
        }
    }
}

/*
 * DarwinChangePointerControl
 *  Set mouse acceleration and thresholding
 *  FIXME: We currently ignore the threshold in ctrl->threshold.
 */
void DarwinModeChangePointerControl(DeviceIntPtr device, PtrCtrl *ctrl)
{
    if (!darwinParamConnect)
        assert(darwinParamConnect = NXOpenEventStatus());
    
    kern_return_t   kr;
    double          acceleration;
    
    acceleration = ctrl->num / ctrl->den;
    kr = IOHIDSetMouseAcceleration(darwinParamConnect, acceleration);
    if (kr != KERN_SUCCESS)
        ErrorF("Could not set mouse acceleration with kernel return = 0x%x.\n", kr);
}
