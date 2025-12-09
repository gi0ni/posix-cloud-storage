#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

#define MAX_THREADS 2
#define MAX_CLIENTS_PER_THREAD 2

struct ThreadInfo
{
	bool alive;
	int id;
};

void* ServerWorker(void* arg);

#endif
