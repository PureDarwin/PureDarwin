/*
 * Copyright (c) 2009-2010,2012,2014 Apple Inc. All Rights Reserved.
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

/*
 * policytree.c - rfc5280 section 6.1.2 and later policy_tree implementation.
 */

#include "policytree.h"
#include <libDER/oids.h>

#include <utilities/debugging.h>

#include <stdlib.h>

#define DUMP_POLICY_TREE  0

#if !defined(DUMP_POLICY_TREE)
#include <stdio.h>
#endif

static policy_set_t policy_set_create(const oid_t *p_oid) {
    policy_set_t policy_set =
        (policy_set_t)malloc(sizeof(*policy_set));
    policy_set->oid_next = NULL;
    policy_set->oid = *p_oid;
    secdebug("policy-set", "%p", policy_set);
    return policy_set;
}

void policy_set_add(policy_set_t *policy_set, const oid_t *p_oid) {
    policy_set_t node = (policy_set_t)malloc(sizeof(*node));
    node->oid_next = *policy_set;
    node->oid = *p_oid;
    *policy_set = node;
    secdebug("policy-set", "%p -> %p", node, node->oid_next);
}

void policy_set_free(policy_set_t node) {
    while (node) {
        policy_set_t next = node->oid_next;
        secdebug("policy-set", "%p -> %p", node, next);
        free(node);
        node = next;
    }
}

bool policy_set_contains(policy_set_t node, const oid_t *oid) {
    for (; node;  node = node->oid_next) {
        if (oid_equal(node->oid, *oid))
            return true;
    }
    return false;
}

void policy_set_intersect(policy_set_t *policy_set, policy_set_t other_set) {
    bool other_has_any = policy_set_contains(other_set, &oidAnyPolicy);
    if (policy_set_contains(*policy_set, &oidAnyPolicy)) {
        policy_set_t node = other_set;
        if (other_has_any) {
            /* Both sets contain anyPolicy so the intersection is anyPolicy
               plus all oids in either set.  */
            while (node) {
                if (!policy_set_contains(*policy_set, &node->oid)) {
                    policy_set_add(policy_set, &node->oid);
                }
            }
        } else {
            /* The result set contains anyPolicy and other_set doesn't. The
               result set should be a copy of other_set.  */
            policy_set_free(*policy_set);
            *policy_set = NULL;
            while (node) {
                policy_set_add(policy_set, &node->oid);
            }
        }
    } else if (!other_has_any) {
        /* Neither set contains any policy oid so remove any values from the
           result set that aren't in other_set. */
        policy_set_t *pnode = policy_set;
        while (*pnode) {
            policy_set_t node = *pnode;
            if (policy_set_contains(other_set, &node->oid)) {
                pnode = &node->oid_next;
            } else {
                *pnode = node->oid_next;
                node->oid_next = NULL;
                policy_set_free(node);
            }
        }
    }
}

policy_tree_t policy_tree_create(const oid_t *p_oid, policy_qualifier_t p_q) {
    policy_tree_t node = malloc(sizeof(*node));
    memset(node, 0, sizeof(*node));
    node->valid_policy = *p_oid;
    node->qualifier_set = p_q;
    node->expected_policy_set = policy_set_create(p_oid);
    secdebug("policy-node", "%p", node);
    return node;
}

/* Walk the nodes in a tree at depth and invoke callback for each one. */
bool policy_tree_walk_depth(policy_tree_t root, int depth,
    bool(*callback)(policy_tree_t, void *), void *ctx) {
    policy_tree_t *stack = (policy_tree_t *)malloc(sizeof(policy_tree_t) * (depth + 1));
    if (!stack) {
        return false;
    }
    int stack_ix = 0;
    stack[stack_ix] = root;
    policy_tree_t node;
    bool match = false;
    bool child_visited = false;
    while (stack_ix >= 0) {
        /* stack[stack_ix - 1] is the parent of the current node. */
        node = stack[stack_ix];
        policy_tree_t child = node->children;
        if (!child_visited && stack_ix < depth && child ) {
            /* If we have a child and we haven't reached the
               required depth yet, we go depth first and proccess it. */
            stack[++stack_ix] = child;
        } else {
            /* Get the sibling now in case we delete the node in the callback */
            policy_tree_t sibling = node->siblings;
            if (stack_ix == depth) {
                /* Proccess node. */
                match |= callback(node, ctx);
            }

            /* Move on to sibling of node. */
            if (sibling) {
                /* Replace current node with it's sibling. */
                stack[stack_ix] = sibling;
                child_visited = false;
            } else {
                /* No more siblings left, so pop the stack and backtrack. */
                stack_ix--;
                /* We've handled the top of the stack's child already so
                   just look at it's siblings. */
                child_visited = true;
            }
        }
    }
    free(stack);
    return match;
}

static void policy_tree_free_node(policy_tree_t node) {
    secdebug("policy-node", "%p children: %p siblngs: %p", node,
        node->children, node->siblings);
    if (node->expected_policy_set) {
        policy_set_free(node->expected_policy_set);
        node->expected_policy_set = NULL;
    }
    free(node);
}

void policy_tree_remove_node(policy_tree_t *node) {
    /* Free node's children */
    policy_tree_t *child = &(*node)->children;
    if (*child)
        policy_tree_prune(child);

    /* Remove node from parent */
    policy_tree_t parent = (*node)->parent;
    parent->children = (*node)->siblings;

    /* Free node */
    policy_tree_free_node(*node);
    *node = NULL;
}

/* Prune nodes from node down. */
void policy_tree_prune(policy_tree_t *node) {
    /* Free all our children and siblings. */
    policy_tree_t *child = &(*node)->children;
    if (*child)
        policy_tree_prune(child);
    policy_tree_t *sibling = &(*node)->siblings;
    if (*sibling)
        policy_tree_prune(sibling);
    policy_tree_free_node(*node);
    *node = NULL;
}

/* Prune childless nodes at depth. */
void policy_tree_prune_childless(policy_tree_t *root, int depth) {
    policy_tree_t *stack[depth + 1];
    int stack_ix = 0;
    stack[stack_ix] = root;
    bool child_visited = false;
    while (stack_ix >= 0) {
        policy_tree_t *node;
        node = stack[stack_ix];
        policy_tree_t *child = &(*node)->children;
        if (!child_visited && stack_ix < depth && *child) {
            /* If we have a child and we haven't reached the
               required depth yet, we go depth first and proccess it. */
            stack[++stack_ix] = child;
        } else if (!*child) {
            /* Childless node found, nuke it. */
#if !defined(DUMP_POLICY_TREE)
            printf("# prune /<%.08lx<\\ |%.08lx| >%.08lx> :%s: depth %d\n",
                (intptr_t)node, (intptr_t)*node, (intptr_t)(*node)->siblings,
                (child_visited ? "v" : " "), stack_ix);
#endif

            policy_tree_t next = (*node)->siblings;
            (*node)->siblings = NULL;
            policy_tree_free_node(*node);
            *node = next;
            if (next) {
                /* stack[stack_ix] (node) already points to next now. */
                child_visited = false;
            } else {
                /* No more siblings left, so pop the stack and backtrack. */
                stack_ix--;
                child_visited = true;
            }
        } else {
            policy_tree_t *sibling = &(*node)->siblings;
            if (*sibling) {
                /* Replace current node with it's sibling. */
                stack[stack_ix] = sibling;
                child_visited = false;
            } else {
                /* No more siblings left, so pop the stack and backtrack. */
                stack_ix--;
                child_visited = true;
            }
        }
    }
}

/* Add a new child to the tree. */
static void policy_tree_add_child_explicit(policy_tree_t parent,
    const oid_t *p_oid, policy_qualifier_t p_q, policy_set_t p_expected) {
    policy_tree_t child = malloc(sizeof(*child));
    memset(child, 0, sizeof(*child));
    child->valid_policy = *p_oid;
    child->qualifier_set = p_q;
    child->expected_policy_set = p_expected;
    child->parent = parent;

#if 0
    printf("# /%.08lx\\ |%.08lx| \\%.08lx/ >%.08lx> \\>%.08lx>/\n",
        (intptr_t)parent, (intptr_t)child, (intptr_t)parent->children,
        (intptr_t)parent->siblings,
        (intptr_t)(parent->children ? parent->children->siblings : NULL));
#endif

    /* Previous child becomes new child's first sibling. */
    child->siblings = parent->children;
    /* New child becomes parent's first child. */
    parent->children = child;

    secdebug("policy-node", "%p siblngs: %p", child, child->siblings);
}

/* Add a new child to the tree parent setting valid_policy to p_oid,
   qualifier_set to p_q and expected_policy_set to a set containing
   just p_oid. */
void policy_tree_add_child(policy_tree_t parent,
    const oid_t *p_oid, policy_qualifier_t p_q) {
    policy_set_t policy_set = policy_set_create(p_oid);
    policy_tree_add_child_explicit(parent, p_oid, p_q, policy_set);
}

/* Add a new sibling to the tree setting valid_policy to p_oid,
   qualifier set to p_q and expected_policy_set to p_expected */
void policy_tree_add_sibling(policy_tree_t sibling, const oid_t *p_oid,
                             policy_qualifier_t p_q, policy_set_t p_expected) {
    policy_tree_add_child_explicit(sibling->parent, p_oid, p_q, p_expected);
}

void policy_tree_set_expected_policy(policy_tree_t node,
    policy_set_t p_expected) {
    if (node->expected_policy_set)
        policy_set_free(node->expected_policy_set);
    node->expected_policy_set = p_expected;
}

#if !defined(DUMP_POLICY_TREE)
/* Dump a policy tree. */
static void policy_tree_dump4(policy_tree_t node, policy_tree_t parent,
    policy_tree_t prev_sibling, int depth) {
    policy_tree_t child = node->children;
    policy_tree_t sibling = node->siblings;
    static const char *spaces = "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ";
    printf("# %.*s/%.08lx\\ |%.08lx| <%.08lx< >%.08lx> depth %d\n",
        depth * 11, spaces,
        (intptr_t)parent, (intptr_t)node, (intptr_t)prev_sibling,
        (intptr_t)sibling, depth);
    if (child)
        policy_tree_dump4(child, node, NULL, depth + 1);
    if (sibling)
        policy_tree_dump4(sibling, parent, node, depth);
}
#endif /* !defined(DUMP_POLICY_TREE) */

void policy_tree_dump(policy_tree_t node) {
#if !defined(DUMP_POLICY_TREE)
    policy_tree_dump4(node, NULL, NULL, 0);
#endif /* !defined(DUMP_POLICY_TREE) */
}
