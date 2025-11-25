#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <signal.h>

#include <stdlib.h>
#include <errno.h>
#include <cstring>
#include <pthread.h>
#include <iostream>

#include "../utils.h"
#include "../packet.h"

namespace glb
{
	int listenSocket;
	int clientCount;

	pthread_mutex_t mut;
};

void SigInt_Handler(int sig)
{
	printf(WARN "\nServer closed forcefully.\n" CLEAR);
	close(glb::listenSocket);
	// shutdow 
	exit(1);
}

#define MAXTHREADS 100

struct ThreadInfo
{
	pthread_t thread;
	bool alive;

	int id;
	int clientSocket;
};

ThreadInfo threadPool[MAXTHREADS];

void* ServerWorker(void* arg)
{
	pthread_detach(pthread_self());
	ThreadInfo& info = *(ThreadInfo*)arg;
	info.alive = true;

	Packet packet;

	try
	{
		packet.Recv(info.clientSocket);
	}
	catch(std::exception& e)
	{
		printf(ERR "Failed to recv from client %d.\n" CLEAR, info.id);
		goto cleanup;
	}


	printf(OK "Received flag %d from client.\n" CLEAR, packet.flag);


	cleanup:
	info.alive = false;
	close(info.clientSocket);
	pthread_mutex_lock(&glb::mut);
	glb::clientCount -= 1;
	pthread_mutex_unlock(&glb::mut);
	printf("Thread %d closed succesfully.\n", info.id);
	printf(WARN "Client %d disconnected.\n\n" CLEAR, info.id);
	return NULL;
}

int main(int argc, char** argv)
{
	glb::listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(glb::listenSocket < 0)
	{
		PrintErr("socket");
		return 1;
	}

	signal(SIGINT, SigInt_Handler);

	fcntl(glb::listenSocket, F_SETFL, fcntl(glb::listenSocket, F_GETFL, 0) | O_NONBLOCK);

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons((short)atoi(argv[2]));

	if(bind(glb::listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
	{
		if(errno == EADDRINUSE)
		{
			printf(WARN "Address is marked as used. Trying to reuse...\n" CLEAR);
			int flag = 1;
			setsockopt(glb::listenSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

			if(bind(glb::listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
			{
				PrintErr("bind");
				close(glb::listenSocket);
				return 1;
			}
		}

		PrintErr("bind");
		close(glb::listenSocket);
		return 1;
	}

	if(listen(glb::listenSocket, 10))
	{
		PrintErr("listen");
		close(glb::listenSocket);
		return 1;
	}

	printf(WARN "Server started.\nWaiting on IP address %s:%d...\n\n" CLEAR, inet_ntoa({serverAddr.sin_addr.s_addr}), ntohs(serverAddr.sin_port));

	int serverTimeout = 120;

	while(true)
	{
		// FIX: use alarm with non blocking socket instead
		if(glb::clientCount == 0)
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

		clientSocket = accept(glb::listenSocket, (sockaddr*)&clientAddr, &addrLength);
		if(clientSocket < 0)
		{
			if(errno != EAGAIN && errno != EWOULDBLOCK)
			{
				PrintErr("accept");
				close(glb::listenSocket);
				return 1;
			}
		}
		else
		{
			printf(WARN "\nClient %d connected from %s:%d.\n" CLEAR, glb::clientCount, inet_ntoa({clientAddr.sin_addr.s_addr}), clientAddr.sin_port);

			int threadIndex = 0;
			for(int i = 0; i < MAXTHREADS; i++)
				if(threadPool[i].alive == false)
				{
					threadIndex = i;
					break;
				}

			ThreadInfo& info = threadPool[threadIndex];
			info.clientSocket = clientSocket;
			info.id = glb::clientCount;
			pthread_create(&info.thread, NULL, ServerWorker, &info);

			glb::clientCount++;
			serverTimeout = 120;
		}
	}

	printf(WARN "Server terminated.\n" CLEAR);
	// shutdown
	close(glb::listenSocket);
	return 0;
}
