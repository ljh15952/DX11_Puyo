#include <Windows.h>
#include "Window.h"
#include "Graphics.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {

	Engine::Window window;
	if(!window.Create(L"Pu", 800, 600))
		return -1;

	// 지역 변수는 선언의 "역순"으로 소멸된다 → graphics 가 먼저 정리되고 window 가 나중에 파괴된다.
	//  반대로 하면 창이 먼저 사라진 뒤 스왑체인이 정리되어 문제가 될 수 있다
	Engine::Graphics graphics;
	if (!graphics.Init(window.GetHwnd(), window.GetWidth(), window.GetHeight()))
		return -1;

	// 루프
	while (window.ProcessMessage()) {
		graphics.BeginFrame(0.1f, 0.1f, 0.25f);
		graphics.EndFrame();
	}

	return 0;
}