#include "CProcessorCpuUsage.h"

CProcessorCpuUsage::CProcessorCpuUsage(HANDLE hProcess)
{
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	_fProcessorTotal = 0; 
	_fProcessorUser = 0;
	_fProcessorKernel = 0;
	_ftProcessor_LastKernel.QuadPart = 0;
	_ftProcessor_LastUser.QuadPart = 0;
	_ftProcessor_LastIdle.QuadPart = 0;
	UpdateProcessorCpuTime();
}

void CProcessorCpuUsage::UpdateProcessorCpuTime()
{
	ULARGE_INTEGER Idle;
	ULARGE_INTEGER Kernel; 
	ULARGE_INTEGER User;

	if ( GetSystemTimes((PFILETIME)&Idle, (PFILETIME)&Kernel, (PFILETIME)&User ) == false )
		return;

	ULONGLONG KernelDiff = Kernel.QuadPart - _ftProcessor_LastKernel.QuadPart;
	ULONGLONG UserDiff = User.QuadPart - _ftProcessor_LastUser.QuadPart;
	ULONGLONG IdleDiff = Idle.QuadPart - _ftProcessor_LastIdle.QuadPart;
	ULONGLONG Total = KernelDiff + UserDiff;

	if (Total == 0) 
	{ 
		_fProcessorUser = 0.0f;
		_fProcessorKernel = 0.0f; 
		_fProcessorTotal = 0.0f;
	}
	else 
	{  
		_fProcessorTotal = (float)((double)(Total - IdleDiff) / Total * 100.0f);
		_fProcessorUser = (float)((double)UserDiff / Total * 100.0f);
		_fProcessorKernel = (float)((double)(KernelDiff - IdleDiff) / Total * 100.0f); 
	}
	_ftProcessor_LastKernel = Kernel;
	_ftProcessor_LastUser = User;
	_ftProcessor_LastIdle = Idle;
}