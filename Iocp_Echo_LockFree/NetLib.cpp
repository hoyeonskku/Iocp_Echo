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
Session g_SessionArray[10000];

unsigned int WINAPI AcceptThread(void* arg)
{
	HANDLE hrd = *(HANDLE*)arg;
	int IoctlsocketRetval;

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

		Session* session = nullptr;

		for (int i = 0; i < 10000; i++)
		{
			if (/*g_SessionArray[i]._invalidFlag == -1 &&*/ InterlockedExchange(&g_SessionArray[i]._invalidFlag, 1) == -1)
			{
				session = &g_SessionArray[i];
				session->Clear(client_sock, clientaddr, sessionIDCount++, i);
				break;
			}
			if (i == 9999)
				DebugBreak();
		}

		CreateIoCompletionPort((HANDLE)client_sock, hrd, (ULONG_PTR)session, 0);

		if (session->_sock == INVALID_SOCKET)
			DebugBreak();

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
		// 비동기 입출력 완료 기다리기
		DWORD cbTransferred;
		SOCKET* client_sock;
		Session* pSession;
		WSAOVERLAPPED* ovl;
		WSABUF wsabuf;

		retval = GetQueuedCompletionStatus(hcp, &cbTransferred,
			(PULONG_PTR)&pSession, &ovl, INFINITE);

		if (pSession->_sock == INVALID_SOCKET)
			DebugBreak();

		if (cbTransferred == 0 && pSession == 0 && ovl == 0)
			return 0;

		if (cbTransferred == 0)
		{
			pSession->_queue.enqueue({ pSession->_sock, EventType::RECV0,GetCurrentThreadId(), pSession->_IOCount,   __LINE__ , (int) cbTransferred});
		}

		else if (&pSession->_recvOvl == ovl)
		{
			pSession->_queue.enqueue({ pSession->_sock, EventType::RECVCOMPLETE, GetCurrentThreadId(), pSession->_IOCount,  __LINE__ ,(int)cbTransferred });
			ProcessRecvMessage(pSession, cbTransferred);

		}
		else if (&pSession->_sendOvl == ovl)
		{
			pSession->_queue.enqueue({ pSession->_sock,   EventType::SENDCOMPLETE,GetCurrentThreadId(),pSession->_IOCount,   __LINE__ ,(int)cbTransferred });
			pSession->_sendBuf.MoveFront(cbTransferred);
			// 여기서 풀어주는 이유는 사이즈를 보고 보낼 게 없을 때 풀어주게 되면 그 사이에 인큐를 해버리는 스레드가 있을 수 있어서 아무도 send를 하지 않게 됨
			InterlockedExchange(&pSession->_sendFlag, 0);

			if (pSession->_sendBuf.GetBufferSize() > 0)
			{
				SendPost(pSession);
			}
			else
			{
				pSession->_queue.enqueue({ pSession->_sock,   EventType::SENDFIRSTSIZE0,GetCurrentThreadId(),pSession->_IOCount,   __LINE__ ,(int)cbTransferred });
			}
		}

		if (InterlockedDecrement(&pSession->_IOCount) == 0)
		{
			pSession->_queue.enqueue({ pSession->_sock,EventType::RELEASE,  GetCurrentThreadId(),pSession->_IOCount,  __LINE__ ,(int) cbTransferred });
			Release(pSession->_sessionID);
		}
	}
	return 0;
}

void RecvPost(Session* pSession)
{
	WSABUF wsabufs[2];
	unsigned int bufsNum = pSession->_recvBuf.SetRecvWsabufs(wsabufs);

	if (wsabufs[0].len + wsabufs[1].len == 0)
		DebugBreak();
	DWORD flags = 0;
	ZeroMemory(&pSession->_recvOvl, sizeof(pSession->_recvOvl));

	InterlockedIncrement(&pSession->_IOCount);
	pSession->_queue.enqueue({ pSession->_sock, EventType::RECV, GetCurrentThreadId(),pSession->_IOCount,  __LINE__ });
	if (pSession->_sock == INVALID_SOCKET)
		DebugBreak();
	DWORD wsaRecvRetval = WSARecv(pSession->_sock, wsabufs, bufsNum, NULL, &flags, &pSession->_recvOvl, NULL);
	if (wsaRecvRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			if (err == 10054) {}
			else if (err == 10053) {}
			else DebugBreak();
			InterlockedDecrement(&pSession->_IOCount);
				pSession->_queue.enqueue({ pSession->_sock,pSession->_IOCount, GetCurrentThreadId(), EventType::RECVFAIL, __LINE__ });
			return;
		}
	}
}

void SendPost(Session* pSession)
{
	if (InterlockedExchange(&pSession->_sendFlag, 1) == 1)
	{
		pSession->_queue.enqueue({ pSession->_sock, EventType::SENDFLAGNOTAQUIRED,GetCurrentThreadId(),pSession->_IOCount,  __LINE__ , pSession->_sendFlag });
		return;
	}

	if (pSession->_sendBuf.GetUseSize() == 0)
	{
		pSession->_queue.enqueue({ pSession->_sock ,EventType::SENDSECONDSIZE0,GetCurrentThreadId(),0, __LINE__ , pSession->_sendFlag });
		InterlockedExchange(&pSession->_sendFlag, 0);
		
		return;
	}
	WSABUF wsabufs[2];

	unsigned int bufsNum = pSession->_sendBuf.SetSendWsabufs(wsabufs);

	if (wsabufs[0].len + wsabufs[1].len == 0)
		DebugBreak();


	ZeroMemory(&pSession->_sendOvl, sizeof(pSession->_sendOvl));
	InterlockedIncrement(&pSession->_IOCount);
	pSession->_queue.enqueue({ pSession->_sock, EventType::SEND,GetCurrentThreadId(),pSession->_IOCount,  __LINE__ ,(int)(wsabufs[0].len + wsabufs[1].len) });
	if (pSession->_sock == INVALID_SOCKET)
		DebugBreak();
	DWORD wsaSendRetval = WSASend(pSession->_sock, wsabufs, bufsNum, NULL, 0, &pSession->_sendOvl, NULL);

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
			pSession->_queue.enqueue({ pSession->_sock,   EventType::SENDFAIL,GetCurrentThreadId(), pSession->_IOCount,__LINE__, (int)(wsabufs[0].len + wsabufs[1].len) });
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
	SendPacket(sessionID, packet);
}

bool Release(UINT64 sessionID)
{
	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);

	Session* session = &g_SessionArray[index];

	closesocket(g_SessionArray[index]._sock);
	g_SessionArray[index]._sock = INVALID_SOCKET;
	//g_SessionArray[index]._invalidFlag = -1;
	InterlockedExchange(&g_SessionArray[index]._invalidFlag, -1);
	return true;
}

void SendPacket(UINT64 sessionID, CPacket* packet)
{
	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);
	short size;
	*packet >> size;
	UINT64 data;
	*packet >> data;
	Session* pSession = &g_SessionArray[index];
	pSession->_sendBuf.Enqueue((const char*)&size, sizeof(short));
	pSession->_sendBuf.Enqueue((const char*)&data, size);
	int useSize = pSession->_sendBuf.GetUseSize();
	/*if (g_SessionArray[index]._sendBuf.GetBufferSize() > 0)
		SendPost(&g_SessionArray[index]);*/
	if (useSize > 0)
	{
		SendPost(pSession);
	}
	else
	{
		pSession->_queue.enqueue({ pSession->_sock,EventType::SENDFIRSTSIZE0, GetCurrentThreadId(), pSession->_IOCount,  __LINE__ });
	}
}
