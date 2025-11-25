#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H

#include <imgui.h>
#include <vector>
#include <string>

class State
{
	private:
	static State* instance;

	State();

	public:
	int serverSocket;
	bool windowShouldClose = false;

	bool flag = false;
	int counter = 0;

	int value = 123456;

	ImFont* mainFont;

	char ip[64];
	char port[64];
	bool connected = false;

	std::string cwd;
	std::vector<std::string> dirContents;

	static State& GetInstance();
};

extern State& glb;

#endif
