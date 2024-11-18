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
Session g_SessionArray[65535];

unsigned int WINAPI AcceptThread(void* arg)
{
	HANDLE hrd = *(HANDLE*)arg;
	int IoctlsocketRetval;

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

		Session* session = nullptr;

		for (int i = 0; i < 65535; i++)
		{
			if (g_SessionArray[i]._invalidFlag == -1 && InterlockedExchange(&g_SessionArray[i]._invalidFlag, 1) == -1)
			{
				session = &g_SessionArray[i];
				session->Clear(client_sock, clientaddr, sessionIDCount++, i);
				break;
			}
		}

		CreateIoCompletionPort((HANDLE)client_sock, hrd, (ULONG_PTR)session, 0);

		RecvPost(session);
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

		if (cbTransferred == 0 && pSession == 0 && ovl == 0)
			return 0;

		if (cbTransferred == 0)
		{
		}

		else if (&pSession->_recvOvl == ovl)
		{
			ProcessRecvMessage(pSession, cbTransferred);


		}
		else if (&pSession->_sendOvl == ovl)
		{
			pSession->_sendBuf.MoveFront(cbTransferred);
			InterlockedExchange(&pSession->_sendFlag, 0);

			if (pSession->_sendBuf.GetBufferSize() > 0)
				SendPost(pSession);


		}

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


	if (wsabufs[0].len + wsabufs[1].len == 0)
		DebugBreak();

	int ioCount = InterlockedIncrement(&pSession->_IOCount);
	DWORD wsaRecvRetval = WSARecv(pSession->_sock, wsabufs, 2, NULL, &flags, &pSession->_recvOvl, NULL);
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

	// send 0�� �Ǿ �Ϸ������� �ȿ�, �ٸ����� �̹� send �� �����.
	if (pSession->_sendBuf.GetUseSize() == 0)
	{
		InterlockedExchange(&pSession->_sendFlag, 0);
		return;
	}
	WSABUF wsabufs[2];

	pSession->_sendBuf.SetSendWsabufs(wsabufs);

	if (wsabufs[0].len + wsabufs[1].len == 0)
		DebugBreak();

	ZeroMemory(&pSession->_sendOvl, sizeof(pSession->_sendOvl));
	InterlockedIncrement(&pSession->_IOCount);
	DWORD wsaSendRetval = WSASend(pSession->_sock, wsabufs, 2, NULL, 0, &pSession->_sendOvl, NULL);

	if (wsabufs[0].len + wsabufs[1].len == 0)
		DebugBreak();

	if (wsaSendRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			if (err == 10054) {}
			else if (err == 10053) {}
			else DebugBreak();
			InterlockedExchange(&pSession->_sendFlag, 0);
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
		// �����ŭ ���� �� ������
		short num;
		int ret = pSession->_recvBuf.Peek((char*)&num, sizeof(short));
		short size = static_cast<short>(num);
		if (pSession->_recvBuf.GetUseSize() < size + sizeof(short))
			break;
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
	}

	RecvPost(pSession);
}

void OnRecv(UINT64 sessionID, CPacket* packet)
{
	SendPacket(sessionID, packet);
}

bool Release(UINT64 sessionID)
{
	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);
	g_SessionArray[index]._invalidFlag = -1;
	return true;
}

void SendPacket(UINT64 sessionID, CPacket* packet)
{
	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);
	short size;
	*packet >> size;
	UINT64 data;
	*packet >> data;

	g_SessionArray[index]._sendBuf.Enqueue((const char*)&size, sizeof(short));
	g_SessionArray[index]._sendBuf.Enqueue((const char*)&data, size);

	if (g_SessionArray[index]._sendBuf.GetBufferSize() > 0)
		SendPost(&g_SessionArray[index]);
}
