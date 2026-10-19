// SPDX-License-Identifier: MIT
// renderer IDs as CGLDescribeRenderer and kCGLPFARendererID use them

#ifndef _PUREDARWIN_OPENGL_CGLRENDERERS_H
#define _PUREDARWIN_OPENGL_CGLRENDERERS_H

// the vendor and device bits of a renderer ID, the low byte is the revision
#define kCGLRendererIDMatchingMask 0x00FE7F00

#define kCGLRendererGenericID 0x00020200
#define kCGLRendererGenericFloatID 0x00020400
#define kCGLRendererAppleSWID 0x00020600
#define kCGLRendererATIRage128ID 0x00021000
#define kCGLRendererATIRadeonID 0x00021200
#define kCGLRendererATIRageProID 0x00021400
#define kCGLRendererATIRadeon8500ID 0x00021600
#define kCGLRendererATIRadeon9700ID 0x00021800
#define kCGLRendererATIRadeonX1000ID 0x00021900
#define kCGLRendererATIRadeonX2000ID 0x00021A00
#define kCGLRendererATIRadeonX3000ID 0x00021B00
#define kCGLRendererATIRadeonX4000ID 0x00021C00
#define kCGLRendererGeForce2MXID 0x00022000
#define kCGLRendererGeForce3ID 0x00022200
#define kCGLRendererGeForceFXID 0x00022400
#define kCGLRendererGeForce8xxxID 0x00022600
#define kCGLRendererGeForceID 0x00022700
#define kCGLRendererVTBladeXP2ID 0x00023000
#define kCGLRendererIntel900ID 0x00024000
#define kCGLRendererIntelX3100ID 0x00024200
#define kCGLRendererIntelHDID 0x00024300
#define kCGLRendererIntelHD4000ID 0x00024400
#define kCGLRendererIntelHD5000ID 0x00024500
#define kCGLRendererMesa3DFXID 0x00040000

#endif
