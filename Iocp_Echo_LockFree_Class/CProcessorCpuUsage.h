#include <windows.h>

class CProcessorCpuUsage 
{
public:
	CProcessorCpuUsage(HANDLE hProcess = INVALID_HANDLE_VALUE);
	void UpdateProcessorCpuTime(void);

	float GetProcessorTotalTime(void) { return _fProcessorTotal; }
	float GetProcessorUserTime(void) { return _fProcessorUser; }
	float GetProcessorKernelTime(void) { return _fProcessorKernel; }



private:
	HANDLE _hProcess;
	float _fProcessorTotal;
	float _fProcessorUser; 
	float _fProcessorKernel;

	ULARGE_INTEGER _ftProcessor_LastKernel;
	ULARGE_INTEGER _ftProcessor_LastUser;
	ULARGE_INTEGER _ftProcessor_LastIdle;
};