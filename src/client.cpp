#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <signal.h>

#include <stdlib.h>
#include <cstring>

#include "utils.h"
#include "packet.h"

int serverSocket;

void SigInt_Handler(int sig)
{
	Packet packet;
	packet.flag = Flags::QUIT;
	packet.size = 8;
	SendPacket(packet, serverSocket);

	printf(WARN "\nDisconnected forcefully.\n" CLEAR);
	close(serverSocket);
	exit(1);
}

int main(int argc, char** argv)
{
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(serverSocket < 0)
	{
		PrintErr("socket");
		return 1;
	}

	signal(SIGINT, SigInt_Handler);

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
	serverAddr.sin_port = htons((short)atoi(argv[2]));

	if(connect(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
	{
		// TODO: retry connection
		PrintErr("connect");
		return 1;
	}

	printf(WARN "Connected to server.\n" CLEAR);

	getchar();

	Packet packet;
	packet.flag = Flags::QUIT;
	packet.size = 8;
	SendPacket(packet, serverSocket);

	printf(WARN "Disconnected from server.\n" CLEAR);

	close(serverSocket);
	return 0;
}
