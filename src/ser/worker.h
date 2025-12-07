#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

#define MAXTHREADS 100

struct ThreadInfo
{
	pthread_t thread;
	bool alive;

	int id;
	int clientSocket;
};

void* ServerWorker(void* arg);

#endif
