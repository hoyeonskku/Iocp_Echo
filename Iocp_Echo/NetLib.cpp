#define _WINSOCK_DEPRECATED_NO_WARNINGS // �ֽ� VC++ ������ �� ��� ����
#include "NetLib.h"
#pragma comment(lib, "ws2_32")
#include <Windows.h>
#include <winsock2.h>
#include "Session.h"
#include "SerializingBuffer.h"
#include "unordered_map"

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
		printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

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
		EnterCriticalSection(&pSession->_cs);

		if (cbTransferred == 0)
		{

		}
		else if (&pSession->_recvOvl == ovl)
		{
			// ������ ����
			{
				pSession->_recvBuf.MoveRear(cbTransferred);
				while (ProcessRecvMessage(pSession))
				{


				}

				/*pSession->_sendBuf.Enqueue(wsabufs[0].buf, wsabufs[0].len);
				pSession->_sendBuf.Enqueue(wsabufs[1].buf, wsabufs[1].len);*/

				pSession->_recvBuf.MoveFront(cbTransferred);
			}
			if (InterlockedExchange(&pSession->_sendFlag, 1) == 0)
				SendPost(pSession);
			RecvPost(pSession);
		}
		else if (&pSession->_sendOvl == ovl)
		{

			pSession->_sendBuf.MoveFront(cbTransferred);
			if (pSession->_sendBuf.GetUseSize() > 0)
				SendPost(pSession);
			else
				pSession->_sendFlag = false;
		}

		LeaveCriticalSection(&pSession->_cs);

		if (InterlockedDecrement(&pSession->_IOCount) == 0)
		{
			printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
				inet_ntoa(pSession->_addr.sin_addr), ntohs(pSession->_addr.sin_port));


			Disconnect(pSession);
		}
	}
	return 0;
}

void Disconnect(Session* pSession)
{
	closesocket(pSession->_sock);
	delete pSession;
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
			InterlockedDecrement(&pSession->_IOCount);
			//DebugBreak();
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
			InterlockedDecrement(&pSession->_IOCount);
			//DebugBreak();
			return;
		}
	}
}

bool ProcessRecvMessage(Session* pSession)
{
	// �����ŭ ���� �� ������
	if (pSession->_recvBuf.GetUseSize() >= sizeof(char))
	{
		char num;
		int ret = pSession->_recvBuf.Peek((char*)&num, sizeof(char));
		int size = num - '0';
		if (pSession->_recvBuf.GetUseSize() < size + sizeof(char))
			return false;
		pSession->_recvBuf.MoveFront(sizeof(char));


		char* packetData = new char[100];

		// ������� �������� ���� �۴ٸ�
		if (pSession->_recvBuf.DirectDequeueSize() < size)
		{
			int freeSize = pSession->_recvBuf.DirectDequeueSize();
			int remainLength = size - freeSize;
			memcpy(packetData, pSession->_recvBuf.GetFrontBufferPtr(), freeSize);
			pSession->_recvBuf.MoveFront(freeSize);

			memcpy((packetData)+freeSize, pSession->_recvBuf.GetFrontBufferPtr(), remainLength);
			pSession->_recvBuf.MoveFront(remainLength);
		}
		// ����� ���� �� ������
		else
		{
			memcpy(packetData, pSession->_recvBuf.GetFrontBufferPtr(), size);
			pSession->_recvBuf.MoveFront(size);
		}

		OnRecv(pSession, packetData, size);

		delete[] packetData;
		return true;
	}
}
void OnRecv(Session* pSession, char* packet, int size)
{

	char ip[INET_ADDRSTRLEN]; // IPv4 �ּҸ� ������ ����
	// �����ϰ� IP �ּ� ��ȯ (inet_ntop ���)
	if (inet_ntop(AF_INET, &(pSession->_addr.sin_addr), ip, sizeof(ip)) == nullptr) {
		std::cerr << "Failed to convert IP address" << std::endl;
		return;
	}
	printf("[TCP/%s:%d] %.*s\n",
		ip,
		ntohs(pSession->_addr.sin_port),
		size, packet);
	SendPacket(pSession, packet, size);
}

void SendPacket(Session* pSession, char* packet, int size)
{
	pSession->_sendBuf.Enqueue(packet, size);
}
