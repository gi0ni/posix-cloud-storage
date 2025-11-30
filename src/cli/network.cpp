#include "network.h"

#include "state.h"
#include "../utils.h"
#include "../packet.h"

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>

#include <sodium/crypto_scalarmult_curve25519.h>
#include <sodium/crypto_aead_aes256gcm.h>

#include <stdlib.h>

#include <iostream>

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
		glb.error = true;
		glb.errormsg = std::string(strerror(errno));
	}
	else
	{
		glb.connected = true;
		glb.error = false;
		printf(WARN "Connected to server.\n" CLEAR);
	}

	// KEY EXCHANGE
	unsigned char private_key[32];
	unsigned char public_key[32];

	int fd = open("/dev/random", O_RDONLY);
	read(fd, private_key, 32);
	close(fd);

	crypto_scalarmult_curve25519_base(public_key, private_key);
	std::cout << "private key: "; HexDump((char*)private_key, 32);
	std::cout << "public  key: "; HexDump((char*)public_key,  32);

	Packet packet(Flags::KEY_EXCHANGE, (char*)public_key, 32);
	packet.Send(glb.serverSocket);
	packet.Recv(glb.serverSocket);

	if(packet.flag == Flags::KEY_EXCHANGE)
	{
		unsigned char peer_public_key[32];
		strncpy((char*)peer_public_key, packet.data, 32);
		std::cout << "peer   key: "; HexDump((char*)peer_public_key, 32);
		int err = crypto_scalarmult_curve25519(glb.secret_key, private_key, peer_public_key);
		// err
		std::cout << "secret key: "; HexDump((char*)glb.secret_key, 32);
	}
	else
	{
		// refused
	}

	printf(OK "KEY EXCHANGE SUCCESS\n" CLEAR);
}

int SendAuthReq()
{
	std::string data = std::string(glb.username) + '\n' + glb.password + '\n';
	std::cout << "expect enc msg len: " << data.size() + 12 + 16 << '\n';
	std::string encrypted_data = Encrypt(data.c_str(), data.size(), glb.secret_key);

	std::cout << "OK LETS CHECK AGAIN WHAT WE ARE SENDING TO SERVER:\n";
	std::cout << "AES MESSAGE:\n";
	std::cout << "nonce     : "; HexDump(std::string(&encrypted_data[0], 12));
	std::cout << "cyphertext: "; HexDump(std::string(&encrypted_data[12], encrypted_data.size() - 12 - 16));
	std::cout << "MAC       : "; HexDump(std::string(&encrypted_data[encrypted_data.size() - 16], 16));

	Packet packet(Flags::AUTH_REQUEST, &encrypted_data[0], encrypted_data.size());
	packet.Send(glb.serverSocket);
	packet.Recv(glb.serverSocket);

	if(packet.flag == Flags::FAILURE)
	{
		printf(ERR "AUTH FAIL\n" CLEAR);
		return -1;
	}

	printf(OK "AUTH SUCCESS\n" CLEAR);
	return 0;
}

