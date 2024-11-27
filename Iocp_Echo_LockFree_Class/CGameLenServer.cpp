#include "CGameLenServer.h"
#include "SerializingBuffer.h"


bool CGameLenServer::OnConnectionRequest(const wchar_t* IP, short Port)
{
	return false;
}

bool CGameLenServer::OnAccept(unsigned long long sessionID)
{
	CPacket* packet = new CPacket();
	packet->AddRef();
	*packet << (short)8;
	*packet << 0x7fffffffffffffff;
	bool ret = SendPacket(sessionID, packet);
	packet->Release();
	return ret;
}

void CGameLenServer::OnRelease(unsigned long long sessionID)
{
}

bool CGameLenServer::OnRecv(unsigned long long sessionID, CPacket* pPacket)
{
	return SendPacket(sessionID, pPacket);
}

void CGameLenServer::OnError(int errorcode, WCHAR*)
{
}
