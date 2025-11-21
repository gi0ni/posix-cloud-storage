#include "utils.h"

void PrintErr(const char* msg)
{
	fprintf(stderr, ERR);
	perror(msg);
	fprintf(stderr, CLEAR);
}
