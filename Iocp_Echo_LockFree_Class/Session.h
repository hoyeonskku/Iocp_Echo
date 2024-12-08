#pragma once
#pragma comment(lib, "ws2_32")
#include <ws2tcpip.h>
#include <unordered_map>
#include "CRingBuffer.h"
using namespace std;


enum EventType
{
	ACCEPT,
	ACCEPTSEND,
	ACCEPTSENDFAIL,
	ACCEPTRECVFAIL,
	RECV,
	RECVCOMPLETE,
	RECVFAIL,
	RECV0,
	SEND,
	SENDCOMPLETE,
	SENDFAIL,
	SENDFIRSTSIZE0,
	SENDFLAGNOTAQUIRED,
	SENDSECONDSIZE0,
	RELEASE,
	CLEAR,
 };

struct SessionLog
{
    int eventType;
	int index;
	unsigned long long sock;
	unsigned long threadID;
    int ioCount;
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
    SessionLogQueue(int capacity = 99)
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
#define RingbufferSize 10000
class Session
{
public:
	Session() : _recvBuf(RingbufferSize), _sendBuf(RingbufferSize), _invalidFlag(-1)
	{
	}
	Session(SOCKET sock, SOCKADDR_IN addr, UINT64 sessionID)
		: _sock(sock), _addr(addr), _recvBuf(RingbufferSize), _sendBuf(RingbufferSize), _sessionID(sessionID), _invalidFlag(-1)
	{
	}

	~Session()
	{
	}

	void Clear(SOCKET sock, SOCKADDR_IN addr, UINT64 sessionID, USHORT index)
	{
		_sock = sock;
		_toBeDeletedSock = sock;
		if (_sock == INVALID_SOCKET)
			DebugBreak();
		_addr = addr;
		//_IOCount = 0;
		_sendCount = 0;
		_recvBuf.ClearBuffer();
		_sendBuf.ClearBuffer();
		_sendFlag = false;
		_sessionID = sessionID;
		_sessionID &= 0x0000FFFFFFFFFFFF;  // ���� 6����Ʈ�� �״�� �ΰ�
		_sessionID |= static_cast<UINT64>(index) << 48;  // ���� 2����Ʈ�� index ���� ����

	}

public:
	SOCKET _sock;
	SOCKET _toBeDeletedSock;
	SOCKADDR_IN _addr;

	CRingBuffer _recvBuf;
	CRingPtrBuffer<CPacket*> _sendBuf;

	WSAOVERLAPPED  _recvOvl;
	WSAOVERLAPPED  _sendOvl;

	volatile long _sendCount;

	volatile long _IOCount = 0;
	long _sendFlag = false;
	long _invalidFlag = false;
	UINT64 _sessionID;

	long _recvBytes = 0;
	long _sendReservedBytes = 0;
	SessionLogQueue _sessionLogQueue;
};
