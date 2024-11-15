#pragma once
#define SERVERPORT 6000
#include <list>
#include <winsock2.h>
#include <assert.h>
#include <unordered_map>

class Session;
class CPacket;

extern std::unordered_map<int, Session*> g_sessionMap;
extern CRITICAL_SECTION g_sessionMapCs;
unsigned int WINAPI AcceptThread(void* arg);
unsigned int WINAPI NetworkThread(void* arg);

void RecvPost(Session* pSession);
void SendPost(Session* pSession);

void OnRecv(UINT64 sessionID, CPacket* packet);
bool Release(UINT64 sessionID);
void SendPacket(UINT64 sessionID, CPacket* packet);

bool ProcessRecvMessage(Session* pSession);

extern bool g_bShutdown;

extern SOCKET listenSocket;