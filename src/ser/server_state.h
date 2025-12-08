#ifndef SERVER_STATE_H
#define SERVER_STATE_H

#include <imgui.h>
#include <vector>
#include <string>

#include <tinyxml2.h>
using namespace tinyxml2;

class State
{
	private:
	static State* instance;

	State();

	public:
	static State& GetInstance();

	int listenSocket;
	int clientCount;

	pthread_mutex_t listenSocketMut;

	XMLDocument doc;
	pthread_mutex_t xmldocwriteMut;

	int serverTimeout = 120;
};

extern State& glb;

#endif
