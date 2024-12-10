#include <iostream>
#include <windows.h>
#include <conio.h>
#include <pdhmsg.h>
#include <strsafe.h>
#include "CNetworkUsage.h"

CNetworkUsage::CNetworkUsage(HANDLE hProcess)
{
    _hProcess = hProcess;
    _recvBytes = 0;
    _sendBytes = 0;
    int iCnt = 0;
    bool bErr = false;
    WCHAR* szCur = NULL;
    WCHAR* szCounters = NULL;
    WCHAR* szInterfaces = NULL;
    DWORD dwCounterSize = 0, dwInterfaceSize = 0;
    WCHAR szQuery[1024] = { 0, };

    PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0);
    szCounters = new WCHAR[dwCounterSize];
    szInterfaces = new WCHAR[dwInterfaceSize];

    if (PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS)
    {
        delete[] szCounters;
        delete[] szInterfaces;
        __debugbreak();
        return;
    }
    iCnt = 0;
    szCur = szInterfaces;

    for (; *szCur != L'\0' && iCnt < df_PDH_ETHERNET_MAX; szCur += wcslen(szCur) + 1, iCnt++)
    {
        _EthernetStruct[iCnt]._bUse = true;
        _EthernetStruct[iCnt]._szName[0] = L'\0';
        wcscpy_s(_EthernetStruct[iCnt]._szName, szCur);

        // \Network Interface(Realtek PCIe GbE Family Controller)\Bytes Received/sec

        szQuery[0] = L'\0';
        PdhOpenQuery(NULL, NULL, &_EthernetStruct[iCnt]._recvQuery);
        StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Received\/sec", szCur);
        PdhAddCounter(_EthernetStruct[iCnt]._recvQuery, szQuery, NULL, &_EthernetStruct[iCnt]._pdh_Counter_Network_RecvBytes);

        szQuery[0] = L'\0';
        PdhOpenQuery(NULL, NULL, &_EthernetStruct[iCnt]._sendQuery);
        StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Sent\/sec", szCur);
        PdhAddCounter(_EthernetStruct[iCnt]._sendQuery, szQuery, NULL, &_EthernetStruct[iCnt]._pdh_Counter_Network_SendBytes);
    }
}

void CNetworkUsage::UpdateNetworkUsage(void)
{
    _recvBytes = 0;
    _sendBytes = 0;
    for (int iCnt = 0; iCnt < df_PDH_ETHERNET_MAX; iCnt++)
    {
        if (_EthernetStruct[iCnt]._bUse)
        {
            PDH_FMT_COUNTERVALUE counterVal1;
            PdhCollectQueryData(_EthernetStruct[iCnt]._recvQuery);
            long status = PdhGetFormattedCounterValue(_EthernetStruct[iCnt]._pdh_Counter_Network_RecvBytes, PDH_FMT_DOUBLE, NULL, &counterVal1);
            if (status == 0) _recvBytes += counterVal1.doubleValue;

            PdhCollectQueryData(_EthernetStruct[iCnt]._sendQuery);
            PDH_FMT_COUNTERVALUE counterVal2;
            status = PdhGetFormattedCounterValue(_EthernetStruct[iCnt]._pdh_Counter_Network_SendBytes, PDH_FMT_DOUBLE, NULL, &counterVal2);
            if (status == 0) _sendBytes += counterVal2.doubleValue;
        }
    }

}
