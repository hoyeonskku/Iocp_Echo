#pragma once
#include "WinSock2.h"
class CPacket;

class CLanClient
{
private:
	bool Connect(const wchar_t* bindIP, const wchar_t* serverIP, short serverPort, int numOfWorkerThreads, bool nagle);	//바인딩 IP, 서버IP / 워커스레드 수 / 나글옵션
	bool Disconnect();
	bool SendPacket(CPacket* pPacket);

	virtual void OnEnterJoinServer() = 0; //< 서버와의 연결 성공 후
	virtual void OnLeaveServer() = 0; //< 서버와의 연결이 끊어졌을 때

	virtual void OnRecv(CPacket* pPacket) = 0; //< 하나의 패킷 수신 완료 후
	virtual void OnSend(int sendsize) = 0; //< 패킷 송신 완료 후

	//	virtual void OnWorkerThreadBegin() = 0;
	//	virtual void OnWorkerThreadEnd() = 0;

	virtual void OnError(int errorcode, wchar_t* message) = 0;

private:
	SOCKET _clientSocket;
	bool _bShutdown;

};

