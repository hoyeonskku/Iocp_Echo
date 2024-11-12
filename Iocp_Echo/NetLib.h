#pragma once
#define SERVERPORT 9000
#include <list>
#include <winsock2.h>
#include <assert.h>

class Session;
class CPacket;

unsigned int WINAPI AcceptThread(void* arg);
unsigned int WINAPI NetworkThread(void* arg);

void Disconnect(Session* pSession);

void RecvPost(Session* pSession);
void SendPost(Session* pSession);

void RecvProcess(Session* pSession);
void SendProcess(Session* pSession);


extern bool g_bShutdown;

extern SOCKET listenSocket;