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
	int serverSocket;
	bool windowShouldClose = false;

	bool flag = false;
	int counter = 0;

	int value = 123456;

	ImFont* mainFont;

	bool connected = false;
	char addr[64] = "";
	char port[64] = "";

	unsigned char secret_key[32];

	bool auth = false;
	char username[64] = "";
	char password[64] = "";
	unsigned char salt_e[32];
	unsigned char fileKey[32];

	bool error = false;
	std::string errormsg;

	std::string cwd = "/";
	std::vector<std::pair<std::string, bool>> dirContents;
	char inputDirName[128];

	static State& GetInstance();
};

extern State& glb;

#endif
