#define _WINSOCK_DEPRECATED_NO_WARNINGS // 최신 VC++ 컴파일 시 경고 방지
#include "NetLib.h"
#pragma comment(lib, "ws2_32")
#include <Windows.h>
#include <winsock2.h>
#include "Session.h"

bool g_bShutdown = true;
SOCKET listenSocket;

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


	while (1) {
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listenSocket, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			break;
		}
		/*printf("[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));*/

		Session* pSession = new Session(client_sock, clientaddr);

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

		if (pSession->_deleteFlag == true && pSession->_IOCount == 0)
		{
			printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
				inet_ntoa(pSession->_addr.sin_addr), ntohs(pSession->_addr.sin_port));

			LeaveCriticalSection(&pSession->_cs);
			delete pSession;
			continue;
		}
		if (ovl != NULL)
		{
			InterlockedDecrement(&pSession->_IOCount);
			if (&pSession->_recvOvl == ovl)
			{;
				if (cbTransferred == 0)
				{
					Disconnect(pSession);
					LeaveCriticalSection(&pSession->_cs);
					continue;
				}
				// 컨텐츠 로직
				{
					pSession->_recvBuf.MoveRear(cbTransferred);
					WSABUF wsabufs[2];
					wsabufs[0].buf = pSession->_recvBuf.GetFrontBufferPtr();
					wsabufs[0].len = pSession->_recvBuf.DirectDequeueSize();
					wsabufs[1].buf = pSession->_recvBuf._buffer;
					wsabufs[1].len = pSession->_recvBuf.GetUseSize() - wsabufs[0].len;

					char ip[INET_ADDRSTRLEN]; // IPv4 주소를 저장할 버퍼
					// 안전하게 IP 주소 변환 (inet_ntop 사용)
					/*if (inet_ntop(AF_INET, &(pSession->_addr.sin_addr), ip, sizeof(ip)) == nullptr) {
						std::cerr << "Failed to convert IP address" << std::endl;
						return 0;
					}*/

					/*printf("[TCP/%s:%d] %.*s%.*s\n",
						ip,
						ntohs(pSession->_addr.sin_port),
						wsabufs[0].len, wsabufs[0].buf,
						wsabufs[1].len, wsabufs[1].buf);*/

					pSession->_sendBuf.Enqueue(wsabufs[0].buf, wsabufs[0].len);
					//pSession->_sendBuf.Enqueue(wsabufs[1].buf, wsabufs[1].len);

					pSession->_recvBuf.MoveFront(cbTransferred);
				}

				SendPost(pSession);
				RecvPost(pSession);
			}
			else if (&pSession->_sendOvl == ovl)
			{
				pSession->_sendBuf.MoveFront(cbTransferred);
			}
		}
		else
		{
			Disconnect(pSession);
		}

		LeaveCriticalSection(&pSession->_cs);
	}

	return 0;
}

void Disconnect(Session* pSession)
{
	pSession->_deleteFlag = true;
}

void RecvPost(Session* pSession)
{
	EnterCriticalSection(&pSession->_cs);
	InterlockedIncrement(&pSession->_IOCount);
	WSABUF wsabufs[2];

	int freeSize = pSession->_sendBuf.GetFreeSize();
	wsabufs[0].buf = pSession->_recvBuf.GetRearBufferPtr();
	wsabufs[0].len = pSession->_recvBuf.DirectEnqueueSize();

	wsabufs[1].buf = pSession->_recvBuf._buffer;
	wsabufs[1].len = freeSize - wsabufs[0].len;

	DWORD flags = 0;
	ZeroMemory(&pSession->_recvOvl, sizeof(pSession->_recvOvl));
	DWORD wsaSendRetval = WSARecv(pSession->_sock, wsabufs, 1, NULL, &flags, &pSession->_recvOvl, NULL);
	if (wsaSendRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			DebugBreak();
			return;
		}
	}

	LeaveCriticalSection(&pSession->_cs);

}

void SendPost(Session* pSession)
{
	EnterCriticalSection(&pSession->_cs);
	InterlockedIncrement(&pSession->_IOCount);
	WSABUF wsabufs[2];
	//memset(wsabufs, 0, sizeof(wsabufs));
	int useSize = pSession->_sendBuf.GetUseSize();

	wsabufs[0].buf = pSession->_sendBuf.GetFrontBufferPtr();
	wsabufs[0].len = pSession->_sendBuf.DirectDequeueSize();

	wsabufs[1].buf = pSession->_sendBuf._buffer;
	wsabufs[1].len = (useSize - wsabufs[0].len);

	ZeroMemory(&pSession->_sendOvl, sizeof(pSession->_sendOvl));
	//DWORD wsaSendRetval = send(pSession->_sock, wsabufs[0].buf, wsabufs[0].len, 0);
	DWORD wsaSendRetval = WSASend(pSession->_sock, wsabufs, 1, NULL, 0, &pSession->_sendOvl, NULL);

	if (wsaSendRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			DebugBreak();
			return;
		}
	}
	LeaveCriticalSection(&pSession->_cs);
}
