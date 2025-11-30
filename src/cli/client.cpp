#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>

#include <stdlib.h>
#include <cstring>
#include <iostream>

#include "../utils.h"
#include "../packet.h"
#include "state.h"
#include "widgets.h"

#include "sodium/core.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <SDL2/SDL.h>

// TODO: console window with imgui. would be a pain tho
// listbox with textinput and its totally doable
// can't print anymore need to redirect here

// FIX: used non docking imgui accidentally. actually i might have mixed them

int main(int argc, char** argv)
{
	// FIX: parse args properly
	strcpy(glb.ip, argv[1]);
	strcpy(glb.port, argv[2]);
	strcpy(glb.username, argv[3]);
	strcpy(glb.password, argv[4]);
	///////////////////////////////////////////

	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1152, 648, SDL_WINDOW_RESIZABLE);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_ShowWindow(window);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	glb.mainFont = io.Fonts->AddFontFromFileTTF("./res/fonts/NotoSansNerdFontPropo-Regular.ttf", 20);
	// glb.mainFont = io.Fonts->AddFontFromFileTTF("./res/fonts/JetBrainsMonoNerdFontMono-Regular.ttf", 20);
	IM_ASSERT(glb.mainFont != NULL);
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer2_Init(renderer);

	int frame = 0;
	// std::cout << "" << '\n';

	glb.cwd = ".";
	// glb.dirContents = Crawl(glb.cwd);

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
					std::cout << event.drop.file << '\n';
				break;
			}

			ImGui_ImplSDL2_ProcessEvent(&event);
		}

		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGui::PushFont(glb.mainFont);
		ConnectMenu(); // FIX: needs to let you try again. not just crash the entire app
		AuthMenu();
		FileViewMenu();

		ImGui::PopFont();

		ImGui::Render();
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

	if(glb.connected)
	{
		Packet packet(Flags::QUIT, NULL, 0);
		packet.Send(glb.serverSocket);

		printf(WARN "Disconnected from server.\n" CLEAR);
		close(glb.serverSocket);
	}

	// FIX: CLEAR IMGUI 

	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
