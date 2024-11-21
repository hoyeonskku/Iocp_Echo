#pragma once
#include "CLanServer.h"
class CGameLenServer : public CLanServer
{
public:
	virtual bool OnConnectionRequest(const wchar_t* IP, short Port); //< accept 직후
	//return false; 시 클라이언트 거부.
	//return true; 시 접속 허용

	virtual bool OnAccept(/*Client 정보 / SessionID / 기타등등*/unsigned long long sessionID) override ; // < Accept 후 접속처리 완료 후 호출.
	virtual void OnRelease(unsigned long long sessionID) override; // < Release 후 호출
	virtual bool OnRecv(unsigned long long sessionID, CPacket* pPacket) ; // < 패킷 수신 완료 후
	//	virtual void OnSend(SessionID, int sendsize) = 0;           < 패킷 송신 완료 후
	//	virtual void OnWorkerThreadBegin() = 0;                    < 워커스레드 GQCS 바로 하단에서 호출
	//	virtual void OnWorkerThreadEnd() = 0;                      < 워커스레드 1루프 종료 후
	virtual void OnError(int errorcode, WCHAR*) override;
};

