/*
 * Copyright (c) 2013-2014 Chun-Ying Huang
 * Modification Copyright (c) 2021 me(github.com/am009)
 *
 * This file is part of GamingAnywhere (GA).
 *
 * GA is free software; you can redistribute it and/or modify it
 * under the terms of the 3-clause BSD License as published by the
 * Free Software Foundation: http://directory.fsf.org/wiki/License:BSD_3Clause
 *
 * GA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the 3-clause BSD License along with GA;
 * if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>

#include "ga-common.h"
#include "controller.h"
#include "ctrl-sdl.h"


#include <map>
using namespace std;

static double scaleFactorX = 1.0;
static double scaleFactorY = 1.0;
static int outputW, outputH;	// client window resolution
static int cxsize, cysize;	// for mapping mouse coordinates

// support key blocking
static bool keyblock_initialized = false;
static map<unsigned short, unsigned short> kbScancode;
static map<int, int> kbSdlkey;

#define	INVALID_KEY	0xffff
typedef WORD	KeySym;		// map to Windows Virtual Keycode

static bool keymap_initialized = false;
static void SDLKeyToKeySym_init();
static map<int, KeySym> keymap;
static KeySym SDLKeyToKeySym(int sdlkey);

static struct gaRect *prect = NULL;
static struct gaRect croprect;

// only support SDL2: remap key codes 1.2 -> 2.0
#define	SDLK_KP0	SDLK_KP_0
#define	SDLK_KP1	SDLK_KP_1
#define	SDLK_KP2	SDLK_KP_2
#define	SDLK_KP3	SDLK_KP_3
#define	SDLK_KP4	SDLK_KP_4
#define	SDLK_KP5	SDLK_KP_5
#define	SDLK_KP6	SDLK_KP_6
#define	SDLK_KP7	SDLK_KP_7
#define	SDLK_KP8	SDLK_KP_8
#define	SDLK_KP9	SDLK_KP_9
//
#define SDLK_NUMLOCK	SDLK_NUMLOCKCLEAR
#define SDLK_SCROLLOCK	SDLK_SCROLLLOCK
#define SDLK_RMETA	SDLK_RGUI
#define SDLK_LMETA	SDLK_LGUI
//#define SDLK_LSUPER
//#define SDLK_RSUPER
//#define SDLK_COMPOSE
//#define SDLK_PRINT
#define SDLK_BREAK	SDLK_PRINTSCREEN

sdlmsg_t *
sdlmsg_ntoh(sdlmsg_t *msg) {
	sdlmsg_keyboard_t *msgk = (sdlmsg_keyboard_t*) msg;
	sdlmsg_mouse_t *msgm = (sdlmsg_mouse_t*) msg;
	if(msg == NULL)
		return NULL;
	switch(msg->msgtype) {
	case SDL_EVENT_MSGTYPE_KEYBOARD:
		if(msgk->scancode)	msgk->scancode = ntohs(msgk->scancode);
		if(msgk->sdlkey)	msgk->sdlkey = (int) ntohl(msgk->sdlkey);
		if(msgk->unicode)	msgk->unicode = ntohl(msgk->unicode);
		if(msgk->sdlmod)	msgk->sdlmod = ntohs(msgk->sdlmod);
		break;
	case SDL_EVENT_MSGTYPE_MOUSEKEY:
	case SDL_EVENT_MSGTYPE_MOUSEMOTION:
	case SDL_EVENT_MSGTYPE_MOUSEWHEEL:
		if(msgm->mousex)	msgm->mousex = ntohs(msgm->mousex);
		if(msgm->mousey)	msgm->mousey = ntohs(msgm->mousey);
		if(msgm->mouseRelX)	msgm->mouseRelX = ntohs(msgm->mouseRelX);
		if(msgm->mouseRelY)	msgm->mouseRelY = ntohs(msgm->mouseRelY);
		break;
	}
	return msg;
}

sdlmsg_t *
sdlmsg_keyboard(sdlmsg_t *msg, unsigned char pressed, unsigned short scancode, SDL_Keycode key, unsigned short mod, unsigned int unicode)
{
	sdlmsg_keyboard_t *msgk = (sdlmsg_keyboard_t*) msg;
	//ga_error("sdl client: key event code=%x key=%x mod=%x pressed=%u\n", scancode, key, mod, pressed);
	bzero(msg, sizeof(sdlmsg_keyboard_t));
	msgk->msgsize = htons(sizeof(sdlmsg_keyboard_t));
	msgk->msgtype = SDL_EVENT_MSGTYPE_KEYBOARD;
	msgk->is_pressed = pressed;
	msgk->scancode = htons(scancode);
	msgk->sdlkey = htonl(key);
	msgk->unicode = htonl(unicode);
	msgk->sdlmod = htons(mod);
	return msg;
}

sdlmsg_t *
sdlmsg_mousekey(sdlmsg_t *msg, unsigned char pressed, unsigned char button, unsigned short x, unsigned short y) {
	sdlmsg_mouse_t *msgm = (sdlmsg_mouse_t*) msg;
	//ga_error("sdl client: button event btn=%u pressed=%u\n", button, pressed);
	bzero(msg, sizeof(sdlmsg_mouse_t));
	msgm->msgsize = htons(sizeof(sdlmsg_mouse_t));
	msgm->msgtype = SDL_EVENT_MSGTYPE_MOUSEKEY;
	msgm->is_pressed = pressed;
	msgm->mousex = htons(x);
	msgm->mousey = htons(y);
	msgm->mousebutton = button;
	return msg;
}

sdlmsg_t *
sdlmsg_mousewheel(sdlmsg_t *msg, unsigned short mousex, unsigned short mousey) {
	sdlmsg_mouse_t *msgm = (sdlmsg_mouse_t*) msg;
	//ga_error("sdl client: motion event x=%u y=%u\n", mousex, mousey);
	bzero(msg, sizeof(sdlmsg_mouse_t));
	msgm->msgsize = htons(sizeof(sdlmsg_mouse_t));
	msgm->msgtype = SDL_EVENT_MSGTYPE_MOUSEWHEEL;
	msgm->mousex = htons(mousex);
	msgm->mousey = htons(mousey);
	return msg;
}

sdlmsg_t *
sdlmsg_mousemotion(sdlmsg_t *msg, unsigned short mousex, unsigned short mousey, unsigned short relx, unsigned short rely, unsigned char state, int relativeMouseMode) {
	sdlmsg_mouse_t *msgm = (sdlmsg_mouse_t*) msg;
	//ga_error("sdl client: motion event x=%u y=%u\n", mousex, mousey);
	bzero(msg, sizeof(sdlmsg_mouse_t));
	msgm->msgsize = htons(sizeof(sdlmsg_mouse_t));
	msgm->msgtype = SDL_EVENT_MSGTYPE_MOUSEMOTION;
	msgm->mousestate = state;
	msgm->relativeMouseMode = (relativeMouseMode != 0) ? 1 : 0;
	msgm->mousex = htons(mousex);
	msgm->mousey = htons(mousey);
	msgm->mouseRelX = htons(relx);
	msgm->mouseRelY = htons(rely);
	return msg;
}

int
sdlmsg_replay_init(void* arg) {
	// rectangle related function is removed
	if (keyblock_initialized == false) {
		sdlmsg_kb_init();
		keyblock_initialized = true;
	}
	if (keymap_initialized == false) {
		SDLKeyToKeySym_init();
	}
	//
	ga_error("sdl_replayer: sizeof(sdlmsg) = %d\n", sizeof(sdlmsg_t));
	//
	cxsize = GetSystemMetrics(SM_CXSCREEN);
	cysize = GetSystemMetrics(SM_CYSCREEN);
	ctrl_server_set_resolution(cxsize, cysize);
	ctrl_server_set_output_resolution(cxsize, cysize);
	ga_error("sdl replayer: Replay using SendInput(), screen-size=%dx%d\n", cxsize, cysize);
	// compute scale factor
	do {
		//int resolution[2];
		int baseX, baseY;
		//if (ga_conf_readints("output-resolution", resolution, 2) != 2)
		//	break;
		//
		outputW = cxsize;
		outputH = cysize;
		ctrl_server_set_output_resolution(outputW, outputH);
		//
		baseX = cxsize;
		baseY = cysize;
		ctrl_server_set_resolution(baseX, baseY);
		ctrl_server_get_scalefactor(&scaleFactorX, &scaleFactorY);
		//
		ga_error("sdl replayer: mouse coordinate scale factor = (%.3f,%.3f)\n",
			scaleFactorX, scaleFactorY);
	} while (0);
	// register callbacks
	ctrl_server_setreplay(sdlmsg_replay_callback);
	//
	return 0;
}

int
sdlmsg_replay_deinit(void *arg) {
	return 0;
}

static void
sdlmsg_replay_native(sdlmsg_t *msg) {
	INPUT in;
	sdlmsg_keyboard_t *msgk = (sdlmsg_keyboard_t*) msg;
	sdlmsg_mouse_t *msgm = (sdlmsg_mouse_t*) msg;

	switch(msg->msgtype) {
	case SDL_EVENT_MSGTYPE_KEYBOARD:
		bzero(&in, sizeof(in));
		in.type = INPUT_KEYBOARD;
		if((in.ki.wVk = SDLKeyToKeySym(msgk->sdlkey)) != INVALID_KEY) {
			if(msgk->is_pressed == 0) {
				in.ki.dwFlags |= KEYEVENTF_KEYUP;
			}
			in.ki.wScan = MapVirtualKey(in.ki.wVk, MAPVK_VK_TO_VSC);
			//ga_error("sdl replayer: vk=%x scan=%x\n", in.ki.wVk, in.ki.wScan);
			SendInput(1, &in, sizeof(in));
		} else {
		////////////////
		ga_error("sdl replayer: undefined key scan=%u(%04x) key=%u(%04x) mod=%u(%04x) pressed=%d\n",
			msgk->scancode, msgk->scancode,
			msgk->sdlkey, msgk->sdlkey, msgk->sdlmod, msgk->sdlmod,
			msgk->is_pressed);
		////////////////
		}
		break;
	case SDL_EVENT_MSGTYPE_MOUSEKEY:
		//ga_error("sdl replayer: button event btn=%u pressed=%d\n", msg->mousebutton, msg->is_pressed);
		bzero(&in, sizeof(in));
		in.type = INPUT_MOUSE;
		if(msgm->mousebutton == 1 && msgm->is_pressed != 0) {
			in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		} else if(msgm->mousebutton == 1 && msgm->is_pressed == 0) {
			in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
		} else if(msgm->mousebutton == 2 && msgm->is_pressed != 0) {
			in.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
		} else if(msgm->mousebutton == 2 && msgm->is_pressed == 0) {
			in.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
		} else if(msgm->mousebutton == 3 && msgm->is_pressed != 0) {
			in.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
		} else if(msgm->mousebutton == 3 && msgm->is_pressed == 0) {
			in.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
		} else if(msgm->mousebutton == 4 && msgm->is_pressed != 0) {
			// mouse wheel forward
			in.mi.dwFlags = MOUSEEVENTF_WHEEL;
			in.mi.mouseData = +WHEEL_DELTA;
		} else if(msgm->mousebutton == 5 && msgm->is_pressed != 0) {
			// mouse wheel backward
			in.mi.dwFlags = MOUSEEVENTF_WHEEL;
			in.mi.mouseData = -WHEEL_DELTA;
		}
		SendInput(1, &in, sizeof(in));
		break;
	case SDL_EVENT_MSGTYPE_MOUSEWHEEL:
		if(msgm->mousex != 0) {
			bzero(&in, sizeof(in));
			in.type = INPUT_MOUSE;
			if(((short) msgm->mousex) > 0) {
				// mouse wheel forward
				in.mi.dwFlags = MOUSEEVENTF_WHEEL;
				in.mi.mouseData = +WHEEL_DELTA;
			} else if(((short) msgm->mousex) < 0 ) {
				// mouse wheel backward
				in.mi.dwFlags = MOUSEEVENTF_WHEEL;
				in.mi.mouseData = -WHEEL_DELTA;
			}
			SendInput(1, &in, sizeof(in));
		}
#if 0
		if(msgm->mousey != 0) {
			bzero(&in, sizeof(in));
			in.type = INPUT_MOUSE;
			if(((short) msgm->mousey) > 0) {
				// mouse wheel forward
				in.mi.dwFlags = MOUSEEVENTF_HWHEEL;
				in.mi.mouseData = +WHEEL_DELTA;
			} else if(((short) msgm->mousey) < 0 ) {
				// mouse wheel backward
				in.mi.dwFlags = MOUSEEVENTF_HWHEEL;
				in.mi.mouseData = -WHEEL_DELTA;
			}
			SendInput(1, &in, sizeof(in));
		}
#endif
		break;
	case SDL_EVENT_MSGTYPE_MOUSEMOTION:
		//ga_error("sdl replayer: motion event x=%u y=%d\n", msgm->mousex, msgm->mousey);
		bzero(&in, sizeof(in));
		in.type = INPUT_MOUSE;
		// mouse x/y has to be mapped to (0,0)-(65535,65535)
		if(msgm->relativeMouseMode == 0) {
			if(prect == NULL) {
				in.mi.dx = (DWORD)
					(65536.0 * scaleFactorX * msgm->mousex) / cxsize;
				in.mi.dy = (DWORD)
					(65536.0 * scaleFactorY * msgm->mousey) / cysize;
			} else {
				in.mi.dx = (DWORD)
					(65536.0 * (prect->left + scaleFactorX * msgm->mousex)) / cxsize;
				in.mi.dy = (DWORD)
					(65536.0 * (prect->top + scaleFactorY * msgm->mousey)) / cysize;
			}
			in.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
		} else {
			in.mi.dx = (short) (scaleFactorX * (short) msgm->mouseRelX);
			in.mi.dy = (short) (scaleFactorY * (short) msgm->mouseRelY);
			in.mi.dwFlags = MOUSEEVENTF_MOVE;
		}
		SendInput(1, &in, sizeof(in));
		break;
	default: // do nothing
		break;
	}
	return;
}


int
sdlmsg_kb_init() {
	kbScancode.clear();
	kbSdlkey.clear();
	//// load scancodes
	// load blocked keys to kbScancode kbSdlkey
	ga_error("key-blocking initialized: %u+%u keys blocked.\n",
		kbScancode.size(), kbSdlkey.size());
	//
	return 0;
}

GEN_KB_ADD_FUNC(unsigned short, scancode, kbScancode)
GEN_KB_ADD_FUNC(int, sdlkey, kbSdlkey)
GEN_KB_MATCH_FUNC(unsigned short, scancode, kbScancode)
GEN_KB_MATCH_FUNC(int, sdlkey, kbSdlkey)

int
sdlmsg_key_blocked(sdlmsg_t *msg) {
	sdlmsg_keyboard_t *msgk;
	if(msg->msgtype != SDL_EVENT_MSGTYPE_KEYBOARD) {
		return 0;
	}
	//
	msgk = (sdlmsg_keyboard_t*) msg;
	//
	if(sdlmsg_kb_match_scancode(msgk->scancode)) {
		return 1;
	}
	if(sdlmsg_kb_match_sdlkey(msgk->sdlkey)) {
		return 1;
	}
	return 0;
}

int
sdlmsg_replay(sdlmsg_t *msg) {
	// convert from network byte order to host byte order
	sdlmsg_ntoh(msg);
	if(sdlmsg_key_blocked(msg)) {
		return 0;
	}
	sdlmsg_replay_native(msg);
	return 0;
}

void
sdlmsg_replay_callback(void *msg, int msglen) {
	sdlmsg_t *m = (sdlmsg_t*) msg;
	if(msglen != ntohs(m->msgsize)/*sizeof(sdlmsg_t)*/) {
		ga_error("message length mismatched. (%d != %d)\n",
			msglen, ntohs(m->msgsize));
	}
	sdlmsg_replay((sdlmsg_t*) msg);
	return;
}

//////////////////////////////////////////////////////////////////////////////

static void
SDLKeyToKeySym_init() {
	unsigned short i;
	//
	keymap[SDLK_BACKSPACE]	= VK_BACK;		//		= 8,
	keymap[SDLK_TAB]	= VK_TAB;		//		= 9,
	keymap[SDLK_CLEAR]	= VK_CLEAR;		//		= 12,
	keymap[SDLK_RETURN]	= VK_RETURN;		//		= 13,
	keymap[SDLK_PAUSE]	= VK_PAUSE;		//		= 19,
	keymap[SDLK_ESCAPE]	= VK_ESCAPE;		//		= 27,
	// Latin 1: starting from space (0x20)
	keymap[SDLK_SPACE]	= VK_SPACE;		//		= 32,
	// (0x20) space, exclam, quotedbl, numbersign, dollar, percent, ampersand,
	// (0x27) quoteright, parentleft, parentright, asterisk, plus, comma,
	// (0x2d) minus, period, slash
	//SDLK_EXCLAIM		= 33,
	keymap[SDLK_QUOTEDBL]	= VK_OEM_7;		//		= 34,
	//SDLK_HASH		= 35,
	//SDLK_DOLLAR		= 36,
	//SDLK_AMPERSAND		= 38,
	keymap[SDLK_QUOTE]	= VK_OEM_7;		//		= 39,
	//SDLK_LEFTPAREN		= 40,
	//SDLK_RIGHTPAREN		= 41,
	//SDLK_ASTERISK		= 42,
	keymap[SDLK_PLUS]	= VK_OEM_PLUS;		//		= 43,
	keymap[SDLK_COMMA]	= VK_OEM_COMMA;		//		= 44,
	keymap[SDLK_MINUS]	= VK_OEM_MINUS;		//		= 45,
	keymap[SDLK_PERIOD]	= VK_OEM_PERIOD;	//		= 46,
	keymap[SDLK_SLASH]	= VK_OEM_2;		//		= 47,
	keymap[SDLK_COLON]	= VK_OEM_1;		//		= 58,
	keymap[SDLK_SEMICOLON]	= VK_OEM_1;		//		= 59,
	keymap[SDLK_LESS]	= VK_OEM_COMMA;		//		= 60,
	keymap[SDLK_EQUALS]	= VK_OEM_PLUS;		//		= 61,
	keymap[SDLK_GREATER]	= VK_OEM_PERIOD;	//		= 62,
	keymap[SDLK_QUESTION]	= VK_OEM_2;		//		= 63,
	//SDLK_AT			= 64,
	/* 
	   Skip uppercase letters
	 */
	keymap[SDLK_LEFTBRACKET]= VK_OEM_4;		//		= 91,
	keymap[SDLK_BACKSLASH]	= VK_OEM_5;		//		= 92,
	keymap[SDLK_RIGHTBRACKET]= VK_OEM_6;		//		= 93,
	//SDLK_CARET		= 94,
	keymap[SDLK_UNDERSCORE]	= VK_OEM_MINUS;		//		= 95,
	keymap[SDLK_BACKQUOTE]	= VK_OEM_3;		//		= 96,
	// (0x30-0x39) 0-9
	for(i = 0x30; i <= 0x39; i++) {
		keymap[i] = i;
	}
	// (0x3a) colon, semicolon, less, equal, greater, question, at
	// (0x41-0x5a) A-Z
	// SDL: no upper cases, only lower cases
	// (0x5b) bracketleft, backslash, bracketright, asciicircum/caret,
	// (0x5f) underscore, grave
	// (0x61-7a) a-z
	for(i = 0x61; i <= 0x7a; i++) {
		keymap[i] = i & 0xdf;	// convert to uppercases
	}
	keymap[SDLK_DELETE]	= VK_DELETE;		//		= 127,
	// SDLK_WORLD_0 (0xa0) - SDLK_WORLD_95 (0xff) are ignored
	/** @name Numeric keypad */
	keymap[SDLK_KP0]	= VK_NUMPAD0;	//		= 256,
	keymap[SDLK_KP1]	= VK_NUMPAD1;	//		= 257,
	keymap[SDLK_KP2]	= VK_NUMPAD2;	//		= 258,
	keymap[SDLK_KP3]	= VK_NUMPAD3;	//		= 259,
	keymap[SDLK_KP4]	= VK_NUMPAD4;	//		= 260,
	keymap[SDLK_KP5]	= VK_NUMPAD5;	//		= 261,
	keymap[SDLK_KP6]	= VK_NUMPAD6;	//		= 262,
	keymap[SDLK_KP7]	= VK_NUMPAD7;	//		= 263,
	keymap[SDLK_KP8]	= VK_NUMPAD8;	//		= 264,
	keymap[SDLK_KP9]	= VK_NUMPAD9;	//		= 265,
	keymap[SDLK_KP_PERIOD]	= VK_DECIMAL;	//		= 266,
	keymap[SDLK_KP_DIVIDE]	= VK_DIVIDE;	//		= 267,
	keymap[SDLK_KP_MULTIPLY]= VK_MULTIPLY;	//		= 268,
	keymap[SDLK_KP_MINUS]	= VK_SUBTRACT;	//		= 269,
	keymap[SDLK_KP_PLUS]	= VK_ADD;	//		= 270,
	//keymap[SDLK_KP_ENTER]	= XK_KP_Enter;	//		= 271,
	//keymap[SDLK_KP_EQUALS]	= XK_KP_Equal;	//		= 272,
	/** @name Arrows + Home/End pad */
	keymap[SDLK_UP]		= VK_UP;	//		= 273,
	keymap[SDLK_DOWN]	= VK_DOWN;	//		= 274,
	keymap[SDLK_RIGHT]	= VK_RIGHT;	//		= 275,
	keymap[SDLK_LEFT]	= VK_LEFT;	//		= 276,
	keymap[SDLK_INSERT]	= VK_INSERT;	//		= 277,
	keymap[SDLK_HOME]	= VK_HOME;	//		= 278,
	keymap[SDLK_END]	= VK_END;	//		= 279,
	keymap[SDLK_PAGEUP]	= VK_PRIOR;	//		= 280,
	keymap[SDLK_PAGEDOWN]	= VK_NEXT;	//		= 281,
	/** @name Function keys */
	keymap[SDLK_F1]		= VK_F1;	//		= 282,
	keymap[SDLK_F2]		= VK_F2;	//		= 283,
	keymap[SDLK_F3]		= VK_F3;	//		= 284,
	keymap[SDLK_F4]		= VK_F4;	//		= 285,
	keymap[SDLK_F5]		= VK_F5;	//		= 286,
	keymap[SDLK_F6]		= VK_F6;	//		= 287,
	keymap[SDLK_F7]		= VK_F7;	//		= 288,
	keymap[SDLK_F8]		= VK_F8;	//		= 289,
	keymap[SDLK_F9]		= VK_F9;	//		= 290,
	keymap[SDLK_F10]	= VK_F10;	//		= 291,
	keymap[SDLK_F11]	= VK_F11;	//		= 292,
	keymap[SDLK_F12]	= VK_F12;	//		= 293,
	keymap[SDLK_F13]	= VK_F13;	//		= 294,
	keymap[SDLK_F14]	= VK_F14;	//		= 295,
	keymap[SDLK_F15]	= VK_F15;	//		= 296,
	/** @name Key state modifier keys */
	keymap[SDLK_NUMLOCK]	= VK_NUMLOCK;	//		= 300,
	keymap[SDLK_CAPSLOCK]	= VK_CAPITAL;	//		= 301,
	keymap[SDLK_SCROLLOCK]	= VK_SCROLL;	//		= 302,
	keymap[SDLK_RSHIFT]	= VK_RSHIFT;	//		= 303,
	keymap[SDLK_LSHIFT]	= VK_LSHIFT;	//		= 304,
	keymap[SDLK_RCTRL]	= VK_RCONTROL;	//		= 305,
	keymap[SDLK_LCTRL]	= VK_LCONTROL;	//		= 306,
	keymap[SDLK_RALT]	= VK_RMENU;	//		= 307,
	keymap[SDLK_LALT]	= VK_LMENU;	//		= 308,
	keymap[SDLK_RMETA]	= VK_RWIN;	//		= 309,
	keymap[SDLK_LMETA]	= VK_LWIN;	//		= 310,
	//keymap[SDLK_LSUPER]	= XK_Super_L;	//		= 311,		/**< Left "Windows" key */
	//keymap[SDLK_RSUPER]	= XK_Super_R;	//		= 312,		/**< Right "Windows" key */
	keymap[SDLK_MODE]	= VK_MODECHANGE;//		= 313,		/**< "Alt Gr" key */
	//keymap[SDLK_COMPOSE]	= XK_Multi_key;	//		= 314,		/**< Multi-key compose key */
	/** @name Miscellaneous function keys */
	keymap[SDLK_HELP]	= VK_HELP;	//		= 315,
	//keymap[SDLK_SYSREQ]	= XK_Sys_Req;	//		= 317,
	keymap[SDLK_BREAK]	= VK_CANCEL;	//		= 318,
	keymap[SDLK_MENU]	= VK_MENU;	//		= 319,
	//keymap[SDLK_UNDO]	= XK_Undo;	//		= 322,		/**< Atari keyboard has Undo */
	//
	keymap_initialized = true;
	return;
}

static KeySym
SDLKeyToKeySym(int sdlkey) {
	map<int, KeySym>::iterator mi;
	if(keymap_initialized == false) {
		SDLKeyToKeySym_init();
	}
	if((mi = keymap.find(sdlkey)) != keymap.end()) {
		return mi->second;
	}
	return INVALID_KEY;
}

