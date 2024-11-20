#include "CLanClient.h"
#include "WinSock2.h"
#include "Windows.h"
#include <ws2tcpip.h>

bool CLanClient::Connect(const wchar_t* bindIP, const wchar_t* serverIP, short serverPort, int numOfWorkerThreads, bool nagle)
{
    int retval;
    int BindRetval;
    int BindError;
    int ConnectRetval;
    int ConnectError;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    _clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_clientSocket == INVALID_SOCKET)
        return false;

    if (bindIP)
    {
        SOCKADDR_IN myAddress;
        myAddress.sin_family = AF_INET;

        InetPton(AF_INET, bindIP, &myAddress.sin_addr);
        // bind
        BindRetval = ::bind(_clientSocket, (SOCKADDR*)(&myAddress), sizeof(myAddress));
        if (BindRetval == SOCKET_ERROR)
        {
            BindError = WSAGetLastError();
            _bShutdown = false;
            return false;
        }
    }

    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;

    InetPton(AF_INET, serverIP, &serveraddr.sin_addr);
    serveraddr.sin_port = htons(serverPort);
    ConnectRetval = retval = connect(_clientSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (ConnectRetval == SOCKET_ERROR)
    {
        ConnectError = WSAGetLastError();
        _bShutdown = false;
        return false;
    }
}

bool CLanClient::Disconnect()
{
	return false;
}

bool CLanClient::SendPacket(CPacket* pPacket)
{
	return false;
}
