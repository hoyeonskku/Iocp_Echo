#include <iostream>
#include "CGameLenServer.h"

int main()
{
	CGameLenServer gameServer;

	gameServer.Start(nullptr, 6000, 4, true, 10000);

	while (true) {}

	gameServer.Stop();
}
