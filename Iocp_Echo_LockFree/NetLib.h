#pragma once
#define SERVERPORT 6000
#include <list>
#include <winsock2.h>
#include <assert.h>
#include <unordered_map>

class Session;
class CPacket;

unsigned int WINAPI AcceptThread(void* arg);
unsigned int WINAPI NetworkThread(void* arg);

void RecvPost(Session* pSession);
void SendPost(Session* pSession);

void OnAccept(UINT64 sessionID);
void OnRecv(UINT64 sessionID, CPacket* packet);

bool Release(UINT64 sessionID);
void SendPacket(UINT64 sessionID, CPacket* packet);

void ProcessRecvMessage(Session* pSession, int cbTransferred);

extern bool g_bShutdown;

extern SOCKET listenSocket;

extern Session g_SessionArray[10000];