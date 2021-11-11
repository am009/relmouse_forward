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
 * implementation: common GA functions and macros
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#ifndef WIN32
#ifndef ANDROID
#include <execinfo.h>
#endif /* !ANDROID */
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/syscall.h>
#endif /* !WIN32 */
#ifdef ANDROID
#include <android/log.h>
#endif /* ANDROID */
#ifdef __APPLE__
#include <syslog.h>
#endif

#if !defined(WIN32) && !defined(__APPLE__) && !defined(ANDROID)
#include <X11/Xlib.h>
#endif

#include "ga-common.h"
#include "config.h"
// #include "ga-conf.h"
#ifndef ANDROID_NO_FFMPEG
// #include "ga-avcodec.h"
#endif
// #include "rtspconf.h"

#include <map>
#include <list>
using namespace std;

#ifndef NIPQUAD
/** For printing IPv4 addresses: convert an unsigned int to 4 unsigned char. */
#define NIPQUAD(x)	((unsigned char*)&(x))[0],	\
			((unsigned char*)&(x))[1],	\
			((unsigned char*)&(x))[2],	\
			((unsigned char*)&(x))[3]
#endif

/** The gloabl log file name */
static char *ga_logfile = NULL;

/**
 * Compute the time difference for two \a timeval data structure, i.e.,
 * \a tv1 - \a tv2.
 *
 * @param tv1 [in] Pointer to the first \a timeval data structure.
 * @param tv2 [in] Pointer to the second \a timeval data structure.
 * @return The difference in micro seconds.
 */
EXPORT
long long
tvdiff_us(struct timeval *tv1, struct timeval *tv2) {
	struct timeval delta;
	delta.tv_sec = tv1->tv_sec - tv2->tv_sec;
	delta.tv_usec = tv1->tv_usec - tv2->tv_usec;
	if(delta.tv_usec < 0) {
		delta.tv_sec--;
		delta.tv_usec += 1000000;
	}
	return 1000000LL*delta.tv_sec + delta.tv_usec;
}

/**
 * Sleep and wake up at \a ptv + \a interval (micro seconds).
 *
 * @param interval [in] The expected sleeping time (in micro seconds).
 * @param ptv [in] Pointer to the baseline time.
 * @return Currently always return 0.
 *
 * This function is useful for controlling precise sleeps.
 * We usually have to process each video frame in a fixed interval.
 * Each time interval includes the processing time and the sleeping time.
 * However, the processing time could be different in each iteration, so
 * the sleeping time has to be adapted as well.
 * To achieve the goal, we have to obtain the baseline time \a ptv
 * (using \a gettimeofday function)
 * \em before the processing task and call this function \em after
 * the processing task. In this case, the \a interval is set to the total
 * length of the interval, e.g., 41667 for 24fps video.
 *
 * This function sleeps for \a interval micro seconds if the baseline
 * time is not specified.
 */
EXPORT
long long
ga_usleep(long long interval, struct timeval *ptv) {
	long long delta;
	struct timeval tv;
	if(ptv != NULL) {
		gettimeofday(&tv, NULL);
		delta = tvdiff_us(&tv, ptv);
		if(delta >= interval) {
			usleep(1);
			return -1;
		}
		interval -= delta;
	}
	usleep(interval);
	return 0LL;
}

/**
 * Write message \a s into the log file.
 * This in an internal function only called by \em ga_log function.
 *
 * @param tv [in] The timestamp of logging.
 * @param s [in] The message to be written.
 *
 * This function is SLOW, but it attempts to make all writes successfull.
 * It appends the message into the file each time when it is called.
 */
static void
ga_writelog(struct timeval tv, const char *s) {
	FILE *fp;
	if(ga_logfile == NULL)
		return;
	if((fp = fopen(ga_logfile, "at")) != NULL) {
		fprintf(fp, "[%d] %ld.%06ld %s", getpid(), tv.tv_sec, tv.tv_usec, s);
		fclose(fp);
	}
	return;
}

/**
 * Write log messages and print on Android console.
 *
 * @param fmt [in] The format string for the message.
 * @param ... [in] The arguments for replacing specifiers in the format string.
 *
 * This function has the same syntax as the \em printf function.
 * It outputs a timestamp before the message, and optionally writing
 * the message into a log file if log feature is turned on.
 */
EXPORT
int
ga_log(const char *fmt, ...) {
	char msg[4096];
	struct timeval tv;
	va_list ap;
	//
	gettimeofday(&tv, NULL);
	va_start(ap, fmt);
#ifdef ANDROID
	__android_log_vprint(ANDROID_LOG_INFO, "ga_log.native", fmt, ap);
#endif
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
#ifdef __APPLE__
	syslog(LOG_NOTICE, "%s", msg);
#endif
	//
	ga_writelog(tv, msg);
	//
	return 0;
}

/**
 * Print out log messages (on \em stderr or log console (on Android)).
 *
 * @param fmt [in] The format string for the message.
 * @param ... [in] The arguments for replacing specifiers in the format string.
 *
 * This function has the same syntax as the \em printf function.
 * It outputs a timestamp before the message.
 */
EXPORT
int
ga_error(const char *fmt, ...) {
	char msg[4096];
	struct timeval tv;
	va_list ap;
	gettimeofday(&tv, NULL);
	va_start(ap, fmt);
#ifdef ANDROID
	__android_log_vprint(ANDROID_LOG_INFO, "ga_log.native", fmt, ap);
#endif
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
#ifdef __APPLE__
	syslog(LOG_NOTICE, "%s", msg);
#endif
	fprintf(stderr, "# [%d] %ld.%06ld %s", getpid(), tv.tv_sec, tv.tv_usec, msg);
	//
	ga_writelog(tv, msg);
	//
	return -1;
}

/**
 * Get the thread ID in long format.
 */
EXPORT
long
ga_gettid() {
#ifdef WIN32
	return GetCurrentThreadId();
#elif defined __APPLE__
	return pthread_mach_thread_np(pthread_self());
#elif defined ANDROID
	return gettid();
#else
	return (pid_t) syscall(SYS_gettid);
#endif
}

/**
 * Initialize windows socket sub-system.
 *
 * @return 0 on success and -1 on error.
 */
int
winsock_init() {
#ifdef WIN32
	WSADATA wd;
	if(WSAStartup(MAKEWORD(2,2), &wd) != 0)
		return -1;
#endif
	return 0;
}

