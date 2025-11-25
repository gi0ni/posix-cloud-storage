#include "widgets.h"

#include <imgui.h>

#include "state.h"
#include "network.h"
#include "../utils.h"

#include <iostream>
#include <filesystem>

// TODO: when clicking on file show a simple preview with ImGUI::Text in a new window

namespace fs = std::filesystem;

void ConnectMenu()
{
	if(glb.connected)
		return;

	ImGuiIO& io = ImGui::GetIO();

	ImGui::Begin("Menu");
	ImGui::Text("%.1f", io.Framerate);
	ImGui::Text("%d", glb.value);

	if(ImGui::Button("CLICK ME"))
	{
		glb.flag = true;
		glb.counter++;
	}

	if(glb.flag == true)
	{
		ImGui::Text("you clicked me!! x%d", glb.counter);
	}

	ImGui::InputText("IP", glb.ip, 64);
	ImGui::InputText("PORT", glb.port, 64);

	if(!glb.connected && ImGui::Button("Connect"))
	{
		Connect();
		glb.connected = true;
	}

	if(glb.connected && ImGui::Button("Disconnect"))
	{
		glb.windowShouldClose = true;
	}

	ImGui::End();
}

void FileViewMenu()
{
	ImGui::Begin("Files");

	ImGui::Text("%s", (fs::canonical(glb.cwd)).c_str());
	ImGui::BeginListBox("##filelist", ImGui::GetContentRegionAvail()); // FIX: will return error if too small or hidden. need to handle that

	ImDrawList& render = *ImGui::GetWindowDrawList();

	int index = 0;
	for(auto file : glb.dirContents)
	{
		bool isDir = fs::is_directory(glb.cwd + "/" + file);

		if(isDir)
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 152, 65, 255));
		// ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 255));

		render.ChannelsSplit(2);

		render.ChannelsSetCurrent(1);

		if(ImGui::Selectable(( (isDir ? " " : "") + file).c_str(), false))
		{
			if(isDir)
			{
				glb.cwd = fs::canonical(glb.cwd + "/" + file);
				std::cout << glb.cwd << '\n';
				glb.dirContents = Crawl(glb.cwd);

				if(isDir)
					ImGui::PopStyleColor();
				break;
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
