#include <unistd.h>

#include <cstdlib>
#include <cstring>

#include "utils.h"
#include "client_state.h"
#include "widgets.h"
#include "network.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <SDL2/SDL.h>
#include <nfd.h>

void ParseArgs(int argc, char** argv);

int main(int argc, char** argv)
{
	ParseArgs(argc, argv);

	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1152, 648, SDL_WINDOW_RESIZABLE);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_ShowWindow(window);

	NFD_Init();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	glb.mainFont = io.Fonts->AddFontFromFileTTF("./res/fonts/NotoSansNerdFontPropo-Regular.ttf", 20);
	IM_ASSERT(glb.mainFont != NULL);
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer2_Init(renderer);

	while(glb.windowShouldClose == false)
	{
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_QUIT:
					glb.windowShouldClose = true;
				break;

				case SDL_KEYDOWN:
					if(!io.WantTextInput && event.key.keysym.sym == SDLK_ESCAPE)
						glb.windowShouldClose = true;
				break;

				case SDL_DROPFILE:
					HandleUploadFile(event.drop.file);
				break;
			}

			ImGui_ImplSDL2_ProcessEvent(&event);
		}

		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGui::PushFont(glb.mainFont);

		ImGuiErrorRecoveryState state;
		ImGui::ErrorRecoveryStoreState(&state);
		io.ConfigErrorRecoveryEnableAssert = false;

		try
		{
			ConnectMenu();
			AuthMenu();
			FileViewMenu();

			if(io.MouseReleased[0])
			{
				glb.dragFrame++;

				if(glb.dragFrame == 2)
				{
					glb.fileDragged = false;
					glb.dragFrame = 0;
				}
			}
			else
				glb.dragFrame = 0;
		}
		catch(std::exception& e)
		{
			printf(ERR "Unhandled exception caught in main!\n" CLEAR);
			printf(ERR "what: %s\n" CLEAR, e.what());

			glb.error = true;
			glb.errorMsg = "Lost connection";
			glb.auth = false;
			glb.connected = false;
			close(glb.serverSocket);

			ImGui::ErrorRecoveryTryToRecoverState(&state);
		}

		ImGui::PopFont();

		ImGui::Render();
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

	if(glb.connected)
	{
		Packet packet(Flags::QUIT, NULL, 0);

		try
		{
			packet.Send(glb.serverSocket);
		}
		catch(std::exception& e)
		{
			// just quit
		}

		printf(WARN "\nDisconnected from server.\n" CLEAR);
		close(glb.serverSocket);
	}

	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	NFD_Quit();
	return 0;
}

void ParseArgs(int argc, char** argv)
{
	bool malformedArgs = false;
	int i;

	for(i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "--ip") == 0)
		{
			if(i + 1 < argc && strlen(argv[i + 1]) < 64)
			{
				strcpy(glb.addr, argv[++i]);
			}
		}

		else if(strcmp(argv[i], "--port") == 0)
		{
			if(i + 1 < argc && strlen(argv[i + 1]) < 64)
			{
				strcpy(glb.port, argv[++i]);
			}
		}

		else if(strcmp(argv[i], "--username") == 0)
		{
			if(i + 1 < argc && strlen(argv[i + 1]) < 64)
			{
				strcpy(glb.username, argv[++i]);
			}
		}

		else if(strcmp(argv[i], "--password") == 0)
		{
			if(i + 1 < argc && strlen(argv[i + 1]) < 64)
			{
				strcpy(glb.password, argv[++i]);
			}
		}

		else
		{
			malformedArgs = true;
			break;
		}
	}

	if(malformedArgs == true)
	{
		printf(ERR "Malformed arguments. Received unexpected argument '%s'!\n" CLEAR, argv[i]);
		printf(ERR "Recognized options are as follows:\n* --ip <address>\n* --port <number>\n* --username <user>\n* --password <pass>\n" CLEAR);
		exit(1);
	}
}
