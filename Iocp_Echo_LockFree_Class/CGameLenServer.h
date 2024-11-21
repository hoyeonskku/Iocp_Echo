#pragma once
#include "CLanServer.h"
class CGameLenServer : public CLanServer
{
public:
	virtual bool OnConnectionRequest(const wchar_t* IP, short Port); //< accept ����
	//return false; �� Ŭ���̾�Ʈ �ź�.
	//return true; �� ���� ���

	virtual bool OnAccept(/*Client ���� / SessionID / ��Ÿ���*/unsigned long long sessionID) override ; // < Accept �� ����ó�� �Ϸ� �� ȣ��.
	virtual void OnRelease(unsigned long long sessionID) override; // < Release �� ȣ��
	virtual bool OnRecv(unsigned long long sessionID, CPacket* pPacket) ; // < ��Ŷ ���� �Ϸ� ��
	//	virtual void OnSend(SessionID, int sendsize) = 0;           < ��Ŷ �۽� �Ϸ� ��
	//	virtual void OnWorkerThreadBegin() = 0;                    < ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
	//	virtual void OnWorkerThreadEnd() = 0;                      < ��Ŀ������ 1���� ���� ��
	virtual void OnError(int errorcode, WCHAR*) override;
};

