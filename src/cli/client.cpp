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

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>

// TODO: console window with imgui. would be a pain tho
// listbox with textinput and its totally doable
// can't print anymore need to redirect here

// FIX: used non docking imgui accidentally. actually i might have mixed them

int main(int argc, char** argv)
{
	// FIX: parse args properly
	strcpy(glb.ip, argv[1]);
	strcpy(glb.port, argv[2]);
	///////////////////////////////////////////

	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("Client", 1152, 648, SDL_WINDOW_RESIZABLE);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
	SDL_SetRenderVSync(renderer, 1);
	SDL_ShowWindow(window);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	glb.mainFont = io.Fonts->AddFontFromFileTTF("./res/fonts/NotoSansNerdFontPropo-Regular.ttf", 20);
	// glb.mainFont = io.Fonts->AddFontFromFileTTF("./res/fonts/JetBrainsMonoNerdFontMono-Regular.ttf", 20);
	IM_ASSERT(glb.mainFont != NULL);
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer3_Init(renderer);

	int frame = 0;
	// std::cout << "" << '\n';

	glb.cwd = ".";
	glb.dirContents = Crawl(glb.cwd);

	while(glb.windowShouldClose == false)
	{
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_EVENT_QUIT:
					glb.windowShouldClose = true;
				break;

				case SDL_EVENT_KEY_DOWN:
					if(!io.WantTextInput && event.key.key == SDLK_ESCAPE)
						glb.windowShouldClose = true;
				break;

				case SDL_EVENT_DROP_FILE:
					std::cout << event.drop.data << '\n';
				break;
			}

			ImGui_ImplSDL3_ProcessEvent(&event);
		}

		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		ImGui::PushFont(glb.mainFont);
		// ConnectMenu();
		FileViewMenu();

		ImGui::Begin("asdf");
		ImGui::Text("%d", glb.value);

		if(ImGui::Button("mod"))
		{
			glb.value = -33;
		}

		ImGui::End();
		ImGui::PopFont();

		ImGui::Render();
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

	if(glb.connected)
	{
		Packet packet(Flags::QUIT, NULL, 0);
		packet.Send(glb.serverSocket);
		packet.Send(glb.serverSocket);

		printf(WARN "Disconnected from server.\n" CLEAR);
		close(glb.serverSocket);
	}

	SDL_Quit();
	return 0;
}
