#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <signal.h>

#include <stdlib.h>
#include <cstring>

#include "utils.h"
#include "packet.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>

namespace glb
{
	int serverSocket;
	bool windowShouldClose = false;

	char ip[64];
	char port[64];
};

void SigInt_Handler(int sig)
{
	Packet packet;
	packet.flag = Flags::QUIT;
	packet.size = 8;
	SendPacket(packet, glb::serverSocket);

	printf(WARN "\nDisconnected forcefully.\n" CLEAR);
	close(glb::serverSocket);
	exit(1);
}

int main(int argc, char** argv)
{
	strcpy(glb::ip, argv[1]);
	strcpy(glb::port, argv[2]);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
	SDL_Window* window = SDL_CreateWindow("Client", 1152, 648, SDL_WINDOW_RESIZABLE);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
	SDL_SetRenderVSync(renderer, 1);
	SDL_ShowWindow(window);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImFont* mainFont = io.Fonts->AddFontFromFileTTF("./res/fonts/NotoSans-Regular.ttf", 20);
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer3_Init(renderer);

	glb::serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(glb::serverSocket < 0)
	{
		PrintErr("socket");
		return 1;
	}

	signal(SIGINT, SigInt_Handler);

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
	serverAddr.sin_port = htons((short)atoi(argv[2]));

	if(connect(glb::serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
	{
		// TODO: retry connection
		PrintErr("connect");
		return 1;
	}

	printf(WARN "Connected to server.\n" CLEAR);

	bool flag = false;
	int counter = 0;

	int frame = 0;

	while(glb::windowShouldClose == false)
	{
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			if(event.type == SDL_EVENT_QUIT)
				glb::windowShouldClose = true;
			else if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
				glb::windowShouldClose = true;

			ImGui_ImplSDL3_ProcessEvent(&event);
		}

		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		ImGui::PushFont(mainFont);
		ImGui::Begin("Menu");
		ImGui::Text("%.1f", io.Framerate);

		if(ImGui::Button("CLICK ME"))
		{
			flag = true;
			counter++;
		}

		if(flag == true)
		{
			ImGui::Text("you clicked me!! x%d", counter);
		}

		ImGui::InputText("IP", glb::ip, 64);
		ImGui::InputText("PORT", glb::port, 64);

		ImGui::PopFont();
		ImGui::End();

		ImGui::Render();
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

	Packet packet;
	packet.flag = Flags::QUIT;
	packet.size = 8;
	SendPacket(packet, glb::serverSocket);

	printf(WARN "Disconnected from server.\n" CLEAR);

	close(glb::serverSocket);
	SDL_Quit();
	return 0;
}
