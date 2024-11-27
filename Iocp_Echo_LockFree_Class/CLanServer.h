#pragma once
#include "Session.h"
#include "Protocol.h"

#include <ws2tcpip.h>
#include <process.h>
#include <unordered_map>
#pragma comment (lib, "winmm")
class CPacket;

class CLanServer
{
public:
	bool Start(const wchar_t* IP, short port, int numOfWorkerThreads, bool nagle, int sessionMax);
	void Stop();

	bool Disconnect(unsigned long long sessionID);
	bool SendPacket(unsigned long long sessionID, CPacket* pCPacket);

	virtual bool OnConnectionRequest(const wchar_t* IP, short Port) = 0; //< accept ����
	//return false; �� Ŭ���̾�Ʈ �ź�.
	//return true; �� ���� ���

	virtual bool OnAccept(/*Client ���� / SessionID / ��Ÿ���*/ unsigned long long sessionID) = 0; // < Accept �� ����ó�� �Ϸ� �� ȣ��.
	virtual void OnRelease(unsigned long long sessionID) = 0; // < Release �� ȣ��
	virtual bool OnRecv(unsigned long long sessionID, CPacket* packet) = 0; // < ��Ŷ ���� �Ϸ� ��
	//	virtual void OnSend(SessionID, int sendsize) = 0;           < ��Ŷ �۽� �Ϸ� ��
	//	virtual void OnWorkerThreadBegin() = 0;                    < ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
	//	virtual void OnWorkerThreadEnd() = 0;                      < ��Ŀ������ 1���� ���� ��
	virtual void OnError(int errorcode, WCHAR*) = 0;

	int GetSessionCount() { return _sessionCount; }
	int GetAcceptTPS() { return _acceptTPS; }
	int GetDiscounnectTPS() { return _disconnectTPS; }
	int GetRecvMessageTPS() { return _recvMessageTPS; }
	int GetSendMessageTPS() { return _acceptTPS; }

private:
	static unsigned int WINAPI AcceptThread(void* arg);
	static unsigned int WINAPI NetworkThread(void* arg);

	void ProcessRecvMessage(Session* pSession, int cbTransferred);

	bool RecvPost(Session* pSession);
	bool SendPost(Session* pSession);

	bool Release(unsigned long long sessionID);

private:
	Session* _sessionArray;
	SOCKET _listenSocket;
	short _sessionCount = 0;
	unsigned long long _sessionIDCount = 0;
	bool _bShutdown = false;
	HANDLE _iocpHandle;

private:
	wchar_t _IP[10];
	short _port;
	int _numOfWorkerThreads;
	bool _nagle;
	int _sessionMax;

private:
	int _acceptTotal = 0;
	int _acceptCount = 0;
	int _acceptTPS = 0;
	int _disconnectTotal = 0;
	//int _disconnectCount = 0;
	int _disconnectTPS = 0;
	int _recvMessageTPS = 0;
	int _sendMessageTPS = 0;

private:
	HANDLE _acceptThread;
	HANDLE* _networkThreads;
};
