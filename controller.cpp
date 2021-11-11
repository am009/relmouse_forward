/*
 * Copyright (c) 2013 Chun-Ying Huang
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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "controller.h"
#include "config.h"
#include "ctrl-sdl.h"

using namespace std;

static char *myctrlid = NULL;
static bool ctrlenabled = true;
//
static pthread_mutex_t wakeup_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wakeup = PTHREAD_COND_INITIALIZER;
static SOCKET ctrlsocket = -1;
static struct sockaddr_in ctrlsin;
// message queue
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static int qhead, qtail, qsize, qunit;
static unsigned char *qbuffer = NULL;

static msgfunc replay = NULL;

static unsigned long
name_resolve(const char *hostname) {
	struct in_addr addr;
	struct hostent *hostEnt;
	if((addr.s_addr = inet_addr(hostname)) == INADDR_NONE) {
		if((hostEnt = gethostbyname(hostname)) == NULL)
			return INADDR_NONE;
		bcopy(hostEnt->h_addr, (char *) &addr.s_addr, hostEnt->h_length);
	}
	return addr.s_addr;
}

// queue routines
int
ctrl_queue_init(int size, int maxunit) {
	qunit = maxunit + sizeof(struct queuemsg);
	qsize = size - (size % qunit);
	qhead = qtail = 0;
	if((qbuffer = (unsigned char*) malloc(qsize)) == NULL) {
		return -1;
	}
	ga_error("controller queue: initialized size=%d (%d units)\n",
		qsize, qsize/qunit);
	return 0;
}

void
ctrl_queue_free() {
	pthread_mutex_lock(&queue_mutex);
	if(qbuffer != NULL)
		free(qbuffer);
	qbuffer = NULL;
	qhead = qtail = qsize = 0;
	pthread_mutex_unlock(&queue_mutex);
}

struct queuemsg *
ctrl_queue_read_msg() {
	struct queuemsg *msg;
	//
	pthread_mutex_lock(&queue_mutex);
	if(qbuffer == NULL) {
		pthread_mutex_unlock(&queue_mutex);
		ga_error("controller queue: buffer released.\n");
		return NULL;
	}
	if(qtail == qhead) {
		// queue is empty
		msg = NULL;
	} else {
		msg = (struct queuemsg *) (qbuffer + qhead);
	}
	pthread_mutex_unlock(&queue_mutex);
	//
	return msg;
}

void
ctrl_queue_release_msg(struct queuemsg *msg) {
	struct queuemsg *currmsg;
	pthread_mutex_lock(&queue_mutex);
	if(qbuffer == NULL) {
		pthread_mutex_unlock(&queue_mutex);
		ga_error("controller queue: buffer released.\n");
		return;
	}
	if(qhead == qtail) {
		// queue is empty
		pthread_mutex_unlock(&queue_mutex);
		return;
	}
	currmsg = (struct queuemsg *) (qbuffer + qhead);
	if(msg != currmsg) {
		ga_error("controller queue: WARNING - release an incorrect msg?\n");
	}
	qhead += qunit;
	if(qhead == qsize) {
		qhead = 0;
	}
	pthread_mutex_unlock(&queue_mutex);
	return;
}

int
ctrl_queue_write_msg(void *msg, int msgsize) {
	int nextpos;
	struct queuemsg *qmsg;
	//
	if((msgsize + sizeof(struct queuemsg)) > qunit) {
		ga_error("controller queue: msg size exceeded (%d > %d).\n",
			msgsize + sizeof(struct queuemsg), qunit);
		return 0;
	}
	pthread_mutex_lock(&queue_mutex);
	if(qbuffer == NULL) {
		pthread_mutex_unlock(&queue_mutex);
		ga_error("controller queue: buffer released.\n");
		return 0;
	}
	//
	nextpos = qtail + qunit;
	if(nextpos == qsize) {
		nextpos = 0;
	}
	//
	if(nextpos == qhead) {
		// queue is full
		msgsize = 0;
	} else {
		qmsg = (struct queuemsg*) (qbuffer + qtail);
		qmsg->msgsize = msgsize;
		if(msgsize > 0)
			bcopy(msg, qmsg->msg, msgsize);
		qtail = nextpos;
	}
	pthread_mutex_unlock(&queue_mutex);
	//
	return msgsize;
}

void
ctrl_queue_clear() {
	pthread_mutex_lock(&queue_mutex);
	qhead = qtail = 0;
	pthread_mutex_unlock(&queue_mutex);
}

////////////////////////////////////////////////////////////////////

SOCKET
ctrl_socket_init() {
	//
	if(config_ctrlproto == IPPROTO_TCP) {
		ctrlsocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	} else if(config_ctrlproto == IPPROTO_UDP) {
		ctrlsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	} else {
		ga_error("Controller socket-init: not supported protocol.\n");
		return -1;
	}
	if(ctrlsocket < 0) {
		ga_error("Controller socket-init: %s\n", strerror(errno));
	}
	//
	bzero(&ctrlsin, sizeof(struct sockaddr_in));
	ctrlsin.sin_family = AF_INET;
	ctrlsin.sin_port = htons(config_ctrlport);
	if(config_server_name != NULL) {
		ctrlsin.sin_addr.s_addr = name_resolve(config_server_name);
		if(ctrlsin.sin_addr.s_addr == INADDR_NONE) {
			ga_error("Name resolution failed: %s\n", config_server_name);
			return -1;
		}
	}
	ga_error("controller socket: socket address [%s:%u]\n",
		inet_ntoa(ctrlsin.sin_addr), ntohs(ctrlsin.sin_port));

	return ctrlsocket;
}

////////////////////////////////////////////////////////////////////

int
ctrl_client_init(const char *ctrlid) {
	if(ctrl_socket_init() < 0) {
		return -1;
	}
	if(config_ctrlproto == IPPROTO_TCP) {
		struct ctrlhandshake hh;
		// connect to the server
		if(connect(ctrlsocket, (struct sockaddr*) &ctrlsin, sizeof(ctrlsin)) < 0) {
			ga_error("controller client-connect: %s\n", strerror(errno));
			goto error;
		}
		// send handshake
		hh.length = 1+strlen(ctrlid)+1;	// msg total len, id, null-terminated
		if(hh.length > sizeof(hh))
			hh.length = sizeof(hh);
		strncpy(hh.id, ctrlid, sizeof(hh.id));
		if(send(ctrlsocket, (char*) &hh, hh.length, 0) <= 0) {
			ga_error("controller client-send(handshake): %s\n", strerror(errno));
			goto error;
		}
	}
	return 0;
error:
	ctrlenabled = false;
	ga_error("controller client: controller disabled.\n");
	close(ctrlsocket);
	ctrlsocket = -1;
	return -1;
}

void*
ctrl_client_thread(void*) {

	if(ctrl_client_init(CTRL_CURRENT_VERSION) < 0) {
		ga_error("controller client-thread: init failed, thread terminated.\n");
		return NULL;
	}

	ga_error("controller client-thread started: tid=%ld.\n", ga_gettid());

	while(true) {
		struct queuemsg *qm;
		pthread_mutex_lock(&wakeup_mutex);
		pthread_cond_wait(&wakeup, &wakeup_mutex);
		pthread_mutex_unlock(&wakeup_mutex);
		while((qm = ctrl_queue_read_msg()) != NULL) {
			int wlen;
			if(qm->msgsize == 0) {
				ga_error("controller client: null messgae received, terminate the thread.\n");
				goto quit;
			}
			if(config_ctrlproto == IPPROTO_TCP) {
				if((wlen = send(ctrlsocket, (char*) qm->msg, qm->msgsize, 0)) < 0) {
					ga_error("controller client-send(tcp): %s\n", strerror(errno));
					exit(-1);
				}
			} else if(config_ctrlproto == IPPROTO_UDP) {
				if (config_debug) {
					sdlmsg_t* msg = ((sdlmsg_t*)(qm->msg));
					if (msg->msgtype == SDL_EVENT_MSGTYPE_MOUSEMOTION) {
						sdlmsg_mouse_t* mmsg = (sdlmsg_mouse_t*)msg;
						//printf("Mouse Motion %s: X:%d Y:%d relX:%hd relY:%hd \n", mmsg->relativeMouseMode ? "Rel" : "Abs", ntohs(mmsg->mousex), ntohs(mmsg->mousey), ntohs(mmsg->mouseRelX), ntohs(mmsg->mouseRelY));
					}
					else {
						printf("%d\n", msg->msgtype);
					}
				}
				if((wlen = sendto(ctrlsocket, (char*) qm->msg, qm->msgsize, 0, (struct sockaddr*) &ctrlsin, sizeof(ctrlsin))) < 0) {
					ga_error("controller client-send(udp): %s\n", strerror(errno));
					exit(-1);
				}
			}
			ctrl_queue_release_msg(qm);
			//ga_error("controller client-debug: send msg (%d bytes)\n", wlen);
		}
	}

quit:
	close(ctrlsocket);
	ctrlsocket = -1;
	ga_error("controller client-thread terminated: tid=%ld.\n", ga_gettid());

	return NULL;
}

void
ctrl_client_sendmsg(void *msg, int msglen) {
	if(ctrlenabled == false) {
		ga_error("controller client-sendmsg: controller was disabled.\n");
		return;
	}
	if(ctrl_queue_write_msg(msg, msglen) != msglen) {
		ga_error("controller client-sendmsg: queue full, message dropped.\n");
	} else {
		pthread_cond_signal(&wakeup);
	}
	return;
}

////////////////////////////////////////////////////////////////////

int
ctrl_server_init(const char *ctrlid) {
	if(ctrl_socket_init() < 0)
		return -1;
	myctrlid = strdup(ctrlid);
	// reuse port
	do {
		int val = 1;
		if(setsockopt(ctrlsocket, SOL_SOCKET, SO_REUSEADDR, (char*) &val, sizeof(val)) < 0) {
			ga_error("controller server-bind reuse port: %d\n", WSAGetLastError());
			goto error;
		}
	} while(0);
	// bind for either TCP of UDP
	if(bind(ctrlsocket, (struct sockaddr*) &ctrlsin, sizeof(ctrlsin)) < 0) {
		ga_error("controller server-bind: %s\n", strerror(errno));
		goto error;
	}
	// TCP listen
	if(config_ctrlproto == IPPROTO_TCP) {
		if(listen(ctrlsocket, 16) < 0) {
			ga_error("controller server-listen: %s\n", strerror(errno));
			goto error;
		}
	}
	return 0;
error:
	close(ctrlsocket);
	ctrlsocket = -1;
	return -1;
}

msgfunc
ctrl_server_setreplay(msgfunc callback) {
	msgfunc old = replay;
	replay = callback;
	return old;
}

void*
ctrl_server_thread(void* ) {
	struct sockaddr_in csin, xsin;
	SOCKET socket;
	int csinlen, xsinlen;
	int clientaccepted = 0;
	//
	unsigned char buf[8192];
	int bufhead, buflen;

	if(ctrl_server_init(CTRL_CURRENT_VERSION) < 0) {
		ga_error("controller server-thread: init failed, terminated.\n");
		exit(-1);
	}

	ga_error("controller server started: tid=%ld.\n", ga_gettid());

restart:
	bzero(&csin, sizeof(csin));
	csinlen = sizeof(csin);
	csin.sin_family = AF_INET;
	clientaccepted = 0;
	// handle only one client
	if(config_ctrlproto == IPPROTO_TCP) {
		struct ctrlhandshake *hh = (struct ctrlhandshake*) buf;
		//
		if((socket = accept(ctrlsocket, (struct sockaddr*) &csin, &csinlen)) < 0) {
			ga_error("controller server-accept: %s.\n", strerror(errno));
			goto restart;
		}
		ga_error("controller server-thread: accepted TCP client from %s.%d\n",
			inet_ntoa(csin.sin_addr), ntohs(csin.sin_port));
		// check initial handshake message
		if((buflen = recv(socket, (char*) buf, sizeof(buf), 0)) <= 0) {
			ga_error("controller server-thread: %s\n", strerror(errno));
			close(socket);
			goto restart;
		}
		if(hh->length > buflen) {
			ga_error("controller server-thread: bad handshake length (%d > %d)\n",
				hh->length, buflen);
			close(socket);
			goto restart;
		}
		if(memcmp(myctrlid, hh->id, hh->length-1) != 0) {
			ga_error("controller server-thread: mismatched protocol version (%s != %s), length = %d\n",
				hh->id, myctrlid, hh->length-1);
			close(socket);
			goto restart;
		}
		ga_error("controller server-thread: receiving events ...\n");
		clientaccepted = 1;
	}

	buflen = 0;

	while(true) {
		int rlen, msglen;
		//
		bufhead = 0;
		//
		if(config_ctrlproto == IPPROTO_TCP) {
tcp_readmore:
			if((rlen = recv(socket, (char*) buf+buflen, sizeof(buf)-buflen, 0)) <= 0) {
				ga_error("controller server-read: %s\n", strerror(errno));
				ga_error("controller server-thread: conenction closed.\n");
				close(socket);
				goto restart;
			}
			buflen += rlen;
		} else if(config_ctrlproto == IPPROTO_UDP) {
			bzero(&xsin, sizeof(xsin));
			xsinlen = sizeof(xsin);
			xsin.sin_family = AF_INET;
			buflen = recvfrom(ctrlsocket, (char*) buf, sizeof(buf), 0, (struct sockaddr*) &xsin, &xsinlen);
			if(clientaccepted == 0) {
				bcopy(&xsin, &csin, sizeof(csin));
				clientaccepted = 1;
			} else if(memcmp(&csin, &xsin, sizeof(csin)) != 0) {
				ga_error("controller server-thread: NOTICE - UDP client reconnected?\n");
				bcopy(&xsin, &csin, sizeof(csin));
				//continue;
			}
		}
tcp_again:
		if(buflen < 2) {
			if(config_ctrlproto == IPPROTO_TCP)
				goto tcp_readmore;
			else
				continue;
		}
		//
		msglen = ntohs(*((unsigned short*) (buf + bufhead)));
		//
		if(msglen == 0) {
			ga_error("controller server: WARNING - invalid message with size equal to zero!\n");
			continue;
		}
		// buffer checks for TCP - not sufficient?
		if(config_ctrlproto == IPPROTO_TCP) {
			if(buflen < msglen) {
				bcopy(buf+bufhead, buf, buflen);
				goto tcp_readmore;
			}
		} else if(config_ctrlproto == IPPROTO_UDP) {
			if(buflen != msglen) {
				ga_error("controller server: UDP msg size matched (expected %d, got %d).\n",
					msglen, buflen);
				continue;
			}
		}
		if (config_debug) {
			printf("m");
		}
		// handle message
		if(ctrlsys_handle_message(buf+bufhead, msglen) != 0) {
			// message has been handeled, do nothing
		} else if(replay != NULL) {
			replay(buf+bufhead, msglen);
		} else if(ctrl_queue_write_msg(buf+bufhead, msglen) != msglen) {
			ga_error("controller server: queue full, message dropped.\n");
		} else {
			pthread_cond_signal(&wakeup);
		}
		// handle buffers for TCP
		if(config_ctrlproto == IPPROTO_TCP && buflen > msglen) {
			bufhead += msglen;
			buflen -= msglen;
			goto tcp_again;
		}
		buflen = 0;
	}

	clientaccepted = 0;

	goto restart;

	return NULL;
}

int
ctrl_server_readnext(void *msg, int msglen) {
	int ret;
	struct queuemsg *qm;
again:
	if((qm = ctrl_queue_read_msg()) != NULL) {
		if(qm->msgsize > msglen) {
			ret = -1;
		} else {
			bcopy(qm->msg, msg, qm->msgsize);
			ret = qm->msgsize;
		}
		ctrl_queue_release_msg(qm);
		return ret;
	}
	// nothing available, wait for next input
	pthread_mutex_lock(&wakeup_mutex);
	pthread_cond_wait(&wakeup, &wakeup_mutex);
	pthread_mutex_unlock(&wakeup_mutex);
	goto again;
	// never return from here
	return 0;
}

static pthread_rwlock_t reslock = PTHREAD_RWLOCK_INITIALIZER;
static int curr_width = -1;
static int curr_height = -1;
static pthread_rwlock_t oreslock = PTHREAD_RWLOCK_INITIALIZER;
static int output_width = -1;
static int output_height = -1;

void
ctrl_server_set_output_resolution(int width, int height) {
	pthread_rwlock_wrlock(&oreslock);
	output_width = width;
	output_height = height;
	pthread_rwlock_unlock(&oreslock);
	return;
}

void
ctrl_server_set_resolution(int width, int height) {
	pthread_rwlock_wrlock(&reslock);
	curr_width = width;
	curr_height = height;
	pthread_rwlock_unlock(&reslock);
	return;
}

void
ctrl_server_get_resolution(int *width, int *height) {
	pthread_rwlock_rdlock(&reslock);
	*width = curr_width;
	*height = curr_height;
	pthread_rwlock_unlock(&reslock);
	return;
}

void
ctrl_server_get_scalefactor(double *fx, double *fy) {
	double rx, ry;
	pthread_rwlock_rdlock(&reslock);
	pthread_rwlock_rdlock(&oreslock);
	rx = 1.0 * curr_width / output_width;
	ry = 1.0 * curr_height / output_height;
	pthread_rwlock_unlock(&oreslock);
	pthread_rwlock_unlock(&reslock);
	if(rx <= 0.0)	rx = 1.0;
	if(ry <= 0.0)	ry = 1.0;
	*fx = rx;
	*fy = ry;
	return;
}

