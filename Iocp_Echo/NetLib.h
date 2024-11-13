#pragma once
#define SERVERPORT 9000
#include <list>
#include <winsock2.h>
#include <assert.h>
#include <unordered_map>

class Session;
class CPacket;

extern std::unordered_map<int, Session*> g_sessionMap;
unsigned int WINAPI AcceptThread(void* arg);
unsigned int WINAPI NetworkThread(void* arg);

void Disconnect(Session* pSession);

void RecvPost(Session* pSession);
void SendPost(Session* pSession);

void RecvProcess(Session* pSession);
void SendProcess(Session* pSession);

void OnRecv(Session* pSession, char* packet, int size);
void SendPacket(Session* pSession, char* packet, int size);

bool ProcessRecvMessage(Session* pSession);

extern bool g_bShutdown;

extern SOCKET listenSocket;