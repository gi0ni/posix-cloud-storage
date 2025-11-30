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

	bool connected = false;
	char ip[64];
	char port[64];

	unsigned char secret_key[32];

	bool auth = false;
	char username[64];
	char password[64];
	unsigned char salt_e[32];

	bool error = true;
	std::string errormsg;

	std::string cwd;
	std::vector<std::string> dirContents;

	static State& GetInstance();
};

extern State& glb;

#endif
