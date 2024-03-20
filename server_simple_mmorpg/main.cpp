#include "stdafx.h"
#include "global.h"


int main()
{
	setlocale(LC_ALL, "korean");
	std::wcout.imbue(std::locale("korean"));

	g_server.init_npcs();
	g_server.start();
}