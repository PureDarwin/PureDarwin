/**************************************************************
 *
 * Cursor support for Darwin X Server
 *
 * Three different cursor modes are possible:
 *  X (0)         - tracking via Darwin kernel,
 *                  display via X machine independent
 *  Kernel (1)    - tracking and display via Darwin kernel
 *                  (not currently supported)
 *  Hardware (2)  - tracking and display via hardware
 *
 * The X software cursor uses the Darwin software cursor
 * routines in IOFramebuffer.cpp to track the cursor, but
 * displays the cursor image using the X machine
 * independent display cursor routines in midispcur.c.
 *
 * The kernel cursor uses IOFramebuffer.cpp routines to
 * track and display the cursor. This gives better
 * performance as the display calls don't have to cross
 * the kernel boundary. Unfortunately, this mode has
 * synchronization issues with the user land X server
 * and isn't currently used.
 *
 * Hardware cursor support lets the hardware handle these
 * details.
 *
 * Kernel and hardware cursor mode only work for cursors
 * up to a certain size, currently 16x16 pixels. If a
 * bigger cursor is set, we fallback to X cursor mode.
 *
 * HISTORY:
 * 1.0 by Torrey T. Lyons, October 30, 2000
 * Updated by Guillaume Verdeau (2012) to work with actual XOrg:
   - FIXME: cursor privates are now in screen privates (DarwinFramebufferRec)
     but there is probably a nicer way.
   - changes in the SpriteFuncs: the work of XFIOKitRealizeCursor is now done in
     XFIOKitSetCursor for hardware cursors and missing SpriteFuncs added
   - FIXME: update or test XFIOKitRealizeCursor8 and XFIOKitRealizeCursor15
   - plus some little changes...
 *
 **************************************************************/
/*
 * Copyright (c) 2001-2002 Torrey T. Lyons. All Rights Reserved.
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

#include "scrnintstr.h"
#include "cursorstr.h"
#include "mipointrst.h"
#include "micmap.h"

#define NO_CFPLUGIN
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>

#include "darwin.h"

#define DUMP_DARWIN_CURSOR FALSE

#include "darwinEvents.h"

// The cursors format are documented in IOFramebufferShared.h.
#define RGBto34WithGamma(red, green, blue)  \
    (  0x000F                               \
     | (((red) & 0xF) << 12)                \
     | (((green) & 0xF) << 8)               \
     | (((blue) & 0xF) << 4) )
#define RGBto38WithGamma(red, green, blue)  \
    (  0xFF << 24                           \
     | (((red) & 0xFF) << 16)               \
     | (((green) & 0xFF) << 8)              \
     | (((blue) & 0xFF)) )
#define HighBitOf32 0x80000000

/*
===========================================================================

 Pointer sprite functions

===========================================================================
*/

/*
    Realizing the Darwin hardware cursor (ie. converting from the
    X representation to the IOKit representation) is complicated
    by the fact that we have three different potential cursor
    formats to go to, one for each bit depth (8, 15, or 24).
    The IOKit formats are documented in IOFramebufferShared.h.
    X cursors are represented as two pieces, a source and a mask.
    The mask is a bitmap indicating which parts of the cursor are 
    transparent and which parts are drawn.  The source is a bitmap
    indicating which parts of the non-transparent portion of the the
    cursor should be painted in the foreground color and which should
    be painted in the background color. The bitmaps are given in
    32-bit format with least significant byte and bit first.
    (This is opposite PowerPC Darwin.)
*/


/*
 * XFIOKitRealizeCursor8
 * Convert the X cursor representation to an 8-bit depth
 * format for Darwin. This function assumes the maximum cursor
 * width is a multiple of 8.
 */
static Bool
XFIOKitRealizeCursor8(
    ScreenPtr pScreen,
    CursorPtr pCursor)
{
    DarwinFramebufferPtr        dfb = SCREEN_PRIV(pScreen);
    
    unsigned char   *newSourceP, *newMaskP;
    CARD32          *oldSourceP, *oldMaskP;
    xColorItem      fgColor, bgColor;
    int             x, y, rowPad;
    int             cursorWidth, cursorHeight;
    ColormapPtr     pmap;

    // check cursor size just to be sure
    cursorWidth = pCursor->bits->width;
    cursorHeight = pCursor->bits->height;
    if (cursorHeight > CURSORHEIGHT || cursorWidth > CURSORWIDTH)
        return FALSE;

    // get cursor colors in colormap
    //pmap = miInstalledMaps[pScreen->myNum];
    pmap = GetInstalledmiColormap(pScreen);
    
    if (!pmap) return FALSE;

    fgColor.red = pCursor->foreRed;
    fgColor.green = pCursor->foreGreen;
    fgColor.blue = pCursor->foreBlue;
    FakeAllocColor(pmap, &fgColor);
    bgColor.red = pCursor->backRed;
    bgColor.green = pCursor->backGreen;
    bgColor.blue = pCursor->backBlue;
    FakeAllocColor(pmap, &bgColor);
    FakeFreeColor(pmap, fgColor.pixel);
    FakeFreeColor(pmap, bgColor.pixel);

    newSourceP = dfb->cursor.bw8.image[0];
    newMaskP = dfb->cursor.bw8.mask[0];
    
    memset( newSourceP, pScreen->blackPixel, CURSORWIDTH*CURSORHEIGHT );
    memset( newMaskP, 0, CURSORWIDTH*CURSORHEIGHT );

    // convert to 8-bit Darwin cursor format
    oldSourceP = (CARD32 *) pCursor->bits->source;
    oldMaskP = (CARD32 *) pCursor->bits->mask;
    rowPad = CURSORWIDTH - cursorWidth;

    for (y = 0; y < cursorHeight; y++) {
        for (x = 0; x < cursorWidth; x++) {
            if (*oldSourceP & (HighBitOf32 >> x))
                *newSourceP = fgColor.pixel;
            else
                *newSourceP = bgColor.pixel;
            if (*oldMaskP & (HighBitOf32 >> x))
                *newMaskP = 255;
            else
                *newSourceP = pScreen->blackPixel;
            newSourceP++; newMaskP++;
        }
        oldSourceP++; oldMaskP++;
        newSourceP += rowPad; newMaskP += rowPad;
    }

    return TRUE;
}


/*
 * XFIOKitRealizeCursor15
 * Convert the X cursor representation to an 15-bit depth
 * format for Darwin.
 */
static Bool
XFIOKitRealizeCursor15(
    ScreenPtr       pScreen,
    CursorPtr       pCursor)
{
    DarwinFramebufferPtr        dfb = SCREEN_PRIV(pScreen);
    
    unsigned short  fgPixel, bgPixel;
    unsigned short  *newSourceP;
    CARD32          *oldSourceP, *oldMaskP;
    int             x, y, rowPad;
    int             cursorWidth, cursorHeight;

    // check cursor size just to be sure
    cursorWidth = pCursor->bits->width;
    cursorHeight = pCursor->bits->height;
    if (cursorHeight > CURSORHEIGHT || cursorWidth > CURSORWIDTH)
       return FALSE;

    newSourceP = dfb->cursor.rgb.image[0];
    
    memset( newSourceP, 0, CURSORWIDTH*CURSORHEIGHT*sizeof(short) );

    // calculate pixel values
    fgPixel = RGBto34WithGamma( pCursor->foreRed, pCursor->foreGreen,
                                pCursor->foreBlue );
    bgPixel = RGBto34WithGamma( pCursor->backRed, pCursor->backGreen,
                                pCursor->backBlue );

    // convert to 15-bit Darwin cursor format
    oldSourceP = (CARD32 *) pCursor->bits->source;
    oldMaskP = (CARD32 *) pCursor->bits->mask;
    rowPad = CURSORWIDTH - cursorWidth;

    for (y = 0; y < cursorHeight; y++) {
        for (x = 0; x < cursorWidth; x++) {
            if (*oldMaskP & (HighBitOf32 >> x)) {
                if (*oldSourceP & (HighBitOf32 >> x))
                    *newSourceP = fgPixel;
                else
                    *newSourceP = bgPixel;
            } else {
                *newSourceP = 0;
            }
            newSourceP++;
        }
        oldSourceP++; oldMaskP++;
        newSourceP += rowPad;
    }

#if DUMP_DARWIN_CURSOR
    // Write out the cursor
    ErrorF("Cursor: 0x%x\n", pCursor);
    ErrorF("Width = %i, Height = %i, RowPad = %i\n", cursorWidth,
            cursorHeight, rowPad);
    for (y = 0; y < cursorHeight; y++) {
        newSourceP = dfb->cursor.rgb.image[0] + y*CURSORWIDTH;
        for (x = 0; x < cursorWidth; x++) {
            if (*newSourceP == fgPixel)
                ErrorF("x");
            else if (*newSourceP == bgPixel)
                ErrorF("o");
            else
                ErrorF(" ");
            newSourceP++;
        }
        ErrorF("\n");
    }
#endif

    return TRUE;
}


/*
 * XFIOKitRealizeCursor24
 * Convert the X cursor representation to an 24-bit depth
 * format for Darwin. This function assumes the maximum cursor
 * width is a multiple of 8.
 */
#ifndef BITMAP_SCANLINE_PAD
#define BITMAP_SCANLINE_PAD  32
#define LOG2_BITMAP_PAD		5
#define LOG2_BYTES_PER_SCANLINE_PAD	2
#endif
#define BitmapBytePad(w) \
(((int)((w) + BITMAP_SCANLINE_PAD - 1) >> LOG2_BITMAP_PAD) << LOG2_BYTES_PER_SCANLINE_PAD)
static int
GetBit(unsigned char *line, int x)
{
    unsigned char mask;
    
    if (screenInfo.bitmapBitOrder == LSBFirst)
        mask = (1 << (x & 7));
    else
        mask = (0x80 >> (x & 7));
    /* XXX assumes byte order is host byte order */
    line += (x >> 3);
    if (*line & mask)
        return 1;
    return 0;
}
static void
XFIOKitRealizeCursor24(
                       ScreenPtr       pScreen,
                       CursorPtr       pCursor)
{
    DarwinFramebufferPtr        dfb = SCREEN_PRIV(pScreen);

    unsigned int *image = dfb->cursor.rgb24.image[0];
    
    int width = pCursor->bits->width;
    int height = pCursor->bits->height;
    
#ifdef ARGB_CURSOR
    if (pCursor->bits->argb)
        memcpy(image, pCursor->bits->argb, width * height * sizeof(unsigned int));
    else
#endif
    {
        unsigned char *srcLine = pCursor->bits->source;
        unsigned char *mskLine = pCursor->bits->mask;
        int stride = BitmapBytePad(width);
        int x, y;
        
        unsigned int fg = RGBto38WithGamma( pCursor->foreRed, pCursor->foreGreen, pCursor->foreBlue );
        unsigned int bg = RGBto38WithGamma( pCursor->backRed, pCursor->backGreen, pCursor->backBlue );
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if (GetBit(mskLine, x)) {
                    if (GetBit(srcLine, x))
                        *image++ = fg;
                    else
                        *image++ = bg;
                }
                else
                    *image++ = 0;
            }
            srcLine += stride;
            mskLine += stride;
        }
    }
}

/*
 * XFIOKitRealizeCursor
 * 
 */
static Bool
XFIOKitRealizeCursor(
    DeviceIntPtr    pDev,
    ScreenPtr       pScreen,
    CursorPtr       pCursor)
{
    Bool                        result;
    DarwinFramebufferPtr        dfb = SCREEN_PRIV(pScreen);

    if ((pCursor->bits->height > CURSORHEIGHT) ||
        (pCursor->bits->width > CURSORWIDTH) ||
        // FIXME: this condition is not needed after kernel cursor works
        !dfb->canHWCursor) {
        result = (*dfb->spriteFuncs->RealizeCursor)(pDev, pScreen, pCursor);
    } else {
        
        //Is there something to do with hardware/kernel cursor?
        
        result = TRUE;
    }

    return result;
}


/*
 * XFIOKitUnrealizeCursor
 * 
 */
static Bool
XFIOKitUnrealizeCursor(
    DeviceIntPtr pDev,
    ScreenPtr pScreen,
    CursorPtr pCursor)
{
    Bool                        result;
    DarwinFramebufferPtr        dfb = SCREEN_PRIV(pScreen);

    if ((pCursor->bits->height > CURSORHEIGHT) ||
        (pCursor->bits->width > CURSORWIDTH) ||
        // FIXME: this condition is not needed after kernel cursor works
        !dfb->canHWCursor) {
        result = (*dfb->spriteFuncs->UnrealizeCursor)(pDev, pScreen, pCursor);
    } else {
        result = TRUE;
    }

    return result;
}


/*
 * XFIOKitSetCursor
 * Set the cursor sprite and position
 * Use hardware cursor if possible
 */
static void
XFIOKitSetCursor(
    DeviceIntPtr    pDev,
    ScreenPtr       pScreen,
    CursorPtr       pCursor,
    int             x,
    int             y)
{
    kern_return_t               kr;
    DarwinFramebufferPtr        dfb = SCREEN_PRIV(pScreen);
    
    StdFBShmem_t                *cshmem = dfb->cursorShmem;
    
    // are we supposed to remove the cursor?
    if (!pCursor) {
        if (dfb->cursorMode == 0)
            (*dfb->spriteFuncs->SetCursor)(pDev, pScreen, NullCursor, x, y);
        else {
            if (!cshmem->cursorShow) {
                cshmem->cursorShow++;
                if (cshmem->hardwareCursorActive) {
                    kr = IOFBSetCursorVisible(dfb->fbService, FALSE);
                    kern_assert( kr );
                }
            }
        }
        return;
    } 

    // can we use the kernel or hardware cursor?
    if ((pCursor->bits->height <= CURSORHEIGHT) &&
        (pCursor->bits->width <= CURSORWIDTH) &&
        // FIXME: condition not needed when kernel cursor works
        dfb->canHWCursor) {
        if (dfb->cursorMode == 0)    // remove the X cursor
            (*dfb->spriteFuncs->SetCursor)(pDev, pScreen, NullCursor, x, y);
        dfb->cursorMode = 1;         // kernel cursor

        // change the cursor image in shared memory
        if (dfb->bitsPerPixel == 8)
        {
            XFIOKitRealizeCursor8(pScreen, pCursor);
            memcpy((void *)cshmem->cursor.bw8.image[0], dfb->cursor.bw8.image[0], CURSORWIDTH*CURSORHEIGHT);
            memcpy((void *)cshmem->cursor.bw8.mask[0], dfb->cursor.bw8.mask[0], CURSORWIDTH*CURSORHEIGHT);
        }
        else if (dfb->bitsPerPixel == 16)
        {
            XFIOKitRealizeCursor15(pScreen, pCursor);
            memcpy((void *)cshmem->cursor.rgb.image[0], dfb->cursor.rgb.image[0], 2*CURSORWIDTH*CURSORHEIGHT);
        }
        else
        {
            XFIOKitRealizeCursor24(pScreen, pCursor);
            memcpy((void *)cshmem->cursor.rgb24.image[0], dfb->cursor.rgb24.image[0], 4*CURSORWIDTH*CURSORHEIGHT);
        }

        // FIXME: We always use a full size cursor, even if the image
        // is smaller because I couldn't get the padding to come out
        // right otherwise.
//        cshmem->cursorSize[0].width = CURSORWIDTH;
//        cshmem->cursorSize[0].height = CURSORHEIGHT;
        cshmem->cursorSize[0].width = pCursor->bits->width;
        cshmem->cursorSize[0].height = pCursor->bits->height;
        cshmem->hotSpot[0].x = pCursor->bits->xhot;
        cshmem->hotSpot[0].y = pCursor->bits->yhot;

        // try to use a hardware cursor
        if (dfb->canHWCursor) {
            kr = IOFBSetNewCursor(dfb->fbService, 0, 0, 0);
            // FIXME: this is a fatal error without the kernel cursor
            kern_assert( kr );
#if 0
            if (kr != KERN_SUCCESS) {
                ErrorF("Could not set new cursor with kernel return 0x%x.\n", kr);
                dfb->canHWCursor = FALSE;
            }
#endif
        }

        // make the new cursor visible
        if (cshmem->cursorShow)
            cshmem->cursorShow--;

        if (!cshmem->cursorShow && dfb->canHWCursor) {
            kr = IOFBSetCursorVisible(dfb->fbService, TRUE);
            // FIXME: this is a fatal error without the kernel cursor
            kern_assert( kr );
#if 0
            if (kr != KERN_SUCCESS) {
                ErrorF("Couldn't set hardware cursor visible with kernel return 0x%x.\n", kr);
                dfb->canHWCursor = FALSE;
            } else
#endif
                dfb->cursorMode = 2;     // hardware cursor
        }

	return; 
    }

    // otherwise we use a software cursor
    if (dfb->cursorMode) {
        /* remove the kernel or hardware cursor */
        XFIOKitSetCursor(pDev, pScreen, 0, x, y);
    }

    dfb->cursorMode = 0;
    (*dfb->spriteFuncs->SetCursor)(pDev, pScreen, pCursor, x, y);
}


/*
 * XFIOKitMoveCursor
 * Move the cursor. This is a noop for a kernel or hardware cursor.
 */
static void
XFIOKitMoveCursor(
    DeviceIntPtr pDev,
    ScreenPtr   pScreen,
    int         x,
    int         y)
{
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);
    
    // only the X cursor needs to be explicitly moved
    if (!dfb->cursorMode)
        (*dfb->spriteFuncs->MoveCursor)(pDev, pScreen, x, y);
}

/*
 * XFIOKitDeviceCursorInitialize
 */
static Bool
XFIOKitDeviceCursorInitialize(
                  DeviceIntPtr pDev,
                  ScreenPtr   pScreen)
{
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);
    
    return (*dfb->spriteFuncs->DeviceCursorInitialize)(pDev, pScreen);
}

/*
 * XFIOKitDeviceCursorCleanup
 */
static void
XFIOKitDeviceCursorCleanup(
                  DeviceIntPtr pDev,
                  ScreenPtr   pScreen)
{
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);
    
    (*dfb->spriteFuncs->DeviceCursorCleanup)(pDev, pScreen);
}

static miPointerSpriteFuncRec darwinSpriteFuncsRec = {
    XFIOKitRealizeCursor,
    XFIOKitUnrealizeCursor,
    XFIOKitSetCursor,
    XFIOKitMoveCursor,
    XFIOKitDeviceCursorInitialize,
    XFIOKitDeviceCursorCleanup
};

/*
===========================================================================

 Pointer screen functions

===========================================================================
*/

/*
 * XFIOKitCursorOffScreen
 */
static Bool XFIOKitCursorOffScreen(ScreenPtr *pScreen, int *x, int *y)
{	return FALSE;
}


/*
 * XFIOKitCrossScreen
 */
static void XFIOKitCrossScreen(ScreenPtr pScreen, Bool entering)
{	return;
}


/*
 * XFIOKitWarpCursor
 * Change the cursor position without generating an event or motion history
 */
static void
XFIOKitWarpCursor(
    DeviceIntPtr            pDev,
    ScreenPtr               pScreen,
    int                     x,
    int                     y)
{
    kern_return_t           kr;

    kr = IOHIDSetMouseLocation( xfIOKitInputConnect, x, y );
    if (kr != KERN_SUCCESS) {
        ErrorF("Could not set cursor position with kernel return 0x%x.\n", kr);
    }
    miPointerWarpCursor(pDev, pScreen, x, y);
}

static miPointerScreenFuncRec darwinScreenFuncsRec = {
  XFIOKitCursorOffScreen,
  XFIOKitCrossScreen,
  XFIOKitWarpCursor,
//  DarwinEQPointerPost,
//  DarwinEQSwitchScreen
};


/*
===========================================================================

 Other screen functions

===========================================================================
*/

/*
 * XFIOKitCursorQueryBestSize
 * Handle queries for best cursor size
 */
static void
XFIOKitCursorQueryBestSize(
   int              class, 
   unsigned short   *width,
   unsigned short   *height,
   ScreenPtr        pScreen)
{
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);
    
    if (class == CursorShape) {
        *width = CURSORWIDTH;
        *height = CURSORHEIGHT;
    } else
        (*dfb->QueryBestSize)(class, width, height, pScreen);
}


/*
 * XFIOKitInitCursor
 * Initialize cursor support
 */
Bool 
XFIOKitInitCursor(ScreenPtr	pScreen)
{
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);

    miPointerScreenPtr	    PointPriv;
    kern_return_t           kr;

    // start with no cursor displayed
    if (!dfb->cursorShmem->cursorShow++) {
        if (dfb->cursorShmem->hardwareCursorActive) {
            kr = IOFBSetCursorVisible(dfb->fbService, FALSE);
            kern_assert( kr );
        }
    }

    // initialize software cursor handling (always needed as backup)
    if (!miDCInitialize(pScreen, &darwinScreenFuncsRec)) {
        return FALSE;
    }
    
    // check if a hardware cursor is supported
    if (!dfb->cursorShmem->hardwareCursorCapable) {
        dfb->canHWCursor = FALSE;
        ErrorF("Hardware cursor not supported.\n");
    } else {
        // we need to make sure that the hardware cursor really works
        dfb->canHWCursor = TRUE;
        kr = IOFBSetNewCursor(dfb->fbService, 0, 0, 0);
        if (kr != KERN_SUCCESS) {
            ErrorF("Could not set hardware cursor with kernel return 0x%x.\n", kr);
            dfb->canHWCursor = FALSE;
        }
        kr = IOFBSetCursorVisible(dfb->fbService, TRUE);
        if (kr != KERN_SUCCESS) {
            ErrorF("Couldn't set hardware cursor visible with kernel return 0x%x.\n", kr);
            dfb->canHWCursor = FALSE;
        }
        IOFBSetCursorVisible(dfb->fbService, FALSE);
    }

    dfb->cursorMode = 0;
    dfb->pInstalledMap = NULL;

    // override some screen procedures
    dfb->QueryBestSize = pScreen->QueryBestSize;
    pScreen->QueryBestSize = XFIOKitCursorQueryBestSize;
    //ScreenPriv->ConstrainCursor = pScreen->ConstrainCursor;
    //pScreen->ConstrainCursor = XFIOKitConstrainCursor;

    // initialize hardware cursor handling
    PointPriv = dixLookupPrivate(&pScreen->devPrivates, miPointerScreenKey);
    
    dfb->spriteFuncs = PointPriv->spriteFuncs; //Saving spriteFuncs to call them later from the custom funcs
    
    //Custom funcs
    PointPriv->spriteFuncs = &darwinSpriteFuncsRec;

    /* Other routines that might be overridden */
/*
    CursorLimitsProcPtr		CursorLimits;
    RecolorCursorProcPtr	RecolorCursor;
*/

    return TRUE;
}
