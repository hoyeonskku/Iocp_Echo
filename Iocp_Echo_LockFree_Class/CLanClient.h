#pragma once
#include "WinSock2.h"
class CPacket;

class CLanClient
{
private:
	bool Connect(const wchar_t* bindIP, const wchar_t* serverIP, short serverPort, int numOfWorkerThreads, bool nagle);	//���ε� IP, ����IP / ��Ŀ������ �� / ���ۿɼ�
	bool Disconnect();
	bool SendPacket(CPacket* pPacket);

	virtual void OnEnterJoinServer() = 0; //< �������� ���� ���� ��
	virtual void OnLeaveServer() = 0; //< �������� ������ �������� ��

	virtual void OnRecv(CPacket* pPacket) = 0; //< �ϳ��� ��Ŷ ���� �Ϸ� ��
	virtual void OnSend(int sendsize) = 0; //< ��Ŷ �۽� �Ϸ� ��

	//	virtual void OnWorkerThreadBegin() = 0;
	//	virtual void OnWorkerThreadEnd() = 0;

	virtual void OnError(int errorcode, wchar_t* message) = 0;

private:
	SOCKET _clientSocket;
	bool _bShutdown;

};

