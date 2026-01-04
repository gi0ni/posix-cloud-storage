#include "network.h"

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "client_state.h"
#include "utils.h"
#include "packet.h"

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

#include <sodium.h>


void SigInt_Handler(int sig)
{
	Packet packet(Flags::QUIT, NULL, 0);
	packet.Send(glb.serverSocket);

	printf(WARN "\nDisconnected forcefully.\n" CLEAR);
	close(glb.serverSocket);
	exit(1);
}

void SigPipe_Handler(int sig)
{
	printf(ERR "Received signal SIGPIPE!\n" CLEAR);
}


void Connect()
{
	// connect
	glb.serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(glb.serverSocket < 0)
	{
		PrintErr("socket");
		exit(1);
	}

	signal(SIGINT, SigInt_Handler);
	signal(SIGPIPE, SigPipe_Handler);

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(glb.addr);
	serverAddr.sin_port = htons((short)atoi(glb.port));

	if(connect(glb.serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
	{
		PrintErr("connect");
		glb.errorMsg = std::string(strerror(errno));
		glb.error = true;
		return;
	}
	else
	{
		glb.connected = true;
		glb.error = false;
		printf(WARN "Connected to server.\n\n" CLEAR);
	}


	// key exchange
	unsigned char privateKey[32];
	unsigned char publicKey[32];

	RandomBytes(privateKey, 32);

	crypto_scalarmult_curve25519_base(publicKey, privateKey);

	Packet packet(Flags::KEY_EXCHANGE, (char*)publicKey, 32);
	packet.Send(glb.serverSocket);
	packet.Recv(glb.serverSocket);

	unsigned char peerPublicKey[32];
	memcpy((char*)peerPublicKey, packet.data, 32);

	if(crypto_scalarmult_curve25519(glb.secretKey, privateKey, peerPublicKey))
	{
		printf(ERR "Key exchange failed!\n" CLEAR);
		glb.errorMsg = std::string("Key exchange failed");
		glb.error = true;

		Packet packet(Flags::QUIT, NULL, 0);
		packet.Send(glb.serverSocket);
		close(glb.serverSocket);
		glb.connected = false;
		return;
	}

	// FIX: check key worked with predefined msg

	printf(OK "Key exchange finished successfully.\n" CLEAR);
	std::cout << "private key: "; HexDump((char*)privateKey, 32);
	std::cout << " public key: "; HexDump((char*)publicKey, 32);
	std::cout << "   peer key: "; HexDump((char*)peerPublicKey, 32);
	std::cout << " secret key: "; HexDump((char*)glb.secretKey, 32);
	std::cout << '\n';
}


void SendAuthReq(Flags flag)
{
	std::string data = std::string(glb.username) + '\n' + glb.password + '\n';
	std::string encrypted_data = Encrypt(data.c_str(), data.size(), glb.secretKey);

	assert(encrypted_data.size() == data.size() + 12 + 16);

	Packet packet(flag, &encrypted_data[0], encrypted_data.size());

	try
	{
		packet.Send(glb.serverSocket);
		packet.Recv(glb.serverSocket);
	}
	catch(std::exception& e)
	{
		throw e; // FIX: ?
	}

	if(packet.flag == Flags::FAILURE)
	{
		printf(ERR "Failed to get authenticated!\n" CLEAR);
		glb.errorMsg = packet.DataToStr();
		glb.error = true;
		return;
	}

	unsigned char salt_e[32];
	memcpy(salt_e, packet.data, 32);

	int err = crypto_pwhash_argon2id(
			glb.fileKey,
			32,
			glb.password,
			strlen(glb.password),
			salt_e,
			crypto_pwhash_argon2id_OPSLIMIT_INTERACTIVE,
			crypto_pwhash_argon2id_MEMLIMIT_INTERACTIVE,
			crypto_pwhash_argon2id_ALG_ARGON2ID13
	);

	if(err)
	{
		printf(ERR "Failed to get authenticated!\n" CLEAR);
		glb.errorMsg = packet.DataToStr();
		glb.error = true;
		return;
	}

	UpdateDirListContents();

	printf(OK "Authenticated successfully.\n" CLEAR);
	std::cout << "file key: "; HexDump((char*)glb.fileKey, 32);
	glb.auth = true;
	glb.error = false;
	return;
}

void UpdateDirListContents()
{
	Packet packet(Flags::DIR_LIST_REQUEST, NULL, 0);
	packet.Send(glb.serverSocket);
	packet.Recv(glb.serverSocket);

	std::stringstream stream;
	stream << packet.DataToStr();

	glb.dirContents.clear();

	while(!stream.eof())
	{
		FileEntry entry;

		stream >> entry.isDir; stream.ignore(1, '\n');
		std::getline(stream, entry.filename, '\n');
		
		if(entry.isDir == false)
		{
			stream >> entry.size;

			long long timestamp; stream >> timestamp;
			struct tm* date = localtime((time_t*)&timestamp);
			if(date)
			{
				entry.date = std::to_string(date->tm_mday) + "/" + std::to_string(date->tm_mon + 1) + "/" + std::to_string(date->tm_year + 1900);
			}
			else
			{
				entry.date = "unknown date";
			}
		}

		if(entry.filename.size() == 0) break;

		glb.dirContents.push_back(entry);
	}

	std::sort(glb.dirContents.begin(), glb.dirContents.end(), [](const auto& above, const auto& below)
	{
		std::string filenameA = above.filename;
		std::string filenameB = below.filename;
		bool isDirA = above.isDir;
		bool isDirB = below.isDir;

		if(isDirA && !isDirB)
			return true;

		if(!isDirA && isDirB)
			return false;

		return filenameA < filenameB;
	});

	glb.displayCWD = glb.username;

	std::stringstream ss; ss << glb.cwd;
	std::string token;
	while(std::getline(ss, token, '/'))
	{
		glb.displayCWD += token;
		glb.displayCWD += "  ";
	}
}

void HandleUploadFile(const char* filepath)
{
	// std::cout << "Detected dropped file at path: " << filepath << '\n';

	if(glb.auth == true)
	{
		try
		{
			SendFile(filepath, glb.serverSocket, glb.fileKey);
			UpdateDirListContents();
		}
		catch(std::exception& e)
		{
			printf(ERR "%s\n" CLEAR, e.what());
		}
	}
	else
	{
		printf(ERR "Cannot upload file to server. You are not logged in!\n" CLEAR);
	}
}
