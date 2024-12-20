#include "NetLib.h"
#pragma comment(lib, "ws2_32")
#include <Windows.h>
#include <winsock2.h>
#include "Session.h"
#include "SerializingBuffer.h"
#include "unordered_map"
#include "Protocol.h"

bool g_bShutdown = true;
SOCKET listenSocket;
UINT64 sessionIDCount;
CRITICAL_SECTION g_sessionMapCs;
std::unordered_map<int, Session*> g_sessionMap;

unsigned int WINAPI AcceptThread(void* arg)
{
	HANDLE hrd = *(HANDLE*)arg;
	int IoctlsocketRetval;

	InitializeCriticalSection(&g_sessionMapCs);
	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	DWORD recvbytes, flags;
	int retval;

	while (1)
	{
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listenSocket, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			int ret = WSAGetLastError();
			if (ret != 10038 && ret != 10004)
				DebugBreak();
			break;
		}

		Session* pSession = new Session(client_sock, clientaddr, sessionIDCount++);

		EnterCriticalSection(&g_sessionMapCs);
		g_sessionMap.insert({ pSession->_sessionID, pSession });
		LeaveCriticalSection(&g_sessionMapCs);


		CreateIoCompletionPort((HANDLE)client_sock, hrd, (ULONG_PTR)pSession, 0);

		RecvPost(pSession);
	}
	return 0;
}

unsigned int WINAPI NetworkThread(void* arg)
{
	int retval;
	HANDLE hcp = *(HANDLE*)arg;

	while (1)
	{
		// 비동기 입출력 완료 기다리기
		DWORD cbTransferred;
		SOCKET* client_sock;
		Session* pSession;
		WSAOVERLAPPED* ovl;
		WSABUF wsabuf;

		retval = GetQueuedCompletionStatus(hcp, &cbTransferred,
			(PULONG_PTR)&pSession, &ovl, INFINITE);
		EnterCriticalSection(&pSession->_cs);
		if (cbTransferred == 0 && pSession == 0 && ovl == 0)
		{
			LeaveCriticalSection(&pSession->_cs);
			return 0;
		}

		if (cbTransferred == 0) {}

		else if (&pSession->_recvOvl == ovl)
			ProcessRecvMessage(pSession, cbTransferred);
		else if (&pSession->_sendOvl == ovl)
		{
			pSession->_sendBuf.MoveFront(cbTransferred);
			InterlockedExchange(&pSession->_sendFlag, 0);

			if (pSession->_sendBuf.GetBufferSize() > 0)
				SendPost(pSession);
		}
		LeaveCriticalSection(&pSession->_cs);
		if (InterlockedDecrement(&pSession->_IOCount) == 0)
		{
			Release(pSession->_sessionID);
		}
	}
	return 0;
}

void RecvPost(Session* pSession)
{
	WSABUF wsabufs[2];
	pSession->_recvBuf.SetRecvWsabufs(wsabufs);

	DWORD flags = 0;
	ZeroMemory(&pSession->_recvOvl, sizeof(pSession->_recvOvl));
	int ioCount = InterlockedIncrement(&pSession->_IOCount);

	DWORD recvedBytes;
	DWORD wsaRecvRetval = WSARecv(pSession->_sock, wsabufs, 2, &recvedBytes, &flags, &pSession->_recvOvl, NULL);
	if (wsaRecvRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			if (err == 10054) {}
			else if (err == 10053) {}
			else DebugBreak();
			InterlockedDecrement(&pSession->_IOCount);
			return;
		}
	}
}

void SendPost(Session* pSession)
{
	if (InterlockedExchange(&pSession->_sendFlag, 1) == 1)
		return;

	// send 0이 되어서 완료통지가 안옴, 다른쪽이 이미 send 한 경우임.
	if (pSession->_sendBuf.GetUseSize() == 0)
	{
		InterlockedExchange(&pSession->_sendFlag, 0);
		return;
	}
	WSABUF wsabufs[2];

	pSession->_sendBuf.SetSendWsabufs(wsabufs);

	ZeroMemory(&pSession->_sendOvl, sizeof(pSession->_sendOvl));
	InterlockedIncrement(&pSession->_IOCount);
	DWORD wsaSendRetval = WSASend(pSession->_sock, wsabufs, 2, NULL, 0, &pSession->_sendOvl, NULL);

	if (wsaSendRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			if (err == 10054) {}
			else if (err == 10053) {}
			else DebugBreak();
			InterlockedDecrement(&pSession->_IOCount);
			return;
		}
	}
}

void ProcessRecvMessage(Session* pSession, int cbTransferred)
{
	pSession->_recvBuf.MoveRear(cbTransferred);
	while (pSession->_recvBuf.GetUseSize() >= sizeof(stHeader))
	{
		// 헤더만큼 읽을 수 있으면
		short num;
		int ret = pSession->_recvBuf.Peek((char*)&num, sizeof(short));
		short size = static_cast<short>(num);
		if (pSession->_recvBuf.GetUseSize() < size + sizeof(short))
			break;
		pSession->_recvBuf.MoveFront(sizeof(short));

		CPacket packetData;
		packetData << size;

		char* writePos = packetData.GetBufferPtr() + sizeof(short);
		// 사이즈보다 경계까지의 값이 작다면
		if (pSession->_recvBuf.DirectDequeueSize() < size)
		{
			int freeSize = pSession->_recvBuf.DirectDequeueSize();
			int remainLength = size - freeSize;
			memcpy(writePos, pSession->_recvBuf.GetFrontBufferPtr(), freeSize);
			pSession->_recvBuf.MoveFront(freeSize);

			memcpy(writePos + freeSize, pSession->_recvBuf.GetFrontBufferPtr(), remainLength);
			pSession->_recvBuf.MoveFront(remainLength);
		}
		// 충분히 읽을 수 있으면
		else
		{
			memcpy(writePos, pSession->_recvBuf.GetFrontBufferPtr(), size);
			pSession->_recvBuf.MoveFront(size);
		}
		packetData.MoveWritePos(size);
		OnRecv(pSession->_sessionID, &packetData);
	}

	RecvPost(pSession);
}

void OnRecv(UINT64 sessionID, CPacket* packet)
{
	EnterCriticalSection(&g_sessionMapCs);
	auto it = g_sessionMap.find(sessionID);
	LeaveCriticalSection(&g_sessionMapCs);
	if (it != g_sessionMap.end())
		SendPacket(sessionID, packet);
	else
		DebugBreak();

}

bool Release(UINT64 sessionID)
{
	EnterCriticalSection(&g_sessionMapCs);
	auto it = g_sessionMap.find(sessionID);
	if (it != g_sessionMap.end())
	{
		// 샌드패킷에서 세션락을 풀어주는 상태에서 세션락을 해제하면 아래 코드 실행 가능해짐, 그 이후 세션맵 락을 잡고있는 상태이기 때문에 샌드패킷하는 스레드가 없다는 것을 확신할 수 있음.
		EnterCriticalSection(&it->second->_cs);
		LeaveCriticalSection(&it->second->_cs);
		closesocket(it->second->_sock);
		delete it->second;
		g_sessionMap.erase(it);
		LeaveCriticalSection(&g_sessionMapCs);
		return true;
	}
	LeaveCriticalSection(&g_sessionMapCs);
	return false;
}



void SendPacket(UINT64 sessionID, CPacket* packet)
{
	EnterCriticalSection(&g_sessionMapCs);
	auto it = g_sessionMap.find(sessionID);
	// 세션 맵 락을 이용하여 먼저 세션락을 획득, 그 이후 맵 락을 풀어줘서 세션락만 획득한 상태를 유도,
	Session* session = it->second;
	EnterCriticalSection(&session->_cs);
	LeaveCriticalSection(&g_sessionMapCs);
	short size;
	*packet >> size;
	UINT64 data;
	*packet >> data;


	session->_sendBuf.Enqueue((const char*)&size, sizeof(short));
	session->_sendBuf.Enqueue((const char*)&data, size);

	if (session->_sendBuf.GetBufferSize() > 0)
		SendPost(it->second);
	LeaveCriticalSection(&session->_cs);
}