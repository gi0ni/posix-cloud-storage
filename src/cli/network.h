#ifndef NETWORK_H
#define NETWORK_H

#include "packet.h"

void Connect();
void SendAuthReq(Flags flag);
void UpdateDirListContents();
void HandleDropFile(const char* filepath);

#endif
