/*	UNCUserNotification.h
	Copyright 2000, Apple Computer, Inc. All rights reserved.
*/

#ifndef _UNCUSERNOTIFICATION_H_
#define _UNCUSERNOTIFICATION_H_

enum {
    kUNCStopAlertLevel		= 0,
    kUNCNoteAlertLevel		= 1,
    kUNCCautionAlertLevel	= 2,
    kUNCPlainAlertLevel		= 3
};

enum {
    kUNCDefaultResponse		= 0,
    kUNCAlternateResponse	= 1,
    kUNCOtherResponse		= 2,
    kUNCCancelResponse		= 3
};

enum {
    kUNCNoDefaultButtonFlag 	= (1 << 5),
    kUNCUseRadioButtonsFlag 	= (1 << 6)
};

#define UNCCheckBoxChecked(i)	(1 << (8 + i))
#define UNCSecureTextField(i)	(1 << (16 + i))
#define UNCPopUpSelection(n)	(n << 24)


#endif	/* _UNCUSERNOTIFICATION_H_ */

