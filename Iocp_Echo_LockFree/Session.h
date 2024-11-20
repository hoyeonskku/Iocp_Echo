#pragma once
#pragma comment(lib, "ws2_32")
#include <ws2tcpip.h>
#include <unordered_map>
#include "CRingBuffer.h"
using namespace std;


enum EventType
{
	RECV,
	RECVCOMPLETE,
	RECVFAIL,
	RECV0,
	SEND,
	SENDCOMPLETE,
	SENDFAIL,
	RELEASE,
	CLEAR,
 };

struct SessionLog
{
	unsigned long long sock;
    int ioCount;
    int eventType;
	int line;
    int transfered = -1;
};

class SessionLogQueue
{
private:
    SessionLog queue[100];
    volatile long rear;
    int capacity;
    int size;

public:
    // ������: ť�� ũ�⸦ �����ϰ� �ʱ�ȭ
    SessionLogQueue(int capacity = 100)
        : capacity(capacity), rear(0), size(0)
    {
    }

    // �Ҹ���: ť �޸� ����
    ~SessionLogQueue()
    {
        delete[] queue;
    }

    // ť�� �α� �߰�
    void enqueue(const SessionLog& log)
    {
		int index = InterlockedExchange(&rear, (rear + 1) % capacity);
        queue[index] = log;
    }
};

class Session
{
public:
	Session() : _recvBuf(2048), _sendBuf(2048), _invalidFlag(-1)
	{
		InitializeCriticalSection(&cs);
	}
	Session(SOCKET sock, SOCKADDR_IN addr, UINT64 sessionID)
		: _sock(sock), _addr(addr), _recvBuf(2048), _sendBuf(2048), _sessionID(sessionID), _invalidFlag(-1)
	{
	}

	~Session()
	{
	}

	void Clear(SOCKET sock, SOCKADDR_IN addr, UINT64 sessionID, USHORT index)
	{
		_sock = sock;
		if (_sock == INVALID_SOCKET)
			DebugBreak();


		_queue.enqueue({ _sock, _IOCount, EventType::CLEAR, __LINE__ });
		_addr = addr;
		//_IOCount = 0;
		_recvBuf.ClearBuffer();
		_sendBuf.ClearBuffer();
		_sendFlag = false;
		_sessionID = sessionID;    
		_sessionID &= 0x0000FFFFFFFFFFFF;  // ���� 6����Ʈ�� �״�� �ΰ�
		_sessionID |= static_cast<UINT64>(index) << 48;  // ���� 2����Ʈ�� index ���� ����
	}

public:
	SOCKET _sock;
	SOCKADDR_IN _addr;

	CRingBuffer _recvBuf;
	CRingBuffer _sendBuf;

	WSAOVERLAPPED  _recvOvl;
	WSAOVERLAPPED  _sendOvl;

	long _IOCount = 0;
	long _sendFlag = false;
	long _invalidFlag = false;
	UINT64 _sessionID;
	SessionLogQueue _queue;

	long _recvBytes = 0;
	long _sendReservedBytes = 0;
	CRITICAL_SECTION cs;
};
