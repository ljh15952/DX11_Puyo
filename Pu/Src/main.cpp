#include <Windows.h>
#include "Window.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {

	Engine::Window window;
	if(!window.Create(L"Pu", 800, 600))
		return -1;

	// 게임 루프
	while (window.ProcessMessage()) {
		// 게임 로직
		// 렌더링
	}

	return 0;
}