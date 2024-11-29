#include <iostream>
#include "CGameLenServer.h"
#include "CLogManager.h"

CLogManager CLogManager::instance;

int main()
{
	CGameLenServer gameServer;

	gameServer.Start(nullptr, 6000, 4, true, 10000);
	while (true)
	{
		if (GetAsyncKeyState('Q') & 0x8000)
		{
			gameServer.Stop();

			std::cout << "'q' 키 입력: PQCS 전송 완료" << std::endl;
			break;
		}
	}

}
