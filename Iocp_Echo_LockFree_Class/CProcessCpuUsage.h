#pragma once
#include "Windows.h"

class CProcessCpuUsage {
public: 
	CProcessCpuUsage(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void UpdateProcessCpuTime(void);
	void UpdateProcessMemory(void);
	void UpdateMemory(void);

	float GetProcessTotalTime(void) { return _fProcessTotal; }
	float GetProcessUserTime(void) { return _fProcessUser; } 
	float GetProcessKernelTime(void) { return _fProcessKernel; }

	LONGLONG  GetProcessPrivateMemoryUsage() {	return _processPrivateBytes; }
	LONGLONG  GetProcessPoolNonpagedMemoryUsage() {	return _processPoolNonpagedBytes; }

	LONGLONG  GetAvailableMemoryUsage() {	return _availableBytes; }
	LONGLONG  GetPoolNonpagedMemoryUsage() {	return _poolNonpagedBytes; }
	

private:
	HANDLE _hProcess; 
	int _iNumberOfProcessors;
	float _fProcessTotal; 
	float _fProcessUser; 
	float _fProcessKernel;
	ULARGE_INTEGER _ftProcess_LastKernel; 
	ULARGE_INTEGER _ftProcess_LastUser;
	ULARGE_INTEGER _ftProcess_LastTime;

	LONGLONG _processPrivateBytes;
	LONGLONG  _processPoolNonpagedBytes;

	LONGLONG  _availableBytes;
	LONGLONG  _poolNonpagedBytes;

};