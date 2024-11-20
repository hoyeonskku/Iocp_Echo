#include "CGameLenServer.h"

bool CGameLenServer::OnConnectionRequest(const wchar_t* IP, short Port)
{
	return false;
}

void CGameLenServer::OnAccept()
{
}

void CGameLenServer::OnRelease(unsigned long long sessionID)
{
}

void CGameLenServer::OnRecv(unsigned long long sessionID, CPacket* pPacket)
{

	SendPacket(sessionID, pPacket);
}

void CGameLenServer::OnError(int errorcode, WCHAR*)
{
}
