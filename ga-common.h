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

/**
 * @file
 * header: common GA functions and macros
 */

#ifndef __GA_COMMON_H__
#define __GA_COMMON_H__

//#if defined WIN32 && defined GA_LIB
///** Functions exported from DLL's */
//#define	EXPORT __declspec(dllexport)
//#elif defined WIN32 && ! defined GA_LIB
///** Functions imported from DLL's */
//#define	EXPORT __declspec(dllimport)
//#else
///** Not used in UNIX-like systems, but required for compatible with WIN32 libraries */
//#define	EXPORT
//#endif
#define	EXPORT

//#ifdef __cplusplus
//extern "C" {
//#endif
//#include <libavcodec/avcodec.h>
//#ifdef __cplusplus
//}
//#endif

#include "ga-win32.h"
#include <string>

/** Enable audio subsystem? */
#define	ENABLE_AUDIO

/** Unit size size for RGBA pixels, in bytes */
#define	RGBA_SIZE	4

struct gaRect {
	int left, top;
	int right, bottom;
	int width, height;
	int linesize;
	int size;
};

struct gaImage {
	int width;
	int height;
	int bytes_per_line;
};

//EXPORT long long tvdiff_us(struct timeval *tv1, struct timeval *tv2);
//EXPORT long long ga_usleep(long long interval, struct timeval *ptv);
//EXPORT int	ga_log(const char *fmt, ...);
EXPORT int	ga_error(const char *fmt, ...);
//EXPORT int	ga_malloc(int size, void **ptr, int *alignment);
//EXPORT int	ga_alignment(void *ptr, int alignto);
EXPORT long	ga_gettid();
int winsock_init();
char* getCmdOption(char** begin, char** end, const std::string& option);
bool cmdOptionExists(char** begin, char** end, const std::string& option);

#endif
