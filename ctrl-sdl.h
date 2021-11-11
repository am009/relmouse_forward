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

#ifndef __CTRL_SDL_H__
#define __CTRL_SDL_H__

#include <SDL_version.h>
#include <SDL_keycode.h>

#define	SDL_EVENT_MSGTYPE_NULL		0
#define	SDL_EVENT_MSGTYPE_KEYBOARD	1
#define	SDL_EVENT_MSGTYPE_MOUSEKEY	2
#define SDL_EVENT_MSGTYPE_MOUSEMOTION	3
#define SDL_EVENT_MSGTYPE_MOUSEWHEEL	4

#pragma pack(push, 1)
struct sdlmsg_s {
	unsigned short msgsize;		// size of this data-structure
					// every message MUST start from a
					// unsigned short message size
					// the size includes the 'msgsize'
	unsigned char msgtype;
	unsigned char which;
	unsigned char padding[60];	// must be large enough to fit
					// all supported type of messages
}
#pragma pack(pop)
;
typedef struct sdlmsg_s			sdlmsg_t;

// keyboard event
#pragma pack(push, 1)
struct sdlmsg_keyboard_s {
	unsigned short msgsize;
	unsigned char msgtype;		// SDL_EVENT_MSGTYPE_KEYBOARD
	unsigned char which;
	unsigned char is_pressed;
	unsigned char unused0;
	unsigned short scancode;	// scancode
	int sdlkey;			// SDLKey
	unsigned int unicode;		// unicode or ASCII value
	unsigned short sdlmod;		// SDLMod
}
#pragma pack(pop)
;
typedef struct sdlmsg_keyboard_s	sdlmsg_keyboard_t;

// mouse event
#pragma pack(push, 1)
struct sdlmsg_mouse_s {
	unsigned short msgsize;
	unsigned char msgtype;		// SDL_EVENT_MSGTYPE_MOUSEKEY
					// SDL_EVENT_MSGTYPE_MOUSEMOTION
					// SDL_EVENT_MSGTYPE_MOUSEWHEEL
	unsigned char which;
	unsigned char is_pressed;	// for mouse button
	unsigned char mousebutton;	// mouse button
	unsigned char mousestate;	// mouse stat
	unsigned char relativeMouseMode;
	unsigned short mousex;
	unsigned short mousey;
	unsigned short mouseRelX;
	unsigned short mouseRelY;
}
#pragma pack(pop)
;
typedef struct sdlmsg_mouse_s		sdlmsg_mouse_t;

sdlmsg_t* sdlmsg_ntoh(sdlmsg_t *msg);

///// key blocking support
#define	GEN_KB_ADD_FUNC_PROTO(type, field) \
	int sdlmsg_kb_add_##field(type v, int remove)
#define GEN_KB_ADD_FUNC(type, field, db)	\
	GEN_KB_ADD_FUNC_PROTO(type, field) { \
		if(remove) { db.erase(v); } \
		else       { db[v] = v;   } \
		return 0; \
	}
#define GEN_KB_MATCH_FUNC_PROTO(type, field) \
	int sdlmsg_kb_match_##field(type v)
#define	GEN_KB_MATCH_FUNC(type, field, db) \
	GEN_KB_MATCH_FUNC_PROTO(type, field) { \
		if(db.find(v) == db.end()) { return 0; } \
		return 1; \
	}
int sdlmsg_kb_init();
GEN_KB_ADD_FUNC_PROTO(unsigned short, scancode);
GEN_KB_ADD_FUNC_PROTO(int, sdlkey);
GEN_KB_MATCH_FUNC_PROTO(unsigned short, scancode);
GEN_KB_MATCH_FUNC_PROTO(int, sdlkey);
int sdlmsg_key_blocked(sdlmsg_t *msg);
////

sdlmsg_t* sdlmsg_keyboard(sdlmsg_t *msg, unsigned char pressed, unsigned short scancode, SDL_Keycode key, unsigned short mod, unsigned int unicode);
sdlmsg_t* sdlmsg_mousewheel(sdlmsg_t *msg, unsigned short mousex, unsigned short mousey);
sdlmsg_t* sdlmsg_mousekey(sdlmsg_t *msg, unsigned char pressed, unsigned char button, unsigned short x, unsigned short y);
sdlmsg_t* sdlmsg_mousemotion(sdlmsg_t *msg, unsigned short mousex, unsigned short mousey, unsigned short relx, unsigned short rely, unsigned char state, int relativeMouseMode);

int sdlmsg_replay_init(void *arg);
int sdlmsg_replay_deinit(void *arg);
int sdlmsg_replay(sdlmsg_t *msg);
void sdlmsg_replay_callback(void *msg, int msglen);

#endif /* __CTRL_SDL_H__ */
