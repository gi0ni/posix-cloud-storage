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
		glb.errormsg = std::string(strerror(errno));
		glb.error = true;
		return;
	}
	else
	{
		glb.connected = true;
		glb.error = false;
		printf(WARN "Connected to server.\n" CLEAR);
	}


	// key exchange
	unsigned char private_key[32];
	unsigned char public_key[32];

	RandomBytes(private_key, 32);

	crypto_scalarmult_curve25519_base(public_key, private_key);

	Packet packet(Flags::KEY_EXCHANGE, (char*)public_key, 32);
	packet.Send(glb.serverSocket);
	packet.Recv(glb.serverSocket);

	unsigned char peer_public_key[32];
	memcpy((char*)peer_public_key, packet.data, 32);

	if(crypto_scalarmult_curve25519(glb.secret_key, private_key, peer_public_key))
	{
		printf(ERR "Key exchange failed!\n" CLEAR);
		glb.errormsg = std::string("Key exchange failed");
		glb.error = true;

		Packet packet(Flags::QUIT, NULL, 0);
		packet.Send(glb.serverSocket);
		close(glb.serverSocket);
		glb.connected = false;
		return;
	}

	// FIX: check key worked with predefined msg

	printf(OK "Key exchange finished successfully.\n" CLEAR);
	std::cout << "private key: "; HexDump((char*)private_key, 32);
	std::cout << " public key: "; HexDump((char*)public_key, 32);
	std::cout << "   peer key: "; HexDump((char*)peer_public_key, 32);
	std::cout << " secret key: "; HexDump((char*)glb.secret_key, 32);
	std::cout << '\n';
}


// FIX: server crash on login
void SendAuthReq(Flags flag)
{
	std::string data = std::string(glb.username) + '\n' + glb.password + '\n';
	std::string encrypted_data = Encrypt(data.c_str(), data.size(), glb.secret_key);

	assert(encrypted_data.size() == data.size() + 12 + 16);

	Packet packet(flag, &encrypted_data[0], encrypted_data.size());

	try
	{
		packet.Send(glb.serverSocket);
		packet.Recv(glb.serverSocket);
	}
	catch(std::exception& e)
	{
		throw e;
	}

	if(packet.flag == Flags::FAILURE)
	{
		printf(ERR "Failed to get authenticated!\n" CLEAR);
		glb.errormsg = packet.DataToStr();
		glb.error = true;
		return;
	}

	memcpy(glb.salt_e, packet.data, 32);

	int err = crypto_pwhash_argon2id(
			glb.fileKey,
			32,
			glb.password,
			strlen(glb.password),
			glb.salt_e,
			crypto_pwhash_argon2id_OPSLIMIT_INTERACTIVE,
			crypto_pwhash_argon2id_MEMLIMIT_INTERACTIVE,
			crypto_pwhash_argon2id_ALG_ARGON2ID13
	);

	if(err)
	{
		printf(ERR "Failed to get authenticated!\n" CLEAR);
		glb.errormsg = packet.DataToStr();
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
	// ask list files 
	Packet packet2(Flags::DIR_LIST_REQUEST, NULL, 0);
	packet2.Send(glb.serverSocket);
	packet2.Recv(glb.serverSocket);

	std::stringstream stream;
	stream.write(packet2.data, packet2.size - 8); // FIX: confusignly named packet2. just make a move ctor

	std::cout << "Received directory listing: " << stream.str() << '\n';
	std::cout << "recv: " << packet2.size << '\n';

	glb.dirContents.clear();
	std::cout << "after clear: " << glb.dirContents.size() << '\n';

	while(!stream.eof())
	{
		std::string filename;
		bool isDir;
		stream >> isDir;
		stream.ignore(1, '\n');
		std::getline(stream, filename, '\n');

		// FIX:
		if(filename.size() == 0) break;
		std::cout << isDir << ' ' << filename << '\n';

		glb.dirContents.push_back(std::make_pair(filename, isDir));
	}

	std::sort(glb.dirContents.begin(), glb.dirContents.end(), [](const auto& leftpair, const auto& rightpair) {

		std::string l = leftpair.first;
		std::string r = rightpair.first;

		if(leftpair.second && !rightpair.second)
			return true;
		if(!leftpair.second && rightpair.second)
			return false;

		return l < r;
	});

	std::cout << "sizeof dircontents: " << glb.dirContents.size() << '\n';
}

void HandleDropFile(const char* filepath)
{
	std::cout << filepath << '\n';

	if(glb.auth == true)
	{
		try
		{
			SendFile(filepath, glb.serverSocket, glb.fileKey); // FIX: check if file exists before sending message begin to server
			UpdateDirListContents();
		}
		catch(std::exception& e)
		{

		}
	}
	else
	{
		// FIX:
	}
}
