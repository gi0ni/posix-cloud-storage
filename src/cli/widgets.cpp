#include "widgets.h"

#include "client_state.h"
#include "network.h"
#include "utils.h"

#include <sstream>

#include <imgui.h>


// TODO: file preview?


int InputFilterAlphanumeric(ImGuiInputTextCallbackData* data)
{
	char chr = data->EventChar;

	if(chr != 0)
		if(!std::isalpha(chr) && !std::isdigit(chr) && chr != '_' && chr != '.')
		{
			glb.error = true;
			glb.errorMsg = "Alphanumeric, underscore and dot characters only";
			return 1;
		}

	return 0;
}


void ConnectMenu()
{
	if(glb.connected == true)
		return;

	ImGui::Begin("Connect", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
	ImVec2 screenSize = ImGui::GetIO().DisplaySize;
	ImVec2 windowSize = ImGui::GetWindowSize();
	ImGui::SetWindowPos(ImVec2((screenSize.x - windowSize.x)/2, (screenSize.y - windowSize.y)/2));

	ImGui::SetCursorPosY(70);
	ImGui::InputText("ADDR", glb.addr, 64, ImGuiInputTextFlags_CallbackCharFilter, InputFilterAlphanumeric);
	ImGui::InputText("PORT", glb.port, 64, ImGuiInputTextFlags_CallbackCharFilter, InputFilterAlphanumeric);

	ImGui::SetCursorPos(ImVec2(160, 160));
	if(ImGui::Button("Connect", ImVec2(100, 30)))
		Connect();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8784, 0.1059, 0.1412, 1.0000));
	ImGui::SameLine();
	if(ImGui::Button("Exit", ImVec2(100, 30)))
		glb.windowShouldClose = true;
	ImGui::PopStyleColor();

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

	ImGui::Begin("Login", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
	ImVec2 screenSize = ImGui::GetIO().DisplaySize;
	ImVec2 windowSize = ImGui::GetWindowSize();
	ImGui::SetWindowPos(ImVec2((screenSize.x - windowSize.x)/2, (screenSize.y - windowSize.y)/2));

	ImGui::SetCursorPosY(70);
	ImGui::InputText("Username", glb.username, 64, ImGuiInputTextFlags_CallbackCharFilter, InputFilterAlphanumeric);
	ImGui::InputText("Password", glb.password, 64, ImGuiInputTextFlags_Password | ImGuiInputTextFlags_CallbackCharFilter, InputFilterAlphanumeric);

	ImGui::SetCursorPos(ImVec2(100, 160));
	if(ImGui::Button("Login", ImVec2(100, 30)))
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

	ImGui::SameLine();
	if(ImGui::Button("Register", ImVec2(100, 30)))
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

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8784, 0.1059, 0.1412, 1.0000));
	if(ImGui::Button("Disconnect", ImVec2(100, 30)))
	{
		Packet packet(Flags::QUIT, NULL, 0);
		packet.Send(glb.serverSocket);
		glb.connected = false;
	}
	ImGui::PopStyleColor();

	if(glb.error)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 64, 32, 255));
		ImGui::Text("ERR: %s!", glb.errorMsg.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::End();
}


void ContextMenuCreateDir();

void FileViewMenu()
{
	if(glb.auth == false)
		return;

	ImGui::Begin("Files", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
	ImGui::SetWindowPos(ImVec2(0, 0));
	ImGui::SetWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::SetCursorPosY(50);
	ImGui::Text("%s", (glb.displayCWD).c_str());

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 110);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7784, 0.1059, 0.1412, 1.0000));
	if(ImGui::Button("Logout", ImVec2(100, 30)))
	{
		Packet packet(Flags::LOGOUT, NULL, 0);
		packet.Send(glb.serverSocket);
		glb.auth = false;
		ImGui::PopStyleColor();
		ImGui::End();
		return;
	}
	ImGui::PopStyleColor();

	ImGui::SetCursorPosY(100);
	if(!ImGui::BeginListBox("##filelist", ImGui::GetContentRegionAvail()))
	{
		ImGui::End();
		return;
	}

	ContextMenuCreateDir();


	// go back
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 152, 65, 255));
	if(glb.cwd != "/" && ImGui::Selectable("  ..", false))
	{
		Packet packet(Flags::CHANGE_CWD, "../");
		packet.Send(glb.serverSocket);
		packet.Recv(glb.serverSocket);

		if(packet.flag == Flags::FAILURE)
		{
			printf(ERR "%s!\n" CLEAR, packet.DataToStr().c_str());
		}

		int index = glb.cwd.substr(0, glb.cwd.size() - 1).find_last_of("/");
		glb.cwd = glb.cwd.substr(0, index + 1);

		UpdateDirListContents();
	}
	ImGui::PopStyleColor();

	ImDrawList& render = *ImGui::GetWindowDrawList();

	int rownum = 0 + (glb.cwd != "/");
	int index = 0;
	for(auto pair : glb.dirContents)
	{
		std::string file = pair.filename;
		bool isDir = pair.isDir;

		if(isDir) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 152, 65, 255));

		render.ChannelsSplit(2);
		render.ChannelsSetCurrent(1);

		if(ImGui::Selectable(((isDir ? "  " : "") + file + "##selectable").c_str(), false))
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

				glb.cwd += (file + "/");
				UpdateDirListContents();
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
		if(rownum % 2 == 1) render.AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(26, 30, 60, 255));
		render.ChannelsMerge();

		if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			ImGui::OpenPopup( ("ContextMenu" + std::to_string(index)).c_str()  );
		}

		if(ImGui::BeginPopup( ("ContextMenu" + std::to_string(index)).c_str() ))
		{
			ImGui::Text("%s", pair.filename.c_str());
			ImGui::Text("―――――――――――――――");

			if(isDir && ImGui::MenuItem("Open"))
			{
				Packet packet(Flags::FILE_RENAME, NULL, 0);
				packet.Send(glb.serverSocket);
			}

			if(!isDir && ImGui::MenuItem("Download"))
			{
				Packet packet(Flags::FILE_RENAME, NULL, 0);
				packet.Send(glb.serverSocket);
			}

			if(ImGui::MenuItem("Rename"))
			{
				Packet packet(Flags::FILE_RENAME, NULL, 0);
				packet.Send(glb.serverSocket);
			}

			if(ImGui::MenuItem("Copy"))
			{
				Packet packet(Flags::FILE_COPY, NULL, 0);
				packet.Send(glb.serverSocket);
			}

			if(ImGui::MenuItem("Move"))
			{
				Packet packet(Flags::FILE_MOVE, NULL, 0);
				packet.Send(glb.serverSocket);
			}

			if(ImGui::MenuItem("Delete"))
			{
				Packet packet(Flags::FILE_DELETE, NULL, 0);
				packet.Send(glb.serverSocket);
			}

			ImGui::EndPopup();
		}

		rownum++;
		index++;
	}

	ImGui::EndListBox();
	ImGui::End();
}

void ContextMenuCreateDir()
{
	if(ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		ImGui::OpenPopup("ListBoxContextMenu");
	}

	bool flag = false;

	if(ImGui::BeginPopup("ListBoxContextMenu"))
	{
		if(ImGui::MenuItem(" +   New directory"))
			flag = true;

		ImGui::EndPopup();
	}

	if(flag)
	{
		ImGui::OpenPopup("DirnameInputModal");
		ImGui::SetNextWindowPos(ImGui::GetIO().MousePos);
		flag = false;
	}

	if(ImGui::BeginPopup("DirnameInputModal"))
	{
		ImGui::SetWindowFontScale(1.2);
		ImGui::Text("New directory");
		ImGui::SetWindowFontScale(1);
		ImGui::Text("――――――――――――――――――――――");

		ImGui::SetCursorPosY(70);
		ImGui::SetItemDefaultFocus();
		bool pressedEnter = false;
		if(ImGui::InputText("##dirname", glb.inputDirName, 128, ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue, InputFilterAlphanumeric))
			pressedEnter = true;

		ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 220);
		ImGui::SetCursorPosY(120);
		if(ImGui::Button("Cancel", ImVec2(100, 30)))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 110);

		if(ImGui::Button("OK", ImVec2(100, 30)) || pressedEnter)
		{
			Packet packet(Flags::CREATE_DIR, glb.inputDirName, strlen(glb.inputDirName));
			packet.Send(glb.serverSocket);
			packet.Recv(glb.serverSocket);

			if(packet.flag == Flags::FAILURE)
			{
				printf(ERR "%s!\n" CLEAR, packet.DataToStr().c_str());
			}

			UpdateDirListContents();
			ImGui::CloseCurrentPopup();
			glb.inputDirName[0] = '\0';
		}

		ImGui::EndPopup();
	}
}
