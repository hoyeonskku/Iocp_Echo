#pragma once
#pragma comment(lib, "ws2_32")
#include <ws2tcpip.h>
#include <unordered_map>
#include "CRingBuffer.h"
using namespace std;

class Session
{
public:
	Session(SOCKET sock, SOCKADDR_IN addr, UINT64 sessionID)
		: _sock(sock), _addr(addr), _recvBuf(10000), _sendBuf(10000), _sessionID(sessionID)
	{
		InitializeCriticalSection(&_cs);
	}

	~Session()
	{
		DeleteCriticalSection(&_cs);
	}

public:
	SOCKET _sock;
	SOCKADDR_IN _addr;

	CRingBuffer _recvBuf;
	CRingBuffer _sendBuf;

	WSAOVERLAPPED  _recvOvl;
	WSAOVERLAPPED  _sendOvl;

	CRITICAL_SECTION _cs;
	long _IOCount = 0;
	long _sendFlag = false;
	UINT64 _sessionID;
};
