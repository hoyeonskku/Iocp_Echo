#pragma once
#pragma comment(lib, "ws2_32")
#include <ws2tcpip.h>
#include <unordered_map>
#include "CRingBuffer.h"
using namespace std;

class Session
{
public:
	Session(SOCKET sock, SOCKADDR_IN addr)
	{
		_sock = sock;
		_addr = addr;
		_recvBuf = CRingBuffer(10000);
		_sendBuf = CRingBuffer(10000);
		//ZeroMemory(&_recvOvl, sizeof(_recvOvl));
		//ZeroMemory(&_sendOvl, sizeof(_sendOvl));
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
	bool _deleteFlag = false;
};
