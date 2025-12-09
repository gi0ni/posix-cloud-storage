#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

#define MAX_THREADS 2
#define MAX_CLIENTS_PER_THREAD 2

struct ThreadInfo
{
	// pthread_t thread;
	bool alive;

	int id;
	// int clientSocket;
};

void* ServerWorker(void* arg);

#endif
