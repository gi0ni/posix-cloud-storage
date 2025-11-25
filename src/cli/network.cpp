#include "network.h"

#include "state.h"
#include "../utils.h"
#include "../packet.h"

#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>

#include <stdlib.h>

void SigInt_Handler(int sig)
{
	Packet packet(Flags::QUIT, NULL, 0);
	packet.Send(glb.serverSocket);

	printf(WARN "\nDisconnected forcefully.\n" CLEAR);
	close(glb.serverSocket);
	exit(1);
}

void Connect()
{
	glb.serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(glb.serverSocket < 0)
	{
		PrintErr("socket");
		exit(1);
	}

	signal(SIGINT, SigInt_Handler);

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(glb.ip);
	serverAddr.sin_port = htons((short)atoi(glb.port));

	if(connect(glb.serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
	{
		// TODO: retry connection
		PrintErr("connect");
		exit(1);
	}

	printf(WARN "Connected to server.\n" CLEAR);
}

