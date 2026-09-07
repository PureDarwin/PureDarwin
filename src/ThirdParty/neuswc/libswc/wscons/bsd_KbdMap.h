/*
 * Slightly modified xf86KbdBSD.c which is
 *
 * Derived from xf86Kbd.c by S_ren Schmidt (sos@login.dkuug.dk)
 * which is Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 * and from xf86KbdCODrv.c by Holger Veit
 */

#include <stdint.h>

static uint8_t wsUsbMap[] = {
    /* 0 */ KEY_NOTUSED,
    /* 1 */ KEY_NOTUSED,
    /* 2 */ KEY_NOTUSED,
    /* 3 */ KEY_NOTUSED,
    /* 4 */ KEY_A,
    /* 5 */ KEY_B,
    /* 6 */ KEY_C,
    /* 7 */ KEY_D,
    /* 8 */ KEY_E,
    /* 9 */ KEY_F,
    /* 10 */ KEY_G,
    /* 11 */ KEY_H,
    /* 12 */ KEY_I,
    /* 13 */ KEY_J,
    /* 14 */ KEY_K,
    /* 15 */ KEY_L,
    /* 16 */ KEY_M,
    /* 17 */ KEY_N,
    /* 18 */ KEY_O,
    /* 19 */ KEY_P,
    /* 20 */ KEY_Q,
    /* 21 */ KEY_R,
    /* 22 */ KEY_S,
    /* 23 */ KEY_T,
    /* 24 */ KEY_U,
    /* 25 */ KEY_V,
    /* 26 */ KEY_W,
    /* 27 */ KEY_X,
    /* 28 */ KEY_Y,
    /* 29 */ KEY_Z,
    /* 30 */ KEY_1,           /* 1 !*/
    /* 31 */ KEY_2,           /* 2 @ */
    /* 32 */ KEY_3,           /* 3 # */
    /* 33 */ KEY_4,           /* 4 $ */
    /* 34 */ KEY_5,           /* 5 % */
    /* 35 */ KEY_6,           /* 6 ^ */
    /* 36 */ KEY_7,           /* 7 & */
    /* 37 */ KEY_8,           /* 8 * */
    /* 38 */ KEY_9,           /* 9 ( */
    /* 39 */ KEY_0,           /* 0 ) */
    /* 40 */ KEY_Enter,       /* Return  */
    /* 41 */ KEY_Escape,      /* Escape */
    /* 42 */ KEY_BackSpace,   /* Backspace Delete */
    /* 43 */ KEY_Tab,         /* Tab */
    /* 44 */ KEY_Space,       /* Space */
    /* 45 */ KEY_Minus,       /* - _ */
    /* 46 */ KEY_Equal,       /* = + */
    /* 47 */ KEY_LBrace,      /* [ { */
    /* 48 */ KEY_RBrace,      /* ] } */
    /* 49 */ KEY_BSlash,      /* \ | */
    /* 50 */ KEY_BSlash,      /* \ _ # ~ on some keyboards */
    /* 51 */ KEY_SemiColon,   /* ; : */
    /* 52 */ KEY_Quote,       /* ' " */
    /* 53 */ KEY_Tilde,       /* ` ~ */
    /* 54 */ KEY_Comma,       /* , <  */
    /* 55 */ KEY_Period,      /* . > */
    /* 56 */ KEY_Slash,       /* / ? */
    /* 57 */ KEY_CapsLock,    /* Caps Lock */
    /* 58 */ KEY_F1,          /* F1 */
    /* 59 */ KEY_F2,          /* F2 */
    /* 60 */ KEY_F3,          /* F3 */
    /* 61 */ KEY_F4,          /* F4 */
    /* 62 */ KEY_F5,          /* F5 */
    /* 63 */ KEY_F6,          /* F6 */
    /* 64 */ KEY_F7,          /* F7 */
    /* 65 */ KEY_F8,          /* F8 */
    /* 66 */ KEY_F9,          /* F9 */
    /* 67 */ KEY_F10,         /* F10 */
    /* 68 */ KEY_F11,         /* F11 */
    /* 69 */ KEY_F12,         /* F12 */
    /* 70 */ KEY_Print,       /* PrintScrn SysReq */
    /* 71 */ KEY_ScrollLock,  /* Scroll Lock */
    /* 72 */ KEY_Pause,       /* Pause Break */
    /* 73 */ KEY_Insert,      /* Insert XXX  Help on some Mac Keyboards */
    /* 74 */ KEY_Home,        /* Home */
    /* 75 */ KEY_PgUp,        /* Page Up */
    /* 76 */ KEY_Delete,      /* Delete */
    /* 77 */ KEY_End,         /* End */
    /* 78 */ KEY_PgDown,      /* Page Down */
    /* 79 */ KEY_Right,       /* Right Arrow */
    /* 80 */ KEY_Left,        /* Left Arrow */
    /* 81 */ KEY_Down,        /* Down Arrow */
    /* 82 */ KEY_Up,          /* Up Arrow */
    /* 83 */ KEY_NumLock,     /* Num Lock */
    /* 84 */ KEY_KP_Divide,   /* Keypad / */
    /* 85 */ KEY_KP_Multiply, /* Keypad * */
    /* 86 */ KEY_KP_Minus,    /* Keypad - */
    /* 87 */ KEY_KP_Plus,     /* Keypad + */
    /* 88 */ KEY_KP_Enter,    /* Keypad Enter */
    /* 89 */ KEY_KP_1,        /* Keypad 1 End */
    /* 90 */ KEY_KP_2,        /* Keypad 2 Down */
    /* 91 */ KEY_KP_3,        /* Keypad 3 Pg Down */
    /* 92 */ KEY_KP_4,        /* Keypad 4 Left  */
    /* 93 */ KEY_KP_5,        /* Keypad 5 */
    /* 94 */ KEY_KP_6,        /* Keypad 6 */
    /* 95 */ KEY_KP_7,        /* Keypad 7 Home */
    /* 96 */ KEY_KP_8,        /* Keypad 8 Up */
    /* 97 */ KEY_KP_9,        /* KEypad 9 Pg Up */
    /* 98 */ KEY_KP_0,        /* Keypad 0 Ins */
    /* 99 */ KEY_KP_Decimal,  /* Keypad . Del */
    /* 100 */ KEY_Less,       /* < > on some keyboards */
    /* 101 */ KEY_Menu,       /* Menu */
    /* 102 */ KEY_Power,      /* sleep key on Sun USB */
    /* 103 */ KEY_KP_Equal,   /* Keypad = on Mac keyboards */
    /* 104 */ KEY_F13,
    /* 105 */ KEY_F14,
    /* 106 */ KEY_F15,
    /* 107 */ KEY_F16,
    /* 108 */ KEY_NOTUSED,
    /* 109 */ KEY_Power,
    /* 110 */ KEY_NOTUSED,
    /* 111 */ KEY_NOTUSED,
    /* 112 */ KEY_NOTUSED,
    /* 113 */ KEY_NOTUSED,
    /* 114 */ KEY_NOTUSED,
    /* 115 */ KEY_NOTUSED,
    /* 116 */ KEY_L7,
    /* 117 */ KEY_Help,
    /* 118 */ KEY_L3,
    /* 119 */ KEY_L5,
    /* 120 */ KEY_L1,
    /* 121 */ KEY_L2,
    /* 122 */ KEY_L4,
    /* 123 */ KEY_L10,
    /* 124 */ KEY_L6,
    /* 125 */ KEY_L8,
    /* 126 */ KEY_L9,
    /* 127 */ KEY_Mute,
    /* 128 */ KEY_AudioRaise,
    /* 129 */ KEY_AudioLower,
    /* 130 */ KEY_NOTUSED,
    /* 131 */ KEY_NOTUSED,
    /* 132 */ KEY_NOTUSED,
    /* 133 */ KEY_NOTUSED,
    /* 134 */ KEY_NOTUSED,
/*
 * Special keycodes for Japanese keyboards
 * Override atKeyname HKTG and BSlash2 code to unique values for JP106 keyboards
 */
#undef KEY_HKTG
#define KEY_HKTG 200 /* Japanese Hiragana Katakana Toggle */
#undef KEY_BSlash2
#define KEY_BSlash2 203 /* Japanese '\_' key */

    /* 135 */ KEY_BSlash2, /* Japanese 106 kbd: '\_' */
    /* 136 */ KEY_HKTG,    /* Japanese 106 kbd: Hiragana Katakana toggle */
    /* 137 */ KEY_Yen,     /* Japanese 106 kbd: '\|' */
    /* 138 */ KEY_XFER,    /* Japanese 106 kbd: Henkan */
    /* 139 */ KEY_NFER,    /* Japanese 106 kbd: Muhenkan */
    /* 140 */ KEY_NOTUSED,
    /* 141 */ KEY_NOTUSED,
    /* 142 */ KEY_NOTUSED,
    /* 143 */ KEY_NOTUSED,
/*
 * Special keycodes for Korean keyboards
 * Define Hangul and Hangul_Hanja unique key codes
 * These keys also use KANA and EISU on some Macintosh Japanese USB keyboards
 */
#define KEY_Hangul 201          /* Also KANA Key on Mac JP USB kbd */
#define KEY_Hangul_Hanja 202    /* Also EISU Key on Mac JP USB kbd */
    /* 144 */ KEY_Hangul,       /* Korean 106 kbd: Hangul */
    /* 145 */ KEY_Hangul_Hanja, /* Korean 106 kbd: Hangul Hanja */
    /* 146 */ KEY_NOTUSED,
    /* 147 */ KEY_NOTUSED,
    /* 148 */ KEY_NOTUSED,
    /* 149 */ KEY_NOTUSED,
    /* 150 */ KEY_NOTUSED,
    /* 151 */ KEY_NOTUSED,
    /* 152 */ KEY_NOTUSED,
    /* 153 */ KEY_NOTUSED,
    /* 154 */ KEY_NOTUSED,
    /* 155 */ KEY_NOTUSED,
    /* 156 */ KEY_NOTUSED,
    /* 157 */ KEY_NOTUSED,
    /* 158 */ KEY_NOTUSED,
    /* 159 */ KEY_NOTUSED,
    /* 160 */ KEY_NOTUSED,
    /* 161 */ KEY_NOTUSED,
    /* 162 */ KEY_NOTUSED,
    /* 163 */ KEY_NOTUSED,
    /* 164 */ KEY_NOTUSED,
    /* 165 */ KEY_NOTUSED,
    /* 166 */ KEY_NOTUSED,
    /* 167 */ KEY_NOTUSED,
    /* 168 */ KEY_NOTUSED,
    /* 169 */ KEY_NOTUSED,
    /* 170 */ KEY_NOTUSED,
    /* 171 */ KEY_NOTUSED,
    /* 172 */ KEY_NOTUSED,
    /* 173 */ KEY_NOTUSED,
    /* 174 */ KEY_NOTUSED,
    /* 175 */ KEY_NOTUSED,
    /* 176 */ KEY_NOTUSED,
    /* 177 */ KEY_NOTUSED,
    /* 178 */ KEY_NOTUSED,
    /* 179 */ KEY_NOTUSED,
    /* 180 */ KEY_NOTUSED,
    /* 181 */ KEY_NOTUSED,
    /* 182 */ KEY_NOTUSED,
    /* 183 */ KEY_NOTUSED,
    /* 184 */ KEY_NOTUSED,
    /* 185 */ KEY_NOTUSED,
    /* 186 */ KEY_NOTUSED,
    /* 187 */ KEY_NOTUSED,
    /* 188 */ KEY_NOTUSED,
    /* 189 */ KEY_NOTUSED,
    /* 190 */ KEY_NOTUSED,
    /* 191 */ KEY_NOTUSED,
    /* 192 */ KEY_NOTUSED,
    /* 193 */ KEY_NOTUSED,
    /* 194 */ KEY_NOTUSED,
    /* 195 */ KEY_NOTUSED,
    /* 196 */ KEY_NOTUSED,
    /* 197 */ KEY_NOTUSED,
    /* 198 */ KEY_NOTUSED,
    /* 199 */ KEY_NOTUSED,
    /* 200 */ KEY_NOTUSED,
    /* 201 */ KEY_NOTUSED,
    /* 202 */ KEY_NOTUSED,
    /* 203 */ KEY_NOTUSED,
    /* 204 */ KEY_NOTUSED,
    /* 205 */ KEY_NOTUSED,
    /* 206 */ KEY_NOTUSED,
    /* 207 */ KEY_NOTUSED,
    /* 208 */ KEY_NOTUSED,
    /* 209 */ KEY_NOTUSED,
    /* 210 */ KEY_NOTUSED,
    /* 211 */ KEY_NOTUSED,
    /* 212 */ KEY_NOTUSED,
    /* 213 */ KEY_NOTUSED,
    /* 214 */ KEY_NOTUSED,
    /* 215 */ KEY_NOTUSED,
    /* 216 */ KEY_NOTUSED,
    /* 217 */ KEY_NOTUSED,
    /* 218 */ KEY_NOTUSED,
    /* 219 */ KEY_NOTUSED,
    /* 220 */ KEY_NOTUSED,
    /* 221 */ KEY_NOTUSED,
    /* 222 */ KEY_NOTUSED,
    /* 223 */ KEY_NOTUSED,
    /* 224 */ KEY_LCtrl,   /* Left Control */
    /* 225 */ KEY_ShiftL,  /* Left Shift */
    /* 226 */ KEY_Alt,     /* Left Alt */
    /* 227 */ KEY_LMeta,   /* Left Meta */
    /* 228 */ KEY_RCtrl,   /* Right Control */
    /* 229 */ KEY_ShiftR,  /* Right Shift */
    /* 230 */ KEY_AltLang, /* Right Alt, AKA AltGr */
    /* 231 */ KEY_LMeta,   /* Right Meta XXX */
};

static uint8_t wsXtMap[] = {
    /* 0 */ KEY_NOTUSED,
    /* 1 */ KEY_Escape,
    /* 2 */ KEY_1,
    /* 3 */ KEY_2,
    /* 4 */ KEY_3,
    /* 5 */ KEY_4,
    /* 6 */ KEY_5,
    /* 7 */ KEY_6,
    /* 8 */ KEY_7,
    /* 9 */ KEY_8,
    /* 10 */ KEY_9,
    /* 11 */ KEY_0,
    /* 12 */ KEY_Minus,
    /* 13 */ KEY_Equal,
    /* 14 */ KEY_BackSpace,
    /* 15 */ KEY_Tab,
    /* 16 */ KEY_Q,
    /* 17 */ KEY_W,
    /* 18 */ KEY_E,
    /* 19 */ KEY_R,
    /* 20 */ KEY_T,
    /* 21 */ KEY_Y,
    /* 22 */ KEY_U,
    /* 23 */ KEY_I,
    /* 24 */ KEY_O,
    /* 25 */ KEY_P,
    /* 26 */ KEY_LBrace,
    /* 27 */ KEY_RBrace,
    /* 28 */ KEY_Enter,
    /* 29 */ KEY_LCtrl,
    /* 30 */ KEY_A,
    /* 31 */ KEY_S,
    /* 32 */ KEY_D,
    /* 33 */ KEY_F,
    /* 34 */ KEY_G,
    /* 35 */ KEY_H,
    /* 36 */ KEY_J,
    /* 37 */ KEY_K,
    /* 38 */ KEY_L,
    /* 39 */ KEY_SemiColon,
    /* 40 */ KEY_Quote,
    /* 41 */ KEY_Tilde,
    /* 42 */ KEY_ShiftL,
    /* 43 */ KEY_BSlash,
    /* 44 */ KEY_Z,
    /* 45 */ KEY_X,
    /* 46 */ KEY_C,
    /* 47 */ KEY_V,
    /* 48 */ KEY_B,
    /* 49 */ KEY_N,
    /* 50 */ KEY_M,
    /* 51 */ KEY_Comma,
    /* 52 */ KEY_Period,
    /* 53 */ KEY_Slash,
    /* 54 */ KEY_ShiftR,
    /* 55 */ KEY_KP_Multiply,
    /* 56 */ KEY_Alt,
    /* 57 */ KEY_Space,
    /* 58 */ KEY_CapsLock,
    /* 59 */ KEY_F1,
    /* 60 */ KEY_F2,
    /* 61 */ KEY_F3,
    /* 62 */ KEY_F4,
    /* 63 */ KEY_F5,
    /* 64 */ KEY_F6,
    /* 65 */ KEY_F7,
    /* 66 */ KEY_F8,
    /* 67 */ KEY_F9,
    /* 68 */ KEY_F10,
    /* 69 */ KEY_NumLock,
    /* 70 */ KEY_ScrollLock,
    /* 71 */ KEY_KP_7,
    /* 72 */ KEY_KP_8,
    /* 73 */ KEY_KP_9,
    /* 74 */ KEY_KP_Minus,
    /* 75 */ KEY_KP_4,
    /* 76 */ KEY_KP_5,
    /* 77 */ KEY_KP_6,
    /* 78 */ KEY_KP_Plus,
    /* 79 */ KEY_KP_1,
    /* 80 */ KEY_KP_2,
    /* 81 */ KEY_KP_3,
    /* 82 */ KEY_KP_0,
    /* 83 */ KEY_KP_Decimal,
    /* 84 */ KEY_NOTUSED,
    /* 85 */ KEY_NOTUSED,
    /* 86 */ KEY_Less, /* backslash on uk, < on german */
    /* 87 */ KEY_F11,
    /* 88 */ KEY_F12,
    /* 89 */ KEY_NOTUSED,
    /* 90 */ KEY_NOTUSED,
    /* 91 */ KEY_NOTUSED,
    /* 92 */ KEY_NOTUSED,
    /* 93 */ KEY_NOTUSED,
    /* 94 */ KEY_NOTUSED,
    /* 95 */ KEY_NOTUSED,
    /* 96 */ KEY_NOTUSED,
    /* 97 */ KEY_NOTUSED,
    /* 98 */ KEY_NOTUSED,
    /* 99 */ KEY_NOTUSED,
    /* 100 */ KEY_NOTUSED,
    /* 101 */ KEY_NOTUSED,
    /* 102 */ KEY_NOTUSED,
    /* 103 */ KEY_NOTUSED,
    /* 104 */ KEY_NOTUSED,
    /* 105 */ KEY_NOTUSED,
    /* 106 */ KEY_NOTUSED,
    /* 107 */ KEY_NOTUSED,
    /* 108 */ KEY_NOTUSED,
    /* 109 */ KEY_NOTUSED,
    /* 110 */ KEY_NOTUSED,
    /* 111 */ KEY_NOTUSED,
    /* 112 */ KEY_NOTUSED,
    /* 113 */ KEY_NOTUSED,
    /* 114 */ KEY_NOTUSED,
    /* 115 */ KEY_NOTUSED,
    /* 116 */ KEY_NOTUSED,
    /* 117 */ KEY_NOTUSED,
    /* 118 */ KEY_NOTUSED,
    /* 119 */ KEY_NOTUSED,
    /* 120 */ KEY_NOTUSED,
    /* 121 */ KEY_NOTUSED,
    /* 122 */ KEY_NOTUSED,
    /* 123 */ KEY_NOTUSED,
    /* 124 */ KEY_NOTUSED,
    /* 125 */ KEY_NOTUSED,
    /* 126 */ KEY_NOTUSED,
    /* 127 */ KEY_Pause,
    /* 128 */ KEY_NOTUSED,
    /* 129 */ KEY_NOTUSED,
    /* 130 */ KEY_NOTUSED,
    /* 131 */ KEY_NOTUSED,
    /* 132 */ KEY_NOTUSED,
    /* 133 */ KEY_NOTUSED,
    /* 134 */ KEY_NOTUSED,
    /* 135 */ KEY_NOTUSED,
    /* 136 */ KEY_NOTUSED,
    /* 137 */ KEY_NOTUSED,
    /* 138 */ KEY_NOTUSED,
    /* 139 */ KEY_NOTUSED,
    /* 140 */ KEY_NOTUSED,
    /* 141 */ KEY_NOTUSED,
    /* 142 */ KEY_NOTUSED,
    /* 143 */ KEY_NOTUSED,
    /* 144 */ KEY_NOTUSED,
    /* 145 */ KEY_NOTUSED,
    /* 146 */ KEY_NOTUSED,
    /* 147 */ KEY_NOTUSED,
    /* 148 */ KEY_NOTUSED,
    /* 149 */ KEY_NOTUSED,
    /* 150 */ KEY_NOTUSED,
    /* 151 */ KEY_NOTUSED,
    /* 152 */ KEY_NOTUSED,
    /* 153 */ KEY_NOTUSED,
    /* 154 */ KEY_NOTUSED,
    /* 155 */ KEY_NOTUSED,
    /* 156 */ KEY_KP_Enter,
    /* 157 */ KEY_RCtrl,
    /* 158 */ KEY_NOTUSED,
    /* 159 */ KEY_NOTUSED,
    /* 160 */ KEY_Mute,
    /* 161 */ KEY_NOTUSED,
    /* 162 */ KEY_NOTUSED,
    /* 163 */ KEY_NOTUSED,
    /* 164 */ KEY_NOTUSED,
    /* 165 */ KEY_NOTUSED,
    /* 166 */ KEY_NOTUSED,
    /* 167 */ KEY_NOTUSED,
    /* 168 */ KEY_NOTUSED,
    /* 169 */ KEY_NOTUSED,
    /* 170 */ KEY_Print,
    /* 171 */ KEY_NOTUSED,
    /* 172 */ KEY_NOTUSED,
    /* 173 */ KEY_NOTUSED,
    /* 174 */ KEY_AudioLower,
    /* 175 */ KEY_AudioRaise,
    /* 176 */ KEY_NOTUSED,
    /* 177 */ KEY_NOTUSED,
    /* 178 */ KEY_NOTUSED,
    /* 179 */ KEY_NOTUSED,
    /* 180 */ KEY_NOTUSED,
    /* 181 */ KEY_KP_Divide,
    /* 182 */ KEY_NOTUSED,
    /* 183 */ KEY_Print,
    /* 184 */ KEY_AltLang,
    /* 185 */ KEY_NOTUSED,
    /* 186 */ KEY_NOTUSED,
    /* 187 */ KEY_NOTUSED,
    /* 188 */ KEY_NOTUSED,
    /* 189 */ KEY_NOTUSED,
    /* 190 */ KEY_NOTUSED,
    /* 191 */ KEY_NOTUSED,
    /* 192 */ KEY_NOTUSED,
    /* 193 */ KEY_NOTUSED,
    /* 194 */ KEY_NOTUSED,
    /* 195 */ KEY_NOTUSED,
    /* 196 */ KEY_NOTUSED,
    /* 197 */ KEY_NOTUSED,
    /* 198 */ KEY_NOTUSED,
    /* 199 */ KEY_Home,
    /* 200 */ KEY_Up,
    /* 201 */ KEY_PgUp,
    /* 202 */ KEY_NOTUSED,
    /* 203 */ KEY_Left,
    /* 204 */ KEY_NOTUSED,
    /* 205 */ KEY_Right,
    /* 206 */ KEY_NOTUSED,
    /* 207 */ KEY_End,
    /* 208 */ KEY_Down,
    /* 209 */ KEY_PgDown,
    /* 210 */ KEY_Insert,
    /* 211 */ KEY_Delete,
    /* 212 */ KEY_NOTUSED,
    /* 213 */ KEY_NOTUSED,
    /* 214 */ KEY_NOTUSED,
    /* 215 */ KEY_NOTUSED,
    /* 216 */ KEY_NOTUSED,
    /* 217 */ KEY_NOTUSED,
    /* 218 */ KEY_NOTUSED,
    /* 219 */ KEY_LMeta,
    /* 220 */ KEY_RMeta,
    /* 221 */ KEY_Menu,
};
