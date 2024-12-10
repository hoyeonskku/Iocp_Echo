#pragma once
#include <pdh.h>
#pragma comment(lib,"Pdh.lib")

#define df_PDH_ETHERNET_MAX 8

struct st_ETHERNET
{
	bool _bUse;
	WCHAR _szName[128];
	PDH_HQUERY _recvQuery;
	PDH_HQUERY _sendQuery;
	PDH_HCOUNTER _pdh_Counter_Network_RecvBytes;
	PDH_HCOUNTER _pdh_Counter_Network_SendBytes;
};


class CNetworkUsage {
public:
	CNetworkUsage(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void UpdateNetworkUsage(void);

	double GetRecvBytes(void) { return _recvBytes; }
	double GetSendBytes(void) { return _sendBytes; }


private:
	HANDLE _hProcess;
	st_ETHERNET _EthernetStruct[df_PDH_ETHERNET_MAX];
	double _sendBytes;
	double _recvBytes;

};