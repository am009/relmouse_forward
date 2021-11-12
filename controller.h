/*
 * Copyright (c) 2012-2015 Chun-Ying Huang
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

#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

// #include "rtspconf.h"
#include "ctrl-msg.h"

#define	CTRL_MAX_ID_LENGTH	64
#define	CTRL_CURRENT_VERSION	"GACtrlV01"
#define	CTRL_QUEUE_SIZE		65536	// 64K

typedef void (*msgfunc)(void *, int);

// handshake message: 
struct ctrlhandshake {
	unsigned char length;
	char id[CTRL_MAX_ID_LENGTH];
};

struct queuemsg {
	unsigned short msgsize;		// a general header for messages
	unsigned char msg[2];		// use '2' to prevent Windows from complaining
};

EXPORT	int			ctrl_queue_init(int size, int maxunit);
EXPORT	void			ctrl_queue_free();
EXPORT	struct queuemsg *	ctrl_queue_read_msg();
EXPORT	int			ctrl_queue_write_msg(void *msg, int msgsize);
EXPORT	void			ctrl_queue_clear();

EXPORT	SOCKET	ctrl_socket_init(bool isServer);

EXPORT	int	    ctrl_client_init(const char *ctrlid);
EXPORT	void*	ctrl_client_thread(void*);
EXPORT	void	ctrl_client_sendmsg(void *msg, int msglen);

EXPORT	int		ctrl_server_init(const char *ctrlid);
EXPORT	msgfunc ctrl_server_setreplay(msgfunc);
EXPORT	void*	ctrl_server_thread(void*);

EXPORT	void	ctrl_server_set_output_resolution(int width, int height);
EXPORT	void	ctrl_server_set_resolution(int width, int height);
EXPORT	void	ctrl_server_get_resolution(int *width, int *height);
EXPORT	void	ctrl_server_get_scalefactor(double *fx, double *fy);

#endif
