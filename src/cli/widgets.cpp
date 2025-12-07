#include "widgets.h"

#include <imgui.h>

#include "client_state.h"
#include "network.h"
#include "utils.h"

#include <iostream>
#include <filesystem>

// TODO: when clicking on file show a simple preview with ImGUI::Text in a new window

namespace fs = std::filesystem;

void ConnectMenu()
{
	if(glb.connected == true)
		return;

	ImGuiIO& io = ImGui::GetIO();

	ImGui::Begin("Connect");
	// ImGui::Text("%.1f", io.Framerate);

	if(glb.flag == true)
	{
		ImGui::Text("you clicked me!! x%d", glb.counter);
	}

	ImGui::InputText("IP", glb.ip, 64);
	ImGui::InputText("PORT", glb.port, 64);

	if(!glb.connected && ImGui::Button("Connect"))
	{
		Connect();
	}

	if(glb.error)
		ImGui::Text("%s", glb.errormsg.c_str());

	if(glb.connected && ImGui::Button("Disconnect"))
	{
		glb.windowShouldClose = true;
	}

	ImGui::End();
}

void AuthMenu()
{
	if(!glb.connected || glb.auth)
		return;

	ImGui::Begin("Login");
	ImGui::Text("Hello");
	ImGui::InputText("Username", glb.username, 64); // FIX: LIMIT CHARACTERS TO ALPHANUMERIC
	ImGui::InputText("Password", glb.password, 64);
	if(ImGui::Button("Login"))
	{
		if(SendAuthReq(Flags::AUTH_REQUEST))
		{
			glb.error = true;
			glb.errormsg = "Lost connection";
			glb.connected = false;
			ImGui::End();
			return;
		}
		else
		{
			glb.error = false;
			glb.auth = true;
		}
	}

	if(ImGui::Button("Register"))
	{
		if(SendAuthReq(Flags::REGISTER_REQUEST))
		{
			glb.error = true;
		}
		else
		{
			glb.error = false;
			glb.auth = true;
		}
	}

	if(glb.error)
		ImGui::Text("wrong password");

	// TODO: register button

	ImGui::End();
}

void FileViewMenu()
{
	if(glb.auth == false)
		return;

	ImGui::Begin("Files");

	ImGui::Text("%s", (glb.cwd).c_str());
	if(ImGui::Button("Create Folder"))
	{
		Packet packet(Flags::CREATE_DIR, glb.inputDirName, strlen(glb.inputDirName));
		packet.Send(glb.serverSocket);
		packet.Recv(glb.serverSocket);

		if(packet.flag == Flags::FAILURE)
		{
			printf(ERR "%s\n" CLEAR, packet.DataToStr().c_str());
		}

		UpdateDirListContents();
	}

	if(ImGui::Button("TAKE ME BACK"))
	{
		std::string dir("../");
		Packet packet(Flags::CHANGE_DIR, dir.data(), dir.size());
		packet.Send(glb.serverSocket);
		packet.Recv(glb.serverSocket);

		if(packet.flag == Flags::FAILURE)
		{
			printf(ERR "%s\n" CLEAR, packet.DataToStr().c_str());
		}

		UpdateDirListContents();

		int index = glb.cwd.substr(0, glb.cwd.size() - 1).find_last_of("/");
		glb.cwd = glb.cwd.substr(0, index + 1);
		if(glb.cwd == "")
			glb.cwd = "/";
	}

	ImGui::SameLine();

	ImGui::InputText("##dirname", glb.inputDirName, 128);
	// FIX: crashes when minimizing window
	if(!ImGui::BeginListBox("##filelist", ImGui::GetContentRegionAvail())) // FIX: will return error if too small or hidden. need to handle that
	{
		ImGui::End();
		return;
	}

	ImDrawList& render = *ImGui::GetWindowDrawList();

	int index = 0;
	for(auto pair : glb.dirContents)
	{
		std::string file = pair.first;
		bool isDir = pair.second;

		if(isDir)
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 152, 65, 255));
		// ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 255));

		render.ChannelsSplit(2);

		render.ChannelsSetCurrent(1);

		if(ImGui::Selectable(( (isDir ? " " : "") + file + "##selectable").c_str(), false))
		{
			if(isDir)
			{
				Packet packet(Flags::CHANGE_DIR, file.data(), file.size());
				packet.Send(glb.serverSocket);
				packet.Recv(glb.serverSocket);

				if(packet.flag == Flags::FAILURE)
				{
					printf(ERR "%s\n" CLEAR, packet.DataToStr().c_str());
				}

				UpdateDirListContents();
				glb.cwd += (file + "/");
			}
			else
			{
				RecvFile(file.c_str(), glb.serverSocket, glb.fileKey, true);
			}
		}
		if(isDir)
			ImGui::PopStyleColor();

		render.ChannelsSetCurrent(0);
		if(index % 2 == 1)
			render.AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(26, 30, 60, 255));

		render.ChannelsMerge();

		// ImGui::PopStyleColor();

		index++;
	}

	// if(ImGui::IsItemHovered())
	// 	ImGui::Text("hovered!");
	ImGui::EndListBox();

	ImGui::End();
}
