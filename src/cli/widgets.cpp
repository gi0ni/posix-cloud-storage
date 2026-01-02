#include "widgets.h"

#include <iostream>

#include "client_state.h"
#include "network.h"
#include "utils.h"

#include <imgui.h>


// TODO: file preview?


void ConnectMenu()
{
	if(glb.connected == true)
		return;

	ImGui::Begin("Connect");
	ImGui::InputText("ADDR", glb.addr, 64);
	ImGui::InputText("PORT", glb.port, 64);

	if(ImGui::Button("Connect"))
		Connect();

	if(glb.error)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 64, 32, 255));
		ImGui::Text("ERR: %s!", glb.errorMsg.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::End();
}


void AuthMenu()
{
	if(glb.connected == false || glb.auth == true)
		return;

	ImGui::Begin("Login");
	ImGui::InputText("Username", glb.username, 64); // FIX: limit chars to alphanumeric
	ImGui::InputText("Password", glb.password, 64);

	if(ImGui::Button("Login"))
	{
		try
		{
			SendAuthReq(Flags::LOGIN_REQUEST);
		}
		catch(std::exception& e)
		{
			glb.errorMsg = "Lost connection";
			glb.error = true;
			glb.connected = false;
			ImGui::End();
			return;
		}
	}

	if(ImGui::Button("Register"))
	{
		try
		{
			SendAuthReq(Flags::REGISTER_REQUEST);
		}
		catch(std::exception& e)
		{
			glb.errorMsg = "Lost connection";
			glb.error = true;
			glb.connected = false;
			ImGui::End();
			return;
		}
	}

	if(ImGui::Button("Disconnect"))
	{
		Packet packet(Flags::QUIT, NULL, 0);
		packet.Send(glb.serverSocket);
		glb.connected = false;
	}

	if(glb.error)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 64, 32, 255));
		ImGui::Text("ERR: %s!", glb.errorMsg.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::End();
}


void FileViewMenu()
{
	if(glb.auth == false)
		return;

	ImGui::Begin("Files");

	if(ImGui::Button("Logout"))
	{
		Packet packet(Flags::LOGOUT, NULL, 0);
		packet.Send(glb.serverSocket);
		glb.auth = false;
		ImGui::End();
		return;
	}

	ImGui::Text("%s", (glb.cwd).c_str());

	ImGui::InputText("##dirname", glb.inputDirName, 128);
	ImGui::SameLine();
	if(ImGui::Button("Create Folder"))
	{
		Packet packet(Flags::CREATE_DIR, glb.inputDirName, strlen(glb.inputDirName));
		packet.Send(glb.serverSocket);
		packet.Recv(glb.serverSocket);

		if(packet.flag == Flags::FAILURE)
		{
			printf(ERR "%s!\n" CLEAR, packet.DataToStr().c_str());
		}

		UpdateDirListContents();
	}

	if(!ImGui::BeginListBox("##filelist", ImGui::GetContentRegionAvail()))
	{
		ImGui::End();
		return;
	}


	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 152, 65, 255));
	if(glb.cwd != "/" && ImGui::Selectable(" ..", false))
	{
		Packet packet(Flags::CHANGE_CWD, "../");
		packet.Send(glb.serverSocket);
		packet.Recv(glb.serverSocket);

		if(packet.flag == Flags::FAILURE)
		{
			printf(ERR "%s!\n" CLEAR, packet.DataToStr().c_str());
		}

		UpdateDirListContents();

		int index = glb.cwd.substr(0, glb.cwd.size() - 1).find_last_of("/");
		glb.cwd = glb.cwd.substr(0, index + 1);
	}
	ImGui::PopStyleColor();

	ImDrawList& render = *ImGui::GetWindowDrawList();

	int index = 0 + (glb.cwd != "/");
	for(auto pair : glb.dirContents)
	{
		std::string file = pair.first;
		bool isDir = pair.second;

		if(isDir) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 152, 65, 255));

		render.ChannelsSplit(2);
		render.ChannelsSetCurrent(1);

		if(ImGui::Selectable(((isDir ? " " : "") + file + "##selectable").c_str(), false))
		{
			if(isDir)
			{
				Packet packet(Flags::CHANGE_CWD, file.data(), file.size());
				packet.Send(glb.serverSocket);
				packet.Recv(glb.serverSocket);

				if(packet.flag == Flags::FAILURE)
				{
					printf(ERR "%s!\n" CLEAR, packet.DataToStr().c_str());
				}

				UpdateDirListContents();
				glb.cwd += (file + "/");
			}
			else
			{
				try
				{
					RecvFile(file.c_str(), glb.serverSocket, glb.fileKey, true);
				}
				catch(std::exception& e)
				{
					printf(ERR "%s\n" CLEAR, e.what());
				}
			}
		}

		if(isDir) ImGui::PopStyleColor();

		render.ChannelsSetCurrent(0);
		if(index % 2 == 1) render.AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(26, 30, 60, 255));
		render.ChannelsMerge();

		index++;
	}

	ImGui::EndListBox();
	ImGui::End();
}


void ContextMenu()
{
	if(glb.auth == false)
		return;

	ImGui::Begin("Context Menu");

	if(ImGui::Button("DELETE"))
	{
		Packet packet(Flags::FILE_DELETE, NULL, 0);
		packet.Send(glb.serverSocket);
	}

	if(ImGui::Button("RENAME"))
	{
		Packet packet(Flags::FILE_RENAME, NULL, 0);
		packet.Send(glb.serverSocket);
	}

	if(ImGui::Button("COPY"))
	{
		Packet packet(Flags::FILE_COPY, NULL, 0);
		packet.Send(glb.serverSocket);
	}

	if(ImGui::Button("MOVE"))
	{
		Packet packet(Flags::FILE_MOVE, NULL, 0);
		packet.Send(glb.serverSocket);
	}

	ImGui::End();
}
