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

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;

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

		Session* pSession = nullptr;

		for (int i = 0; i < 10000; i++)
		{
			if (/*g_SessionArray[i]._invalidFlag == -1 &&*/ InterlockedExchange(&g_SessionArray[i]._invalidFlag, 1) == -1)
			{
				pSession = &g_SessionArray[i];
				pSession->Clear(client_sock, clientaddr, sessionIDCount++, i);
				break;
			}
			if (i == 9999)
				DebugBreak();
		}

		CreateIoCompletionPort((HANDLE)client_sock, hrd, (ULONG_PTR)pSession, 0);

		if (pSession->_sock == INVALID_SOCKET)
			DebugBreak();

		// accept 또한 하나의 io 일감으로 처리해야 리시브가 안 걸린 순간에 삭제를 막을 수 있음.
		InterlockedIncrement(&pSession->_IOCount);

		OnAccept(pSession->_sessionID);
		RecvPost(pSession);

		// accept에서 실패했다면 여기서 삭제해줘야 함.
		if (InterlockedDecrement(&pSession->_IOCount) == 0)
		{
			Release(pSession->_sessionID);
		}
	}
	return 0;
}

unsigned int WINAPI NetworkThread(void* arg)
{
	int retval;

	HANDLE hrd = *(HANDLE*)arg;
	while (1)
	{
		// 비동기 입출력 완료 기다리기
		DWORD cbTransferred;
		Session* pSession = nullptr;
		WSAOVERLAPPED* ovl;

		retval = GetQueuedCompletionStatus(hrd, &cbTransferred,
			(PULONG_PTR)&pSession, &ovl, INFINITE);

		if (pSession->_sock == INVALID_SOCKET)
			DebugBreak();

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
			if (pSession->_sendCount == 0)
				DebugBreak();
			for (unsigned int i = 0; i < pSession->_sendCount; i++)
			{
				if (pSession->_sendBuf.GetUseSize() == 0)
					DebugBreak();
				CPacket* packet = nullptr;
				pSession->_sendBuf.Dequeue(&packet);
				packet->Release();
			}
			InterlockedExchange(&pSession->_sendCount, 0);
			//pSession->_sendCount = 0;

			// 여기서 풀어주는 이유는 사이즈를 보고 보낼 게 없을 때 풀어주게 되면 그 사이에 인큐를 해버리는 스레드가 있을 수 있어서 아무도 send를 하지 않게 됨
			InterlockedExchange(&pSession->_sendFlag, 0);
			SendPost(pSession);
		}

		if (InterlockedDecrement(&pSession->_IOCount) == 0)
		{
			Release(pSession->_sessionID);
		}
	}
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
			return;
		}
	}
	return;
}

void SendPost(Session* pSession)
{
	if (pSession->_sendBuf.GetUseSize() == 0)
	{
		return;
	}

	if (InterlockedExchange(&pSession->_sendFlag, 1) == 1)
	{
		return;
	}
	
	// send 미아 되는 버그 해결
	// 과정은 send가 완료되는 시점에서 첫번째 사이즈 체크통과, 다른 리시브가 send, recv진행, 그 send 완료통지가 왔을 때 샌드플래그를 풀어줌
	// 그 후 첫 send 완료통지가 두번째 사이즈 체크까지 통과한 후, recv 완료통지가 인큐 하면 send가 미아되는 현상 발생 (리시브가 없으면 send가 없음)
	// 이를 다른 스레드가 플래그를 바꿨다면 양보해주는 식으로 코드를 짜되, 내가 보내려고 했는데 보낼 게 없다면, 다시 재반복 하는 형식으로 해결
	// 만약 while 루프 안의 세션에서 리턴되었다면, 인큐 할 리시브 완료통지에서 send를 할 것이기 때문에 리턴
	// 만약 그 이후에도 send 시도하려는 스레드가 있다면 그 스레드가 시도할 것이기 때문에 리턴
	// 반복
	if (pSession->_sendBuf.GetUseSize() == 0)
	{
		InterlockedExchange(&pSession->_sendFlag, 0);
		return;
		/*while (true)
		{
			if (pSession->_sendBuf.GetUseSize() == 0)
			{
				return;
			}
			if (InterlockedExchange(&pSession->_sendFlag, 1) == 1)
			{
				return;
			}
			if (pSession->_sendBuf.GetUseSize() == 0)
			{
				InterlockedExchange(&pSession->_sendFlag, 0);
				continue;
			}
			break;
		}*/
	}

	WSABUF wsabufs[2000];

	int bufsNum = pSession->_sendBuf.SetSendWsabufs(wsabufs);

	ZeroMemory(&pSession->_sendOvl, sizeof(pSession->_sendOvl));
	InterlockedIncrement(&pSession->_IOCount);
	if (pSession->_sock == INVALID_SOCKET)
		DebugBreak();

	InterlockedAdd(&pSession->_sendCount, bufsNum);
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
			return;
		}
	}
	return;
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

		CPacket* packetData = new CPacket();
		*packetData << size;

		char* writePos = packetData->GetBufferPtr() + sizeof(short);
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
		packetData->MoveWritePos(size);
		OnRecv(pSession->_sessionID, packetData);
	}
	RecvPost(pSession);
}

void OnAccept(UINT64 sessionID)
{
	CPacket* packet = new CPacket();
	*packet << (short) 8;
	*packet << 0x7fffffffffffffff;
	return SendPacket(sessionID, packet);
}


void OnRecv(UINT64 sessionID, CPacket* packet)
{
	return SendPacket(sessionID, packet);
}

bool Release(UINT64 sessionID)
{


	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);

	Session* pSession = &g_SessionArray[index];
	int useSize = pSession->_sendBuf.GetUseSize();
	for (unsigned int i = 0; i < useSize / sizeof(void*); i++)
	{
		CPacket* packet = nullptr;
		pSession->_sendBuf.Dequeue(&packet);
		packet->Release();
	}
	if (pSession->_sendBuf.GetUseSize() != 0)
		DebugBreak();
	closesocket(g_SessionArray[index]._sock);
	g_SessionArray[index]._sock = INVALID_SOCKET;
	g_SessionArray[index]._invalidFlag = -1;
	return true;
}

void  SendPacket(UINT64 sessionID, CPacket* packet)
{
	packet->AddRef();
	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);
	Session* pSession = &g_SessionArray[index];
	pSession->_sendBuf.Enqueue(&packet);

	SendPost(&g_SessionArray[index]);
	return;
}
