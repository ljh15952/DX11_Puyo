#include <Windows.h>
#include <cmath> // std::sin
#include <string> // std::wstring, std::to_wstring
#include "Window\Window.h"
#include "Graphics\Graphics.h"
#include "Timer\Timer.h"
#include "Graphics\SpriteRenderer.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {

	Engine::Window window;
	if (!window.Create(L"Pu", 800, 600))
		return -1;

	// 지역 변수는 선언의 "역순"으로 소멸된다 → graphics 가 먼저 정리되고 window 가 나중에 파괴된다.
	//  반대로 하면 창이 먼저 사라진 뒤 스왑체인이 정리되어 문제가 될 수 있다
	Engine::Graphics graphics;
	if (!graphics.Init(window.GetHwnd(), window.GetWidth(), window.GetHeight()))
		return -1;

	Engine::SpriteRenderer spriteRenderer;
	if (!spriteRenderer.Init(graphics))
		return -1;

	Engine::Timer timer;
	timer.Reset(); // 초기화에 걸린 시간이 첫 dt 에 섞이지 않게 루프 직전에 기준점을 잡는다

	// FPS(초당 프레임 수) 측정용
	float fpsTimer = 0.0f; // 1초마다 FPS 를 계산하기 위한 누적 시간
	int frameCount = 0; // 1초 동안 몇 프레임이 그려졌는지 세기 위한 카운터



	// 뿌요 보드: 가로 6칸 × 세로 12칸, 한 칸 40픽셀
	const float cell = 40.0f;
	const float boardX = 280.0f; // (800 - 6*40) / 2 = 가로 가운데
	const float boardY = 60.0f;
	float fallY = 0.0f; // 떨어지는 사각형의 보드 안 y 위치 (픽셀)




	// 루프
	while (window.ProcessMessage()) {
		timer.Tick(); // 시간 간격 계산
		float dt = timer.GetDeltaTime(); // 지난 프레임과 현재 프레임 사이의 시간 간격

		// ----- FPS 를 1초마다 제목에 표시 -----
		++frameCount;
		fpsTimer += dt;
		if (fpsTimer >= 1.0f) {
			std::wstring title = L"Puyo  FPS: " + std::to_wstring(frameCount);
			window.SetTitle(title.c_str()); // .c_str(): wstring → const wchar_t* (Win32 API 가 받는 형식)

			frameCount = 0;
			fpsTimer -= 1.0f; // = 0 이 아니라 -= 1: 1.02초였다면 남은 0.02초를 버리지 않고 다음 1초에 넘긴다
		}

		// ----- dt 동작 확인용 -----
		//  sin 은 -1 ~ 1 을 반복 → 0.25 ± 0.15 = 0.10 ~ 0.40 사이를 오간다
		//  * 2.0f : 진동 속도 (클수록 빨리 깜빡임)
		float t = timer.GetTotalTime();
		float blue = 0.25f + 0.15f * std::sin(t * 2.0f);

		graphics.BeginFrame(0.1f, 0.1f, blue);
		// 떨어지기: "1초에 120픽셀" × dt → FPS 와 상관없이 같은 속도
		fallY += 120.0f * dt;
		if (fallY > cell * 11)
			fallY = 0.0f; // 바닥(12번째 칸)에 닿으면 맨 위로

		spriteRenderer.DrawRect(boardX, boardY, cell * 6, cell * 12, 0.05f, 0.05f, 0.1f);
		spriteRenderer.DrawRect(boardX + cell * 2, boardY + fallY, cell, cell, 0.9f, 0.2f, 0.2f);
		spriteRenderer.DrawRect(boardX + cell * 2, boardY + fallY - cell, cell, cell, 0.2f, 0.8f, 0.2f);

		graphics.EndFrame();
	}

	return 0;
}