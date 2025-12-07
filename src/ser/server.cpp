#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <errno.h>
#include <cstring>
#include <pthread.h>

#include <iostream>
#include <cassert>

#include <tinyxml2.h>
#include <sodium.h>

#include "../utils.h"
#include "../packet.h"
#include "worker.h"
#include "server_state.h"

using namespace tinyxml2;

void SigInt_Handler(int sig)
{
	printf(WARN "\nServer closed forcefully.\n" CLEAR);
	glb.doc.SaveFile("usr/users.xml");
	close(glb.listenSocket);
	// shutdown
	exit(1);
}

void SigPipe_Handler(int sig)
{
	printf(ERR "SIGPIPE WHY\n" CLEAR);
	exit(1);
}

ThreadInfo threadPool[MAXTHREADS];

int main(int argc, char** argv)
{
	if(sodium_init())
	{
		std::cout << "Failed to init sodium!!\n";
		return 1;
	}

	if(isdir("usr") == false)
	{
		mkdir("usr", 0775);
		std::cout << "no!\n";
	}

	if(glb.doc.LoadFile("usr/users.xml"))
	{
		std::cout << "database not found\n";
		XMLElement* users = glb.doc.NewElement("users");
		glb.doc.InsertEndChild(users);
	}

	//////////////////////

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
	fcntl(glb.listenSocket, F_SETFL, fcntl(glb.listenSocket, F_GETFL, 0) | O_NONBLOCK);

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons((short)atoi(argv[2]));

	if(bind(glb.listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
	{
		if(errno == EADDRINUSE)
		{
			printf(WARN "Address is marked as used. Trying to reuse...\n" CLEAR);
			// int flag = 1;
			// setsockopt(glb.listenSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)); // FIX: well this doesnt seem to work

			if(bind(glb.listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
			{
				PrintErr("bind");
				close(glb.listenSocket);
				return 1;
			}
		}

		printf("I GAVE UP THE FIRST TIME\n");
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

	int serverTimeout = 120;

	while(true)
	{
		// FIX: use alarm with non blocking socket instead
		if(glb.clientCount == 0)
		{
			if(serverTimeout % 20 == 0 || serverTimeout < 10)
				printf("Server times out in %d second(s)...\n", serverTimeout);

			int step = 1;
			serverTimeout -= step;

			if(serverTimeout <= 0)
				break;
		}

		sleep(1);

		int clientSocket;
		sockaddr_in clientAddr;
		socklen_t addrLength = sizeof(clientAddr);

		clientSocket = accept(glb.listenSocket, (sockaddr*)&clientAddr, &addrLength);
		if(clientSocket < 0)
		{
			if(errno != EAGAIN && errno != EWOULDBLOCK)
			{
				PrintErr("accept");
				close(glb.listenSocket);
				return 1;
			}
		}
		else
		{
			printf(WARN "\nClient %d connected from %s:%d.\n" CLEAR, glb.clientCount, inet_ntoa({clientAddr.sin_addr.s_addr}), clientAddr.sin_port);

			int threadIndex = 0;
			for(int i = 0; i < MAXTHREADS; i++)
				if(threadPool[i].alive == false)
				{
					threadIndex = i;
					break;
				}

			ThreadInfo& info = threadPool[threadIndex];
			info.clientSocket = clientSocket;
			info.id = glb.clientCount;
			pthread_create(&info.thread, NULL, ServerWorker, &info);

			glb.clientCount++;
			serverTimeout = 120;
		}
	}

	printf(WARN "Server terminated.\n" CLEAR);
	// shutdown
	close(glb.listenSocket);

	/////
	glb.doc.SaveFile("usr/users.xml");
	return 0;
}
