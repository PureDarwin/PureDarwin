// SPDX-License-Identifier: MIT
// iosurfaced: hands out global IOSurface IDs and answers IOSurfaceLookup across processes
// each surface's token port dies with its creator's right, which retires the ID
#include <mach/mach.h>
#include <mach/mach_port.h>
#include <mach/notify.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iosurfaced.h"

struct record {
    uint32_t id;
    mach_port_t token;
    mach_port_t entry;
    struct record *next;
};

static struct record *records;
static uint32_t nextID = 1;
static mach_port_t service;

static struct record *byID(uint32_t id) {
    for (struct record *r = records; r; r = r->next)
        if (r->id == id)
            return r;
    return NULL;
}

static void retire(mach_port_t token) {
    for (struct record **p = &records; *p; p = &(*p)->next) {
        struct record *r = *p;

        if (r->token != token)
            continue;
        *p = r->next;
        mach_port_deallocate(mach_task_self(), r->entry);
        mach_port_deallocate(mach_task_self(), r->token);
        free(r);
        return;
    }
}

static void reply(mach_port_t to, kern_return_t result, uint32_t id) {
    struct iosurfaced_reply rep;

    memset(&rep, 0, sizeof(rep));
    // replies carry the request's id plus 100, as MIG's do
    rep.head.msgh_id = IOSURFACED_REGISTER + 100;
    rep.head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
    rep.head.msgh_size = sizeof(rep) - sizeof(rep.trailer);
    rep.head.msgh_remote_port = to;
    rep.result = result;
    rep.id = id;
    mach_msg(&rep.head, MACH_SEND_MSG | MACH_SEND_TIMEOUT, rep.head.msgh_size, 0, MACH_PORT_NULL, 1000, MACH_PORT_NULL);
}

static void handleRegister(struct iosurfaced_register_request *req) {
    struct record *r;
    mach_port_t previous = MACH_PORT_NULL;

    if (!(req->head.msgh_bits & MACH_MSGH_BITS_COMPLEX) || req->body.msgh_descriptor_count != 2 ||
        !(r = calloc(1, sizeof(*r)))) {
        reply(req->head.msgh_remote_port, KERN_INVALID_ARGUMENT, 0);
        return;
    }
    r->token = req->token.name, r->entry = req->entry.name;
    // the low half of the ID space, the frameworks' own IDs live in the high half
    do {
        r->id = nextID++ & 0x7fffffff;
    } while (r->id == 0 || byID(r->id));
    // a token that is already dead is retired at once by the notification
    if (mach_port_request_notification(mach_task_self(), r->token, MACH_NOTIFY_DEAD_NAME, 1, service,
                                       MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous) != KERN_SUCCESS) {
        mach_port_deallocate(mach_task_self(), r->entry);
        mach_port_deallocate(mach_task_self(), r->token);
        free(r);
        reply(req->head.msgh_remote_port, KERN_FAILURE, 0);
        return;
    }
    if (previous != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), previous);
    r->next = records;
    records = r;
    reply(req->head.msgh_remote_port, KERN_SUCCESS, r->id);
}

static void handleLookup(struct iosurfaced_lookup_request *req) {
    struct record *r = byID(req->id);
    struct iosurfaced_lookup_reply rep;

    memset(&rep, 0, sizeof(rep));
    rep.head.msgh_id = IOSURFACED_LOOKUP + 100;
    rep.head.msgh_remote_port = req->head.msgh_remote_port;
    if (r) {
        rep.head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0) | MACH_MSGH_BITS_COMPLEX;
        rep.head.msgh_size = sizeof(rep) - sizeof(rep.trailer);
        rep.body.msgh_descriptor_count = 1;
        rep.entry.name = r->entry;
        rep.entry.disposition = MACH_MSG_TYPE_COPY_SEND;
        rep.entry.type = MACH_MSG_PORT_DESCRIPTOR;
    } else {
        rep.head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
        rep.head.msgh_size = sizeof(rep.head);
    }
    mach_msg(&rep.head, MACH_SEND_MSG | MACH_SEND_TIMEOUT, rep.head.msgh_size, 0, MACH_PORT_NULL, 1000, MACH_PORT_NULL);
}

int main(void) {
    union {
        mach_msg_header_t head;
        struct iosurfaced_register_request reg;
        struct iosurfaced_lookup_request look;
        mach_dead_name_notification_t dead;
        uint8_t space[1024];
    } msg;

    if (bootstrap_check_in(bootstrap_port, IOSURFACED_SERVICE, &service) != KERN_SUCCESS) {
        fprintf(stderr, "iosurfaced: no %s service from launchd\n", IOSURFACED_SERVICE);
        return 1;
    }
    for (;;) {
        memset(&msg.head, 0, sizeof(msg.head));
        if (mach_msg(&msg.head, MACH_RCV_MSG, 0, sizeof(msg), service, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL) !=
            MACH_MSG_SUCCESS)
            continue;
        switch (msg.head.msgh_id) {
        case IOSURFACED_REGISTER:
            handleRegister(&msg.reg);
            break;
        case IOSURFACED_LOOKUP:
            handleLookup(&msg.look);
            break;
        case MACH_NOTIFY_DEAD_NAME:
            retire(msg.dead.not_port);
            // the notification carries its own reference to the dead name
            mach_port_deallocate(mach_task_self(), msg.dead.not_port);
            break;
        default:
            mach_msg_destroy(&msg.head);
            break;
        }
    }
}
