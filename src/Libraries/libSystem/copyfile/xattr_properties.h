/*
 * Copyright (c) 2013-19 Apple, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _XATTR_PROPERTIES_H
#define _XATTR_PROPERTIES_H

#include <stdint.h>

#include <sys/cdefs.h>
#include <Availability.h>

__BEGIN_DECLS

/*
 * CopyOperationIntent_t is used to declare what the intent of the copy is.
 * Not a bit-field (for now, at least).
 *
 * CopyOperationIntentCopy indicates that the EA is attached to an object
 * that is simply being copied.  E.g., cp src dst
 *
 * CopyOperationIntentSave indicates that the EA is attached to an object
 * being saved; as in a "safe save," the destination is being replaced by
 * the source, so the question is whether the EA should be applied to the
 * destination, or generated anew.
 *
 * CopyOperationIntentShare indicates that the EA is attached to an object that
 * is being given out to other people.  For example, saving to a public folder,
 * or attaching to an email message.
 */

typedef enum {
	CopyOperationIntentCopy = 1,
	CopyOperationIntentSave,
	CopyOperationIntentShare,
} CopyOperationIntent_t;

typedef uint64_t CopyOperationProperties_t;

/*
 * Various properties used to determine how to handle the xattr during
 * copying.  The intent is that the default is reasonable for most xattrs.
 */

/*
 * kCopyOperationPropertyNoExport
 * Declare that the extended property should not be exported; this is
 * deliberately a bit vague, but this is used by CopyOperationIntentShare
 * to indicate not to preserve the xattr.
 */
#define kCopyOperationPropertyNoExport	((CopyOperationProperties_t)0x0001)

/*
 * kCopyOperationPropertyContentDependent
 * Declares the extended attribute to be tied to the contents of the file (or
 * vice versa), such that it should be re-created when the contents of the
 * file change.  Examples might include cryptographic keys, checksums, saved
 * position or search information, and text encoding.
 *
 * This property causes the EA to be preserved for copy and share, but not for
 * safe save.  (In a safe save, the EA exists on the original, and will not
 * be copied to the new version.)
 */
#define	kCopyOperationPropertyContentDependent	((CopyOperationProperties_t)0x0002)

/*
 * kCopyOperationPropertyNeverPreserve
 * Declares that the extended attribute is never to be copied, for any
 * intention type.
 */
#define kCopyOperationPropertyNeverPreserve	((CopyOperationProperties_t)0x0004)

__END_DECLS

#endif /* _XATTR_PROPERTIES_H */
