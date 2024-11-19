#pragma once
#define SERVERPORT 6000
#include <list>
#include <winsock2.h>
#include <assert.h>
#include <string>


class Session;
class CPacket;



void RecvPost(Session* pSession);
void SendPost(Session* pSession);

void OnRecv(UINT64 sessionID, CPacket* packet);
bool Release(UINT64 sessionID);
void SendPacket(UINT64 sessionID, CPacket* packet);

void ProcessRecvMessage(Session* pSession, int cbTransferred);

extern bool g_bShutdown;

extern SOCKET listenSocket;

extern 

class CLanServer
{
public:
	bool Start(...) ���� IP / ��Ʈ / ��Ŀ������ ��(������, ���׼�) / ���ۿɼ� / �ִ������� ��
	void Stop(...)
	int GetSessionCount(...)

	bool Disconnect(SessionID) / SESSION_ID
	bool SendPacket(SessionID, CPacket*) / SESSION_ID

	virtual bool OnConnectionRequest(IP, Port) = 0; //< accept ����
	//return false; �� Ŭ���̾�Ʈ �ź�.
	//return true; �� ���� ���

	virtual void	OnAccept(Client ���� / SessionID / ��Ÿ���) = 0; // < Accept �� ����ó�� �Ϸ� �� ȣ��.

	virtual void 	OnRelease(UINT64 SessionID) = 0; // < Release �� ȣ��

	virtual void 	OnRecv(UINT64 SessionID, CPacket* packet) = 0; // < ��Ŷ ���� �Ϸ� ��

	//	virtual void OnSend(SessionID, int sendsize) = 0;           < ��Ŷ �۽� �Ϸ� ��

	//	virtual void OnWorkerThreadBegin() = 0;                    < ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
	//	virtual void OnWorkerThreadEnd() = 0;                      < ��Ŀ������ 1���� ���� ��

	virtual void OnError(int errorcode, WCHAR*) = 0;

	int getAcceptTPS();
	int getRecvMessageTPS();
	int getSendMessageTPS();

	static unsigned int WINAPI AcceptThread(void* arg);
	static unsigned int WINAPI NetworkThread(void* arg);

public:
	Session g_SessionArray[65535];
	bool g_bShutdown;
	SOCKET listenSocket;
};

class CLanClient
{
	bool Connect()		//	���ε� IP, ����IP / ��Ŀ������ �� / ���ۿɼ�
	{}
	bool Disconnect()
	{}
	bool SendPacket(CPacket*)
	{}

	virtual void OnEnterJoinServer() = 0; //< �������� ���� ���� ��
	virtual void OnLeaveServer() = 0; //< �������� ������ �������� ��

	virtual void OnRecv(CPacket* packet) = 0; //< �ϳ��� ��Ŷ ���� �Ϸ� ��
	virtual void OnSend(int sendsize) = 0; //< ��Ŷ �۽� �Ϸ� ��

	//	virtual void OnWorkerThreadBegin() = 0;
	//	virtual void OnWorkerThreadEnd() = 0;

	virtual void OnError(int errorcode, WCHAR* wchar) = 0;

};


