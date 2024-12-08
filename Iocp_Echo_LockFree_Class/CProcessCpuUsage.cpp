#include <windows.h>
#include <pdh.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include "CProcessCpuUsage.h"

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "pdh.lib")


CProcessCpuUsage::CProcessCpuUsage(HANDLE hProcess) { 
	if ( hProcess == INVALID_HANDLE_VALUE )
	{
		_hProcess = GetCurrentProcess(); 
	}
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);

	_iNumberOfProcessors = SystemInfo.dwNumberOfProcessors;

	_fProcessTotal = 0; 
	_fProcessUser = 0; 
	_fProcessKernel = 0;

	_ftProcess_LastUser.QuadPart = 0; 
	_ftProcess_LastKernel.QuadPart = 0; 
	_ftProcess_LastTime.QuadPart = 0;

	UpdateProcessCpuTime();
}

void CProcessCpuUsage::UpdateProcessCpuTime()
{
	ULARGE_INTEGER None;
	ULARGE_INTEGER NowTime;
	ULARGE_INTEGER Kernel;
	ULARGE_INTEGER User;

	GetSystemTimeAsFileTime((LPFILETIME)&NowTime);
	GetProcessTimes(_hProcess, (LPFILETIME)&None, (LPFILETIME)&None, (LPFILETIME)&Kernel, (LPFILETIME)&User);

	ULONGLONG TimeDiff = NowTime.QuadPart - _ftProcess_LastTime.QuadPart;
	ULONGLONG UserDiff = User.QuadPart - _ftProcess_LastUser.QuadPart;
	ULONGLONG KernelDiff = Kernel.QuadPart - _ftProcess_LastKernel.QuadPart;
	ULONGLONG Total = KernelDiff + UserDiff;

	_fProcessTotal = (float)(Total / (double)_iNumberOfProcessors / (double)TimeDiff * 100.0f);
	_fProcessKernel = (float)(KernelDiff / (double)_iNumberOfProcessors / (double)TimeDiff * 100.0f);
	_fProcessUser = (float)(UserDiff / (double)_iNumberOfProcessors / (double)TimeDiff * 100.0f);

	_ftProcess_LastTime = NowTime; 
	_ftProcess_LastKernel = Kernel;
	_ftProcess_LastUser = User;
}

void CProcessCpuUsage::UpdateProcessMemory(void)
{
	PDH_HQUERY processMemoryQuery;
	PDH_HCOUNTER processMemoryCounter;
	DWORD processId = GetCurrentProcessId(); 
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);

	PDH_FMT_COUNTERVALUE processMemoryCounterVal;

	wchar_t processName[MAX_PATH];
	if (GetModuleFileNameEx(hProcess, NULL, processName, MAX_PATH) == 0) {
		CloseHandle(hProcess);
		return;
	}

	CloseHandle(hProcess);

	std::wstring processFileName = processName;
	size_t pos = processFileName.find_last_of(L"\\");
	if (pos != std::wstring::npos) {
		processFileName = processFileName.substr(pos + 1);
	}

	size_t exePos = processFileName.rfind(L".exe");
	if (exePos != std::wstring::npos && exePos == processFileName.length() - 4) {
		processFileName = processFileName.substr(0, exePos);
	}

	PdhOpenQuery(NULL, NULL, &processMemoryQuery);

	std::wstring counterPath = L"\\Process(" + processFileName + L")\\Private Bytes";
	PdhAddCounter(processMemoryQuery, counterPath.c_str(), NULL, &processMemoryCounter);

	PdhCollectQueryData(processMemoryQuery); 
	PdhGetFormattedCounterValue(processMemoryCounter, PDH_FMT_LARGE, NULL, &processMemoryCounterVal);
	_processPrivateBytes = processMemoryCounterVal.largeValue;
	// Äõ¸® Á¾·á
	PdhCloseQuery(processMemoryQuery);

	PdhOpenQuery(NULL, NULL, &processMemoryQuery);

	counterPath = L"\\Process(" + processFileName + L")\\Pool Nonpaged Bytes";
	PdhAddCounter(processMemoryQuery, counterPath.c_str(), NULL, &processMemoryCounter);

	PdhCollectQueryData(processMemoryQuery);
	PdhGetFormattedCounterValue(processMemoryCounter, PDH_FMT_LARGE, NULL, &processMemoryCounterVal);
	_processPoolNonpagedBytes = processMemoryCounterVal.largeValue;
	// Äõ¸® Á¾·á
	PdhCloseQuery(processMemoryQuery);

	return;
}

void CProcessCpuUsage::UpdateMemory(void)
{
	PDH_HQUERY memoryQuery;
	PDH_HCOUNTER memoryCounter;
	DWORD processId = GetCurrentProcessId();
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);

	PDH_FMT_COUNTERVALUE memoryCounterVal;


	PdhOpenQuery(NULL, NULL, &memoryQuery);

	std::wstring counterPath = L"\\Memory\\Available MBytes";
	PdhAddCounter(memoryQuery, counterPath.c_str(), NULL, &memoryCounter);

	PdhCollectQueryData(memoryQuery);
	PdhGetFormattedCounterValue(memoryCounter, PDH_FMT_LARGE, NULL, &memoryCounterVal);
	_availableBytes = memoryCounterVal.largeValue;
	// Äõ¸® Á¾·á
	PdhCloseQuery(memoryQuery);

	PdhOpenQuery(NULL, NULL, &memoryQuery);

	counterPath = L"\\Memory\\Pool Nonpaged Bytes";
	PdhAddCounter(memoryQuery, counterPath.c_str(), NULL, &memoryCounter);

	PdhCollectQueryData(memoryQuery);
	PdhGetFormattedCounterValue(memoryCounter, PDH_FMT_LARGE, NULL, &memoryCounterVal);
	_poolNonpagedBytes = memoryCounterVal.largeValue;
	// Äõ¸® Á¾·á
	PdhCloseQuery(memoryQuery);

	return;
}
