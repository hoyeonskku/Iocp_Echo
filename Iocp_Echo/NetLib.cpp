#define _WINSOCK_DEPRECATED_NO_WARNINGS // �ֽ� VC++ ������ �� ��� ����
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
	// ������ ��ſ� ����� ����
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
		/*printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));*/

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
		// �񵿱� ����� �Ϸ� ��ٸ���
		DWORD cbTransferred;
		SOCKET* client_sock;
		Session* pSession;
		WSAOVERLAPPED* ovl;
		WSABUF wsabuf;

		retval = GetQueuedCompletionStatus(hcp, &cbTransferred,
			(PULONG_PTR)&pSession, &ovl, INFINITE);

		if (cbTransferred == 0 && pSession  == 0 && ovl == 0)
			return 0;
		// ���� �� ȹ��
		EnterCriticalSection(&pSession->_cs);

		if (cbTransferred == 0)
		{

		}
		else if (&pSession->_recvOvl == ovl)
		{
			// ������ ����
			pSession->_recvBuf.MoveRear(cbTransferred);
			while (ProcessRecvMessage(pSession))
			{

			}
			// ������ send�� �ϱ� ���� ���� ť�� ���� ��(1������), �� ������ send�� �Ϸ��ϸ� �� ���¿��� send�ϰ� �ȴ�(2������). �׷��� �ٽ� �ѹ� ������ ���� �� ������ üũ(3������)
			if (pSession->_sendBuf.GetUseSize() > 0 && InterlockedExchange(&pSession->_sendFlag, 1) == 0 && pSession->_sendBuf.GetUseSize() > 0)
				SendPost(pSession);
			RecvPost(pSession);
		}
		else if (&pSession->_sendOvl == ovl)
		{
			pSession->_sendBuf.MoveFront(cbTransferred);
			pSession->_sendFlag = false;
			// ������ send�� �ϱ� ���� ���� ť�� ���� ��(1������), �� ������ send�� �Ϸ��ϸ� �� ���¿��� send�ϰ� �ȴ�. �׷��� �ٽ� �ѹ� ������ ���� �� ������ üũ(3������)
			if (pSession->_sendBuf.GetUseSize() > 0 && InterlockedExchange(&pSession->_sendFlag, 1) == 0 && pSession->_sendBuf.GetUseSize() > 0)
				SendPost(pSession);
		}

		if (InterlockedDecrement(&pSession->_IOCount) == 0)
		{
			Release(pSession->_sessionID);
		}

		// ���� �� ����
		LeaveCriticalSection(&pSession->_cs);
	}
	return 0;
}



void RecvPost(Session* pSession)
{
	WSABUF wsabufs[2];

	pSession->_recvBuf.SetRecvWsabufs(wsabufs);

	DWORD flags = 0;
	ZeroMemory(&pSession->_recvOvl, sizeof(pSession->_recvOvl));
	InterlockedIncrement(&pSession->_IOCount);
	DWORD wsaSendRetval = WSARecv(pSession->_sock, wsabufs, 2, NULL, &flags, &pSession->_recvOvl, NULL);
	if (wsaSendRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			if (err == 10054)
			{

			}
			else if (err == 10053)
			{

			}
			else
			{
				DebugBreak();
			}
			InterlockedDecrement(&pSession->_IOCount);
			return;
		}
	}
}

void SendPost(Session* pSession)
{
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
			if (err == 10054)
			{

			}
			else if (err == 10053)
			{

			}
			else
			{
				DebugBreak();
			}
			InterlockedDecrement(&pSession->_IOCount);
			
			return;
		}
	}
}

bool ProcessRecvMessage(Session* pSession)
{
	// �����ŭ ���� �� ������
	if (pSession->_recvBuf.GetUseSize() >= sizeof(stHeader))
	{
		short num;
		int ret = pSession->_recvBuf.Peek((char*)&num, sizeof(short));
		short size = static_cast<short>(num);
		if (pSession->_recvBuf.GetUseSize() < size + sizeof(short))
			return false;
		pSession->_recvBuf.MoveFront(sizeof(short));

		CPacket packetData;
		packetData << size;

		char* writePos = packetData.GetBufferPtr() + sizeof(short);
		// ������� �������� ���� �۴ٸ�
		if (pSession->_recvBuf.DirectDequeueSize() < size)
		{
			int freeSize = pSession->_recvBuf.DirectDequeueSize();
			int remainLength = size - freeSize;
			memcpy(writePos, pSession->_recvBuf.GetFrontBufferPtr(), freeSize);
			pSession->_recvBuf.MoveFront(freeSize);

			memcpy(writePos + freeSize, pSession->_recvBuf.GetFrontBufferPtr(), remainLength);
			pSession->_recvBuf.MoveFront(remainLength);
		}
		// ����� ���� �� ������
		else
		{
			memcpy(writePos, pSession->_recvBuf.GetFrontBufferPtr(), size);
			pSession->_recvBuf.MoveFront(size);

		}

		packetData.MoveWritePos(size);
		OnRecv(pSession->_sessionID, &packetData);

		return true;
	}
}
void OnRecv(UINT64 sessionID, CPacket* packet)
{
	//char ip[INET_ADDRSTRLEN]; // IPv4 �ּҸ� ������ ����
	// �����ϰ� IP �ּ� ��ȯ (inet_ntop ���)
	/*if (inet_ntop(AF_INET, &(pSession->_addr.sin_addr), ip, sizeof(ip)) == nullptr) {
		std::cerr << "Failed to convert IP address" << std::endl;
		return;
	}
	printf("[TCP/%s:%d] %.*s\n",
		ip,
		ntohs(pSession->_addr.sin_port),
		packet->GetDataSize(), packet);*/
	auto it = g_sessionMap.find(sessionID);
	if (it != g_sessionMap.end())
	{
		SendPacket(sessionID, packet);
	}
	else
	{
		DebugBreak();
	}
}

bool Release(UINT64 sessionID)
{
	EnterCriticalSection(&g_sessionMapCs);
	auto it = g_sessionMap.find(sessionID);
	if (it != g_sessionMap.end()) 
	{
		// ������Ŷ���� ���Ƕ��� Ǯ���ִ� ���¿��� ���Ƕ��� �����ϸ� �Ʒ� �ڵ� ���� ��������, �� ���� ���Ǹ� ���� ����ִ� �����̱� ������ ������Ŷ�ϴ� �����尡 ���ٴ� ���� Ȯ���� �� ����.
		EnterCriticalSection(&it->second->_cs);
		LeaveCriticalSection(&it->second->_cs);
		/*printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			inet_ntoa(it->second->_addr.sin_addr), ntohs(it->second->_addr.sin_port));*/
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
	// ���� �� ���� �̿��Ͽ� ���� ���Ƕ��� ȹ��, �� ���� �� ���� Ǯ���༭ ���Ƕ��� ȹ���� ���¸� ����,
	EnterCriticalSection(&it->second->_cs);
	LeaveCriticalSection(&g_sessionMapCs);
	short size;
	*packet >> size;
	UINT64 data;
	*packet >> data;

	it->second->_sendBuf.Enqueue((const char*)&size, sizeof(short));
	it->second->_sendBuf.Enqueue((const char*)&data, size);
	LeaveCriticalSection(&it->second->_cs);
}
