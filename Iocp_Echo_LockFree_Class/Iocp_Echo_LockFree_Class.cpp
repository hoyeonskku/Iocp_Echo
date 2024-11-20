#include <iostream>
#include "CGameLenServer.h"

int main()
{
	CGameLenServer gameServer;

	gameServer.Start(nullptr, 6000, 5, true, 10000);

	while (true) {}

	gameServer.Stop();
}
