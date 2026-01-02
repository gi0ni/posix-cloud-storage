#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <cstring>
#include <pthread.h>

#include <iostream>
#include <cassert>

#include <tinyxml2.h>
#include <sodium.h>

#include "utils.h"
#include "worker.h"
#include "server_state.h"


using namespace tinyxml2;


void SigInt_Handler(int sig)
{
	printf(WARN "\nServer closed forcefully.\n" CLEAR);
	glb.usersXML.SaveFile("usr/users.xml");
	close(glb.listenSocket);
	exit(1);
}

void SigPipe_Handler(int sig)
{
	printf(ERR "Received signal SIGPIPE!\n" CLEAR);
}


void ParseArgs(int argc, char** argv);

int main(int argc, char** argv)
{
	// init libs
	ParseArgs(argc, argv);

	if(sodium_init())
	{
		std::cout << "Failed to init sodium!\n";
		return 1;
	}

	if(IsDirectory("usr") == false)
	{
		printf(WARN "Creating dir 'usr'...\n" CLEAR);
		if(mkdir("usr", 0775))
		{
			PrintErr("mkdir");
			exit(1);
		}
	}

	if(glb.usersXML.LoadFile("usr/users.xml"))
	{
		printf(WARN "Creating 'usr/users.xml'...\n" CLEAR);
		XMLElement* users = glb.usersXML.NewElement("users");
		glb.usersXML.InsertEndChild(users);
	}


	// open connection
	glb.listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(glb.listenSocket < 0)
	{
		PrintErr("socket");
		return 1;
	}

	signal(SIGINT, SigInt_Handler);
	signal(SIGPIPE, SigPipe_Handler);

	int flag = 1;
	setsockopt(glb.listenSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons((short)atoi(glb.addr));

	if(bind(glb.listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
	{
		PrintErr("bind");
		close(glb.listenSocket);
		return 1;
	}

	if(listen(glb.listenSocket, 10))
	{
		PrintErr("listen");
		close(glb.listenSocket);
		return 1;
	}

	printf(WARN "Server started.\nWaiting on IP address %s:%d...\n\n" CLEAR, inet_ntoa({serverAddr.sin_addr.s_addr}), ntohs(serverAddr.sin_port));

	for(int i = 0; i < MAX_THREADS; i++)
	{
		ThreadInfo* info = new ThreadInfo();
		info->id = i;
		info->alive = true;
		pthread_create(&glb.threadPool[i], NULL, ServerWorker, info);
	}

	while(true)
	{
		pthread_mutex_lock(&glb.miscMut);
		if(glb.clientCount == 0)
		{
			if(glb.serverTimeout % 20 == 0 || glb.serverTimeout < 10)
				printf("Server times out in %d second(s)...\n", glb.serverTimeout);

			int step = 1;
			glb.serverTimeout -= step;

			if(glb.serverTimeout <= 0)
				break;
		}
		pthread_mutex_unlock(&glb.miscMut);

		sleep(1);
	}

	printf(WARN "\nServer terminated.\n" CLEAR);
	close(glb.listenSocket);

	glb.usersXML.SaveFile("usr/users.xml");
	return 0;
}

void ParseArgs(int argc, char** argv)
{
	bool malformedArgs = false;
	int i;

	for(i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "--port") == 0)
		{
			if(i + 1 < argc && strlen(argv[i + 1]) < 64)
			{
				strcpy(glb.addr, argv[++i]);
			}
		}

		else
		{
			malformedArgs = true;
			break;
		}
	}

	if(malformedArgs == true)
	{
		printf(ERR "Malformed arguments. Received unexpected argument '%s'!\n" CLEAR, argv[i]);
		printf(ERR "Recognized options are as follows:\n* --port <number>\n" CLEAR);
		exit(1);
	}
}
