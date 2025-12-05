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
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <cassert>

#include <tinyxml2.h>
#include <sodium.h>

#include "../utils.h"
#include "../packet.h"

using namespace tinyxml2;

namespace glb
{
	int listenSocket;
	int clientCount;

	pthread_mutex_t mut;

	XMLDocument doc;
	pthread_mutex_t xmldocwriteMut;
};

void SigInt_Handler(int sig)
{
	printf(WARN "\nServer closed forcefully.\n" CLEAR);
	glb::doc.SaveFile("usr/users.xml");
	close(glb::listenSocket);
	// shutdown
	exit(1);
}

void SigPipe_Handler(int sig)
{
	printf(ERR "SIGPIPE WHY\n" CLEAR);
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

// TODO: put this in its own file bruh
void* ServerWorker(void* arg)
{
	pthread_detach(pthread_self());
	ThreadInfo& info = *(ThreadInfo*)arg;
	info.alive = true;

	Packet packet;
	// std::cout << (packet.data == nullptr) << '\n';

	// FIX: what if the xml doc gets destroyed midway through

	unsigned char public_key[32];
	unsigned char private_key[32];
	unsigned char secret_key[32];

	int fd = open("/dev/random", O_RDONLY);
	read(fd, &private_key[0], 32);
	close(fd);
	
	crypto_scalarmult_curve25519_base(public_key, private_key);
	std::cout << "private key: "; HexDump((char*)private_key, 32);
	std::cout << "public  key: "; HexDump((char*)public_key,  32);
	bool auth = false;

	std::string userdir;
	XMLDocument userFiles;
	std::string filename;
	std::string filenameenc;
	std::ofstream fout;
	int currfilesize;

	while(true)
	{
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

		switch(packet.flag)
		{
			case Flags::KEY_EXCHANGE:
			{
				unsigned char peer_public_key[32];
				memcpy((char*)peer_public_key, packet.data, 32);
				std::cout << "peer key: "; HexDump((char*)peer_public_key, 32);

				int err = crypto_scalarmult_curve25519(secret_key, private_key, peer_public_key);
				std::cout << "secret key: "; HexDump((char*)secret_key, 32);

				printf("SEND BACK KEY\n");
				std::cout << "packing: "; HexDump((char*)public_key, 32);
				Packet response(Flags::KEY_EXCHANGE, (char*)public_key, 32);
				std::cout << "sending: "; HexDump((char*)response.data, 32);
				response.Send(info.clientSocket);
			}
			break;

			case Flags::REGISTER_REQUEST:
			{
				std::string data = Decrypt(packet.data, packet.size - 8, secret_key);

				std::stringstream stream;
				stream << data;

				std::string username; 
				std::getline(stream, username, '\n');
				std::string password;
				std::getline(stream, password, '\n');

				XMLElement* user = glb::doc.FirstChildElement("users")->FirstChildElement("user");

				bool found = false;

				while(user)
				{
					std::string u = user->FirstChildElement("username")->GetText();

					if(u == username)
					{
						found = true;
						break;
					}

					user = user->NextSiblingElement("user");
				}

				if(found)
				{
					Packet response(Flags::FAILURE, NULL, 0);
					response.Send(info.clientSocket);
					auth = false;
				}
				else
				{
					unsigned char hash  [64];
					unsigned char salt_p[32]; // WARN: SALT IS 32B NOT 16B
					unsigned char salt_e[32];

					int fd = open("/dev/random", O_RDONLY);
					read(fd, salt_p, 32);
					read(fd, salt_e, 32);
					close(fd);

					int err = crypto_pwhash_scryptsalsa208sha256(
							hash,
							64,
							&password[0],
							password.size(),
							salt_p,
							crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE,
							crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE
					);

					XMLElement* users = glb::doc.FirstChildElement("users");
					int count = users->ChildElementCount("user");

					XMLElement* user = users->InsertNewChildElement("user");
					user->SetAttribute("id", count);
					user->InsertNewChildElement("username")->SetText(username.c_str());
					user->InsertNewChildElement("hash")->SetText(ToHexString((char*)hash, 64).c_str());
					user->InsertNewChildElement("salt-p")->SetText(ToHexString((char*)salt_p, 32).c_str());
					user->InsertNewChildElement("salt-e")->SetText(ToHexString((char*)salt_e, 32).c_str());

					mkdir( ("usr/" + username).c_str(), 0775 );

					XMLElement* files = userFiles.NewElement("files");
					userFiles.InsertEndChild(files);
					userFiles.SaveFile( ("usr/" + username + "/files.xml").c_str() );

					Packet response(Flags::ACCEPT, (char*)salt_e, 32);
					response.Send(info.clientSocket);

					userdir = ("usr/" + username + "/");
					auth = true;
				}
			}
			break;

			case Flags::AUTH_REQUEST:
			{
				std::cout << "received: " << packet.size - 8 << '\n';
				std::cout << packet.data << '\n';

				std::string data = Decrypt(packet.data, packet.size - 8, secret_key);

				std::stringstream stream;
				stream << data;

				std::string username; 
				std::getline(stream, username, '\n');
				std::string password;
				std::getline(stream, password, '\n');

				std::cout << username << '\n';
				std::cout << password << '\n';

				XMLElement* user = glb::doc.FirstChildElement("users")->FirstChildElement("user");

				bool found = false;

				while(user)
				{
					std::string u = user->FirstChildElement("username")->GetText();

					if(u == username)
					{
						found = true;
						break;
					}

					user = user->NextSiblingElement("user");
				}

				if(found)
				{
					unsigned char oldhash[64];
					unsigned char hash[64];
					unsigned char salt_p[32];

					memcpy((char*)oldhash, FromHexString(user->FirstChildElement("hash")->GetText(), 128).c_str(), 64);
					memcpy((char*)salt_p,  FromHexString(user->FirstChildElement("salt-p")->GetText(), 64).c_str(), 32);

					int err = crypto_pwhash_scryptsalsa208sha256(
							hash,
							64,
							password.c_str(),
							password.size(),
							salt_p,
							crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE,
							crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE
					);

					if(memcmp(hash, oldhash, 64) == 0)
					{
						unsigned char salt_e[32];
						memcpy((char*)salt_e, FromHexString(user->FirstChildElement("salt-e")->GetText(), 64).c_str(), 32);

						Packet response(Flags::ACCEPT, (char*)salt_e, 32);
						response.Send(info.clientSocket);
						userdir = ("usr/" + username + "/");
						userFiles.LoadFile( (userdir + "files.xml").c_str() );
						auth = true;
						break;
					}
				}

				Packet response(Flags::FAILURE, NULL, 0);
				response.Send(info.clientSocket);

				auth = false;
			}
			break;

			case Flags::SEND_FILE_BEGIN: // FIX: if file exists then ask for confirmation to overwrite
			{
				std::stringstream stream;
				stream.write(packet.data, packet.size - 8);

				stream >> filename;
				stream >> currfilesize;

				std::cout << "preparing to write " << userdir + filename << '\n';
				fout.open(userdir + filename);
			}
			break;

			case Flags::FILE_CHUNK:
			{
				std::cout << "FILE CHUNK SIZE: " << packet.size - 8 << '\n';
				fout.write(packet.data, packet.size - 8);
			}
			break;

			case Flags::SEND_FILE_END:
			{
				// check file intact

				// make backup

				XMLElement* file = userFiles.FirstChildElement("files")->InsertNewChildElement("file");

				// FIX: hash file name

				// FIX: there could be a filename collision. use scrypt with salt.

				file->InsertNewChildElement("name")->SetText(filename.c_str());
				file->InsertNewChildElement("size")->SetText(currfilesize);
				file->InsertNewChildElement("birth")->SetText(time(NULL));

				std::cout << "FILE TRANSFER COMPLETE!\n";
				fout.close(); // WARN: do i have to manually add '\n' at end..?
			}
			break;

			case Flags::DIR_LIST_REQUEST:
			{
				if(packet.size == 8)
				{
					XMLElement* files = userFiles.FirstChildElement("files");
					XMLElement* file = files->FirstChildElement("file");

					std::stringstream stream;

					while(file)
					{
						std::string name = file->FirstChildElement("name")->GetText();

						stream << name << '\n';
						file = file->NextSiblingElement("file");
					}

					if(stream.str().size() != 0)
					{
						std::cout << "Sending to client list stream: " << stream.str() << "; stream size is: " << stream.str().size() << '\n';
						Packet response(Flags::DIR_LIST, stream.str().data(), stream.str().size());
						std::cout << "Sending to client list stream: " << std::string(response.data) << "; stream size is: " << response.size << '\n';
						response.Send(info.clientSocket);
					}
					else
					{
						Packet response(Flags::DIR_LIST, nullptr, 0);
						response.Send(info.clientSocket);
					}
				}
			}
			break;

			case Flags::FILE_REQUEST:
			{
				std::string filename = std::string(packet.data, packet.size - 8);
				std::string filepath = userdir + filename;
				// prepend cwd
				
				std::cout << "filename: " << filename << '\n';
				SendFile(filepath.c_str(), info.clientSocket, (unsigned char*)"", false);
			}
			break;

			case Flags::QUIT:
			{
				goto cleanup;
			}
			break;
		}
	}

	cleanup:
	userFiles.SaveFile( (userdir + "files.xml").c_str() );
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

	if(glb::doc.LoadFile("usr/users.xml"))
	{
		std::cout << "database not found\n";
		XMLElement* users = glb::doc.NewElement("users");
		glb::doc.InsertEndChild(users);
	}

	//////////////////////

	glb::listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(glb::listenSocket < 0)
	{
		PrintErr("socket");
		return 1;
	}

	signal(SIGINT, SigInt_Handler);
	signal(SIGPIPE, SigPipe_Handler);

	int flag = 1;
	setsockopt(glb::listenSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
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
			// int flag = 1;
			// setsockopt(glb::listenSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)); // FIX: well this doesnt seem to work

			if(bind(glb::listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
			{
				PrintErr("bind");
				close(glb::listenSocket);
				return 1;
			}
		}

		printf("I GAVE UP THE FIRST TIME\n");
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

	/////
	glb::doc.SaveFile("usr/users.xml");
	return 0;
}
