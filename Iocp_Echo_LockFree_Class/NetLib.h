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
	bool Start(...) 오픈 IP / 포트 / 워커스레드 수(생성수, 러닝수) / 나글옵션 / 최대접속자 수
	void Stop(...)
	int GetSessionCount(...)

	bool Disconnect(SessionID) / SESSION_ID
	bool SendPacket(SessionID, CPacket*) / SESSION_ID

	virtual bool OnConnectionRequest(IP, Port) = 0; //< accept 직후
	//return false; 시 클라이언트 거부.
	//return true; 시 접속 허용

	virtual void	OnAccept(Client 정보 / SessionID / 기타등등) = 0; // < Accept 후 접속처리 완료 후 호출.

	virtual void 	OnRelease(UINT64 SessionID) = 0; // < Release 후 호출

	virtual void 	OnRecv(UINT64 SessionID, CPacket* packet) = 0; // < 패킷 수신 완료 후

	//	virtual void OnSend(SessionID, int sendsize) = 0;           < 패킷 송신 완료 후

	//	virtual void OnWorkerThreadBegin() = 0;                    < 워커스레드 GQCS 바로 하단에서 호출
	//	virtual void OnWorkerThreadEnd() = 0;                      < 워커스레드 1루프 종료 후

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
	bool Connect()		//	바인딩 IP, 서버IP / 워커스레드 수 / 나글옵션
	{}
	bool Disconnect()
	{}
	bool SendPacket(CPacket*)
	{}

	virtual void OnEnterJoinServer() = 0; //< 서버와의 연결 성공 후
	virtual void OnLeaveServer() = 0; //< 서버와의 연결이 끊어졌을 때

	virtual void OnRecv(CPacket* packet) = 0; //< 하나의 패킷 수신 완료 후
	virtual void OnSend(int sendsize) = 0; //< 패킷 송신 완료 후

	//	virtual void OnWorkerThreadBegin() = 0;
	//	virtual void OnWorkerThreadEnd() = 0;

	virtual void OnError(int errorcode, WCHAR* wchar) = 0;

};


