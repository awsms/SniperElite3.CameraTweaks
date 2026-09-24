#pragma once
#include <windows.h>

class FreeCamera {
public:
	static bool ms_bEnabled;
	static void Init();
	static DWORD WINAPI Thread(LPVOID);

};