#include <Windows.h>
#include "Engine.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	MessageBoxW(nullptr, Engine::GetVersion(),L"Check2", MB_OK);
	return 0;
}