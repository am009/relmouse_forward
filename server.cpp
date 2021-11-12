// server.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <pthread.h>

#include "ga-common.h"
#include "ctrl-sdl.h"
#include "controller.h"

int config_ctrlproto = IPPROTO_UDP;
const char* config_ctrlport = "8555";
const char* config_server_name = NULL;
const char* config_log_file = NULL;
bool config_debug =
#ifdef _DEBUG
true;
#else
false;
#endif


int main(int argc, char* argv[])
{
	if (cmdOptionExists(argv, argv + argc, "-h") || cmdOptionExists(argv, argv + argc, "--help"))
	{
		puts("github.com/am009/relmouse-forward server");
		puts("Send relative mouse movements and keyboard key strokes to remote computer. Used with RDP to play games.");
		puts("Default to listening all, port 8555");
		puts("server.exe -b [ip_addr_to_bind] -p [port]");
		return 0;
	}
	config_server_name = getCmdOption(argv, argv + argc, "-b");
	if (config_server_name == NULL) {
		printf("no bind address(argv[1]) provided, defaulting to all.\n");
	}
	char* port = getCmdOption(argv, argv + argc, "-p");
	if (port != NULL) {
		config_ctrlport = port;
	}
	if (cmdOptionExists(argv, argv + argc, "--debug")) {
		config_debug = true;
	}
	if (cmdOptionExists(argv, argv + argc, "--nodebug")) {
		config_debug = false;
	}
	// probably not working
	if (cmdOptionExists(argv, argv + argc, "--tcp")) {
		config_ctrlproto = IPPROTO_TCP;
	}
	printf("bind addr: %s, port: %s\n", config_server_name, config_ctrlport);

	winsock_init();
	sdlmsg_replay_init(NULL);
	pthread_t serverthread;
	if (pthread_create(&serverthread, NULL, ctrl_server_thread, NULL) != 0) {
		printf("Cannot create controller thread, controller disabled.\n");
		return 1;
	}
	pthread_detach(serverthread);

	while (1) {
		usleep(5000000);
	}

	// Deinit
	pthread_cancel(serverthread);

	// End the program
	return 0;
}
