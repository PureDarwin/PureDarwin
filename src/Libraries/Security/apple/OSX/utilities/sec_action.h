/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#ifndef _SEC_ACTION_H_
#define _SEC_ACTION_H_

#include <dispatch/dispatch.h>

/*
 * Simple dispatch-based mechanism to coalesce high-frequency actions like
 * notifications. Sample usage:
 *
 *  static void
 *  notify_frequent_event(void)
 *  {
 *      static dispatch_once_t once;
 *      static sec_action_t action;
 *
 *      dispatch_once(&once, ^{
 *          action = sec_action_create("frequent_event", 2);
 *          sec_action_set_handler(action, ^{
 *              (void)notify_post("com.apple.frequent_event");
 *          });
 *      });
 *
 *      sec_action_perform(action);
 *  }
 *
 * The above will prevent com.apple.frequent_event from being posted more than
 * once every 2 seconds. For example, if notify_frequent_event is called 1000 times
 * over the span of 1.9s, the handler will be called twice, at 0s and 2s (approx).
 *
 * Default behavior is to perform actions on a queue with the same QOS as the caller.
 * If the action should be performed on a specific serial queue, the function
 * sec_action_create_with_queue can alternatively be used.
 */

typedef dispatch_source_t sec_action_t;

__BEGIN_DECLS

DISPATCH_MALLOC DISPATCH_RETURNS_RETAINED DISPATCH_WARN_RESULT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
sec_action_t
sec_action_create(const char *label, uint64_t interval);

DISPATCH_MALLOC DISPATCH_RETURNS_RETAINED DISPATCH_WARN_RESULT DISPATCH_NOTHROW
sec_action_t
sec_action_create_with_queue(dispatch_queue_t queue, const char *label, uint64_t interval);

DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
sec_action_set_handler(sec_action_t action, dispatch_block_t handler);

DISPATCH_NONNULL_ALL
void
sec_action_perform(sec_action_t action);

__END_DECLS

#endif /* _SEC_ACTION_H_ */
