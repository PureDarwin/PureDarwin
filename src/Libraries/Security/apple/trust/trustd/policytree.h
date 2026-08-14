/*
 * Copyright (c) 2009-2010,2012-2014 Apple Inc. All Rights Reserved.
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

/*!
    @header policytree
    The functions provided in policytree.h provide an interface to
    a policy_tree implementation as specified in section 6 of rfc5280.
*/

#ifndef _SECURITY_POLICYTREE_H_
#define _SECURITY_POLICYTREE_H_

#include <libDER/libDER.h>
#include <sys/queue.h>
#include <stdbool.h>

__BEGIN_DECLS


#define oid_equal(oid1, oid2) DEROidCompare(&oid1, &oid2)
typedef DERItem oid_t;
typedef DERItem der_t;

typedef struct policy_set *policy_set_t;
struct policy_set {
    oid_t oid;
    policy_set_t oid_next;
};

typedef const DERItem *policy_qualifier_t;

typedef struct policy_tree *policy_tree_t;
struct policy_tree {
    oid_t valid_policy;
    policy_qualifier_t qualifier_set;
    policy_set_t expected_policy_set;
    policy_tree_t children;
    policy_tree_t siblings;
    policy_tree_t parent;
};

void policy_set_add(policy_set_t *policy_set, const oid_t *p_oid);
void policy_set_intersect(policy_set_t *policy_set, policy_set_t other_set);
bool policy_set_contains(policy_set_t policy_set, const oid_t *oid);
void policy_set_free(policy_set_t policy_set);

policy_tree_t policy_tree_create(const oid_t *p_oid, policy_qualifier_t p_q);

bool policy_tree_walk_depth(policy_tree_t root, int depth,
    bool(*callback)(policy_tree_t, void *), void *ctx);

void policy_tree_remove_node(policy_tree_t *node);
void policy_tree_prune(policy_tree_t *node);
void policy_tree_prune_childless(policy_tree_t *root, int depth);
void policy_tree_add_child(policy_tree_t parent,
    const oid_t *p_oid, policy_qualifier_t p_q);
void policy_tree_add_sibling(policy_tree_t sibling, const oid_t *p_oid,
                             policy_qualifier_t p_q, policy_set_t p_expected);
void policy_tree_set_expected_policy(policy_tree_t node,
    policy_set_t p_expected);

/* noop unless !defined NDEBUG */
void policy_tree_dump(policy_tree_t node);

__END_DECLS

#endif /* !_SECURITY_POLICYTREE_H_ */
