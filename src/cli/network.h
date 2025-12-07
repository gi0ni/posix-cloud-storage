#ifndef CLIENT_NETWORK_H
#define CLIENT_NETWORK_H

#include "packet.h"

void Connect();
int SendAuthReq(Flags flag);
void UpdateDirListContents();

#endif
