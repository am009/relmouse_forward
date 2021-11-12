// sdltest.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <map>
#include <pthread.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include "ctrl-sdl.h"
#include "controller.h"

void
ProcessEvent(SDL_Event* event);

int config_ctrlproto = IPPROTO_UDP;
const char* config_ctrlport = "8555";
const char* config_server_name = NULL;
const char* config_log_file = NULL;
float config_opacity = 0.2f; // window default transparency
bool config_debug =
#ifdef _DEBUG
true;
#else
false;
#endif

bool config_full_screen = false;
bool config_relative_mouse = true;
bool eventloop_running = true;
int showCursor = 0;

double scale_mouse_X = 1920 / 1920;
double scale_mouse_Y = 1080 / 1080;

SDL_Window* window = NULL;

extern "C" int SDL_main(int argc, char* argv[])
{
	if (cmdOptionExists(argv, argv + argc, "-h") || cmdOptionExists(argv, argv + argc, "--help"))
	{
		puts("github.com/am009/relmouse-forward client");
		puts("Send relative mouse movements and keyboard key strokes to remote computer. Used with RDP to play games.");
		puts("Default port 8555");
		puts("server.exe -s [server_ip] -p [port]");
		return 0;
	}
	config_server_name = getCmdOption(argv, argv + argc, "-s");
	if (config_server_name == NULL) {
		printf("please specify server IP address using -s.\n");
		return 0;
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
	printf("server addr: %s, server_port: %s\n", config_server_name, config_ctrlport);

	ga_set_process_dpi_aware();
	winsock_init();
	// Pointers to our window and surface
	SDL_Surface* winSurface = NULL;
	pthread_t ctrlthread;
	
	// Initialize SDL. SDL_Init will return -1 if it fails.
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		std::cout << "Error initializing SDL: " << SDL_GetError() << std::endl;
		system("pause");
		// End the program
		return 1;
	}

	if (ctrl_queue_init(32768, sizeof(sdlmsg_t)) < 0) {
		printf("Cannot initialize controller queue, controller disabled.\n");
		return 1;
	}
	if (pthread_create(&ctrlthread, NULL, ctrl_client_thread, NULL) != 0) {
		printf("Cannot create controller thread, controller disabled.\n");
		return 1;
	}
	pthread_detach(ctrlthread);



	int wflag = 0;
	if (config_relative_mouse) {
		wflag |= SDL_WINDOW_INPUT_GRABBED; // relativeMouseMode
	}
	wflag |= SDL_WINDOW_RESIZABLE;
	wflag |= SDL_WINDOW_SHOWN;
	if (config_full_screen) {
		wflag |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
	}
	// Create our window
	window = SDL_CreateWindow("Controler", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 280, 150, wflag );

	// Make sure creating the window succeeded
	if (!window) {
		std::cout << "Error creating window: " << SDL_GetError() << std::endl;
		// End the program
		return 1;
	}

	//// Get the surface from the window
	winSurface = SDL_GetWindowSurface(window);

	//// Make sure getting the surface succeeded
	if (!winSurface) {
		std::cout << "Error getting surface: " << SDL_GetError() << std::endl;
		return 1;
	}

	// Fill the window with a white rectangle
	SDL_FillRect(winSurface, NULL, SDL_MapRGB(winSurface->format, 255, 255, 255));

	SDL_SetWindowOpacity(window, config_opacity);

	// Update the window display
	SDL_UpdateWindowSurface(window);


	if (config_relative_mouse) {
		// SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1"); // WRAP MOUSE
		SDL_SetRelativeMouseMode(SDL_TRUE);
		showCursor = 0;
	}

	SDL_Event e;

	
	while (eventloop_running) {
		if (SDL_WaitEvent(&e)) {
			ProcessEvent(&e);
		}
	}

	// Deinit
	pthread_cancel(ctrlthread);

	// Destroy the window. This will also destroy the surface
	SDL_DestroyWindow(window);

	// Quit SDL
	SDL_Quit();

	// End the program
	return 0;
}

static int
scale_mouseX(int x) {
	return (int)(scale_mouse_X * x);
}

static int
scale_mouseY(int y) {
	return (int)(scale_mouse_Y * y);
}


static void
switch_grab_input(SDL_Window* w) {
	SDL_bool grabbed;
	if (w != NULL) {
		grabbed = SDL_GetWindowGrab(w);
		if (grabbed == SDL_FALSE)
			SDL_SetWindowGrab(w, SDL_TRUE);
		else
			SDL_SetWindowGrab(w, SDL_FALSE);
	}
	return;
}

void
ProcessEvent(SDL_Event* event) {
	sdlmsg_t m;
	std::map<unsigned int, int>::iterator mi;
	struct timeval tv;

	switch (event->type) {
	case SDL_KEYUP:
		if (event->key.keysym.sym == SDLK_BACKQUOTE
			&& config_relative_mouse) {
			showCursor = 1 - showCursor;
			//SDL_ShowCursor(showCursor);
			switch_grab_input(window);
#if 1
			if (showCursor)
				SDL_SetRelativeMouseMode(SDL_FALSE);
			else
				SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
		}
		// switch between fullscreen?
		if ((event->key.keysym.sym == SDLK_RETURN)
			&& (event->key.keysym.mod & KMOD_ALT)) {
			// do nothing
		}
		else {
			sdlmsg_keyboard(&m, 0,
				event->key.keysym.scancode,
				event->key.keysym.sym,
				event->key.keysym.mod,
				0/*event->key.keysym.unicode*/);
			ctrl_client_sendmsg(&m, sizeof(sdlmsg_keyboard_t));
		}
		if (config_debug) {
			gettimeofday(&tv, NULL);
			printf("KEY-UP: %u.%06u scan 0x%04x sym 0x%04x mod 0x%04x\n",
				tv.tv_sec, tv.tv_usec,
				event->key.keysym.scancode,
				event->key.keysym.sym,
				event->key.keysym.mod);
		}
		break;
	case SDL_KEYDOWN:
		if ((event->key.keysym.sym == SDLK_RETURN)
			&& (event->key.keysym.mod & KMOD_ALT)) {
			// switch_fullscreen();
		}
		else {
			sdlmsg_keyboard(&m, 1,
				event->key.keysym.scancode,
				event->key.keysym.sym,
				event->key.keysym.mod,
				0/*event->key.keysym.unicode*/);
			ctrl_client_sendmsg(&m, sizeof(sdlmsg_keyboard_t));
		}
		if (config_debug) {
			gettimeofday(&tv, NULL);
			printf("KEY-DN: %u.%06u scan 0x%04x sym 0x%04x mod 0x%04x\n",
				tv.tv_sec, tv.tv_usec,
				event->key.keysym.scancode,
				event->key.keysym.sym,
				event->key.keysym.mod);
		}
		break;
	case SDL_MOUSEBUTTONUP:
		sdlmsg_mousekey(&m, 0, event->button.button,
			scale_mouseX(event->button.x),
			scale_mouseY(event->button.y));
		ctrl_client_sendmsg(&m, sizeof(sdlmsg_mouse_t));
		break;
	case SDL_MOUSEBUTTONDOWN:
		sdlmsg_mousekey(&m, 1, event->button.button,
			scale_mouseX(event->button.x),
			scale_mouseY(event->button.y));
		ctrl_client_sendmsg(&m, sizeof(sdlmsg_mouse_t));
		break;
	case SDL_MOUSEMOTION:
		if (!showCursor) {
			if (config_debug) {
				printf("xrel: %d, xrel-s: %d, yrel: %d, yrel-s: %d\n", event->motion.xrel, scale_mouseX(event->motion.xrel), event->motion.yrel, scale_mouseY(event->motion.yrel));
			}
			sdlmsg_mousemotion(&m,
				scale_mouseX(event->motion.x),
				scale_mouseY(event->motion.y),
				scale_mouseX(event->motion.xrel),
				scale_mouseY(event->motion.yrel),
				event->motion.state,
				config_relative_mouse ? 1 : 0);
			ctrl_client_sendmsg(&m, sizeof(sdlmsg_mouse_t));
		}
		break;
	case SDL_MOUSEWHEEL:
		sdlmsg_mousewheel(&m, event->motion.x, event->motion.y);
		ctrl_client_sendmsg(&m, sizeof(sdlmsg_mouse_t));
		break;
	case SDL_WINDOWEVENT:
		if (event->window.event == SDL_WINDOWEVENT_CLOSE) {
			eventloop_running = false;
			return;
		}
		else if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
			int w, h;
			char title[64];
			w = event->window.data1;
			h = event->window.data2;

			snprintf(title, sizeof(title), "Controller (%dx%d)", w, h);
			SDL_SetWindowTitle(window, title);
			printf("event window (%x) resized: w=%d h=%d\n",
				event->window.windowID, w, h);
		}
		break;
	case SDL_QUIT:
		eventloop_running = false;
		return;
	default:
		// do nothing
		break;
	}
	return;
}
