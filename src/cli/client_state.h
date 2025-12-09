#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H

#include <vector>
#include <string>
#include <imgui.h>

class State
{
	private:
	static State* instance;
	State();

	public:
	static State& GetInstance();

	bool windowShouldClose = false;
	ImFont* mainFont;

	bool connected = false;
	int serverSocket;

	char addr[64] = "";
	char port[64] = "";

	unsigned char secretKey[32];
	unsigned char fileKey[32];

	bool auth = false;
	char username[64] = "";
	char password[64] = "";

	std::string errorMsg;
	bool error = false;

	std::string cwd = "/";
	std::vector<std::pair<std::string, bool>> dirContents;
	char inputDirName[128];
};

extern State& glb;

#endif
