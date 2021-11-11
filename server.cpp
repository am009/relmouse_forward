// server.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <pthread.h>

#include "ga-common.h"
#include "ctrl-sdl.h"
#include "controller.h"

int config_ctrlproto = IPPROTO_UDP;
USHORT config_ctrlport = 8555;
const char* config_server_name = "0.0.0.0";
const char* config_log_file = NULL;
bool config_debug = false;

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf("please input IP address as argument.\n");
		return 0;
	}
	config_server_name = argv[1];
	printf("server addr: %s\n", config_server_name);

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

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
