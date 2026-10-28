//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Keyboard input for PureDarwin. fbDOOM's i_input_tty.c relies on
//	Linux console KDSKBMODE/K_MEDIUMRAW raw-scancode ioctls, which have
//	no PureDarwin equivalent (no VT keyboard-mode support), so this just
//	puts the controlling tty into raw mode and reads plain bytes. There
//	is no real key-up event this way; each received byte is posted as a
//	keydown and released on the following poll, which works acceptably
//	because held keys auto-repeat more bytes anyway.
//
//	Also provides the no-op I_InitJoystick/I_UpdateJoystick that
//	fbDOOM's i_joystick.c (SDL-based, not built here) would otherwise
//	supply - there is no joystick, so "do nothing" is the correct
//	behavior, not a stand-in for missing functionality.
//
//	Real mouse look/turn comes from /dev/usb_hid_mouse (the same fixed
//	USBHIDMouseEvent struct src/Userspace/mousemon reads), posted as
//	ev_mouse - DOOM's actual mouse input path (not ev_joystick; there is
//	no separate joystick device here to justify that path).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "doomtype.h"
#include "doomkeys.h"
#include "d_event.h"
#include "i_video.h"
#include "i_system.h"
#include "m_argv.h"

extern int usemouse;

struct USBHIDMouseEvent {
    unsigned int sequence;
    unsigned char mouseIndex;
    unsigned char buttons;
    signed char dx;
    signed char dy;
    signed char wheel;
    unsigned char flags;    // 0x01: absolute, dx/dy hold the x fraction
    unsigned short absY;
};

static int mouse_fd = -1;
static void PollMouse(void);

static struct termios orig_termios;
static boolean have_orig_termios;
static int pending_key = -1;

static unsigned char TranslateKey(unsigned char c)
{
    switch (c) {
    case 27:            return KEY_ESCAPE;
    case '\r':
    case '\n':          return KEY_ENTER;
    case '\t':          return KEY_TAB;
    case 127:
    case 8:              return KEY_BACKSPACE;
    default:
        if (c >= 'A' && c <= 'Z')
            return (unsigned char)(c - 'A' + 'a');
        return c;
    }
}

void I_GetEvent(void)
{
    event_t event;
    unsigned char c;
    ssize_t n;

    PollMouse();

    if (pending_key >= 0) {
        event.type = ev_keyup;
        event.data1 = pending_key;
        event.data2 = -1;
        event.data3 = -1;
        D_PostEvent(&event);
        pending_key = -1;
    }

    n = read(STDIN_FILENO, &c, 1);
    if (n != 1)
        return;

    if (c == 27) {
        /* Could be a bare Escape, or the start of an arrow-key sequence;
         * a real ESC key press won't be immediately followed by more
         * bytes, an arrow sequence will. Non-blocking read below tells
         * them apart without adding input lag for either case. */
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        unsigned char peek;
        ssize_t got = read(STDIN_FILENO, &peek, 1);
        fcntl(STDIN_FILENO, F_SETFL, flags);
        if (got == 1 && (peek == '[' || peek == 'O')) {
            unsigned char buf;
            if (read(STDIN_FILENO, &buf, 1) == 1) {
                switch (buf) {
                case 'A': pending_key = KEY_UPARROW;    break;
                case 'B': pending_key = KEY_DOWNARROW;  break;
                case 'C': pending_key = KEY_RIGHTARROW; break;
                case 'D': pending_key = KEY_LEFTARROW;  break;
                default:  pending_key = -1;             break;
                }
            }
        } else {
            pending_key = KEY_ESCAPE;
        }
    } else if (c == ' ') {
        pending_key = KEY_USE;
    } else {
        pending_key = TranslateKey(c);
    }

    if (pending_key >= 0) {
        event.type = ev_keydown;
        event.data1 = pending_key;
        event.data2 = pending_key;
        event.data3 = pending_key;
        D_PostEvent(&event);
    }
}

static void
PollMouse(void)
{
    struct USBHIDMouseEvent m;

    if (mouse_fd < 0)
        return;

    for (;;) {
        ssize_t n = read(mouse_fd, &m, sizeof(m));
        if (n != (ssize_t)sizeof(m))
            break;

        // doom turns on relative motion only, so a tablet contributes its buttons
        if (m.flags & 0x01)
            m.dx = m.dy = 0;
        if (m.dx == 0 && m.dy == 0 && m.wheel == 0 && m.buttons == 0)
            continue;

        event_t event;
        event.type = ev_mouse;
        event.data1 = m.buttons & 0x7;
        event.data2 = m.dx;
        /* Screen-down dy is positive; DOOM's convention wants forward
         * (mouse pushed away/up) as positive data3. */
        event.data3 = -m.dy;
        D_PostEvent(&event);
    }
}

static void RestoreTerminal(void)
{
    if (have_orig_termios)
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

void I_InitInput(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
        have_orig_termios = true;
        atexit(RestoreTerminal);

        raw = orig_termios;
        cfmakeraw(&raw);
        raw.c_oflag |= OPOST;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    mouse_fd = open("/dev/usb_hid_mouse", O_RDONLY);
    if (mouse_fd >= 0)
        usemouse = 1;
}

void I_InitJoystick(void)
{
}

void I_UpdateJoystick(void)
{
}

void I_BindJoystickVariables(void)
{
}

void kbd_shutdown(void)
{
    RestoreTerminal();
}
