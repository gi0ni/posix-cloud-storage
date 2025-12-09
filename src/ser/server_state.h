#ifndef SERVER_STATE_H
#define SERVER_STATE_H

#include "worker.h"

#include <tinyxml2.h>
using namespace tinyxml2;

class State
{
	private:
	static State* instance;
	State();

	public:
	static State& GetInstance();

	char addr[64];
	int listenSocket;
	pthread_mutex_t listenSocketMut;
	pthread_t threadPool[MAX_THREADS];

	int clientCount;
	int serverTimeout = 120;

	XMLDocument usersXML;
	pthread_mutex_t usersXMLMut;

	pthread_mutex_t miscMut;
};

extern State& glb;

#endif
