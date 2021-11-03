/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _APPLEPS2KEYBOARDDEVICE_H
#define _APPLEPS2KEYBOARDDEVICE_H

#include "ApplePS2Device.h"

class ApplePS2Controller;

class ApplePS2KeyboardDevice : public IOService
{
  OSDeclareDefaultStructors(ApplePS2KeyboardDevice);

private:
  ApplePS2Controller * _controller;

protected:
  struct ExpansionData { /* */ };
  ExpansionData * _expansionData;

public:
  virtual bool attach(IOService * provider) override;
  virtual void detach(IOService * provider) override;

  // Interrupt Handling Routines

  virtual void installInterruptAction(OSObject *, PS2InterruptAction);
  virtual void uninstallInterruptAction();

  // Request Submission Routines

  virtual PS2Request * allocateRequest();
  virtual void         freeRequest(PS2Request * request);
  virtual bool         submitRequest(PS2Request * request);
  virtual void         submitRequestAndBlock(PS2Request * request);

  // Power Control Handling Routines

  virtual void installPowerControlAction(OSObject *, PS2PowerControlAction);
  virtual void uninstallPowerControlAction();

  OSMetaClassDeclareReservedUnused(ApplePS2KeyboardDevice, 0);
  OSMetaClassDeclareReservedUnused(ApplePS2KeyboardDevice, 1);
  OSMetaClassDeclareReservedUnused(ApplePS2KeyboardDevice, 2);
  OSMetaClassDeclareReservedUnused(ApplePS2KeyboardDevice, 3);
  OSMetaClassDeclareReservedUnused(ApplePS2KeyboardDevice, 4);
  OSMetaClassDeclareReservedUnused(ApplePS2KeyboardDevice, 5);
  OSMetaClassDeclareReservedUnused(ApplePS2KeyboardDevice, 6);
  OSMetaClassDeclareReservedUnused(ApplePS2KeyboardDevice, 7);
};

#endif /* !_APPLEPS2KEYBOARDDEVICE_H */
