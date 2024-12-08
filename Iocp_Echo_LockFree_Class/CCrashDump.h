#include <iostream>
#include "Windows.h"
#include "Psapi.h"
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.Lib")

#ifndef _PROCADEMY_LIB_CRASH_DUMP_
#define _PROCADEMY_LIB_CRASH_DUMP_



class CCrashDump

{
public:
	CCrashDump()
	{
		_DumpCount = 0;
		_invalid_parameter_handler oldHandler, newHandler;
		newHandler = mylnvalidParameterHandler;

		oldHandler = _set_invalid_parameter_handler(newHandler); // crt함수에 null포인터 등을 넣었을 때..

		_CrtSetReportMode(_CRT_WARN, 0);	// CRT 오류 메시지 표시 중단. 바로 덤프로 남도록.
		_CrtSetReportMode(_CRT_ASSERT, 0);	// CRT 오류 메시지 표시 중단. 바로 덤프로 남도록.
		_CrtSetReportMode(_CRT_ERROR, 0);	// CRT 오류 메시지 표시 중단. 바로 덤프로 남도록.

		_CrtSetReportHook(_custom_Report_hook);

		// pure virtual function called 에러 핸들러 우회
		_set_purecall_handler(myPurecallHandler);


		SetHandlerDump();
	}

	static void Crash(void)
	{
		int* p = nullptr;
		*p = 0;
	}



	static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
	{
		int iWorkingMemory = 0;

		SYSTEMTIME stNowTime;
		long DumpCount = InterlockedIncrement(&DumpCount);


		// 현재 프로세스의 메모리 사용량을 얻어온다.
		HANDLE hProcess = 0;
		PROCESS_MEMORY_COUNTERS pmc;

		hProcess = GetCurrentProcess();

		if (NULL == hProcess)
			return 0;

		if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
		{
			iWorkingMemory = (int)(pmc.WorkingSetSize / 1024 / 1024);
		}
		CloseHandle(hProcess);
		// 현재 날짜와 시간을 알아온다.


		WCHAR filename[MAX_PATH];
		GetLocalTime(&stNowTime);
		wsprintf(filename, L"Dump_ % d % 0d % 02d_ % 02d. % 02d. % 02d % d % dMB.dmp",
			stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond, DumpCount, iWorkingMemory);
		wprintf(L"WnWnWn!!!CrashError!!! %d.%d.%d / %d:%d::%d\n",
			stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);
		wprintf(L"Now Save dump file...Wn");

		HANDLE hDumpFile = ::CreateFile(filename,
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);
		if (hDumpFile != INVALID_HANDLE_VALUE)
		{
			_MINIDUMP_EXCEPTION_INFORMATION MinidumpExceptionInformation;

			MinidumpExceptionInformation.ThreadId = ::GetCurrentThreadId();
			MinidumpExceptionInformation.ExceptionPointers = pExceptionPointer;
			MinidumpExceptionInformation.ClientPointers = TRUE;

			MiniDumpWriteDump(GetCurrentProcess(),
				GetCurrentProcessId(),
				hDumpFile,
				MiniDumpWithFullMemory,
				&MinidumpExceptionInformation
				, NULL
				, NULL);
			wprintf(L"CrashDumpSaveFinish!");

			CloseHandle(hDumpFile);
		}
		return EXCEPTION_EXECUTE_HANDLER;
	}



	static void SetHandlerDump()
	{
		SetUnhandledExceptionFilter(MyExceptionFilter);
	}


	// Invalid Parameter handler,
	static void mylnvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved)
	{
		Crash();
	}


	static int _custom_Report_hook(int ireposttype, char* message, int* returnvalue)
	{
		Crash();
		return true;
	}


	static void myPurecallHandler(void)
	{
		Crash();
	}


	static long	_DumpCount;
};

#endif
