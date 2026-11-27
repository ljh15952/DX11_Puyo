#include <Windows.h>
#include <cmath> // std::sin, std::fmod
#include <string> // std::wstring, std::to_wstring
#include "Window\Window.h"
#include "Graphics\Graphics.h"
#include "Timer\Timer.h"
#include "Sprite\SpriteRenderer.h"
#include "Texture\Texture.h"
#include "Math\MathTypes.h"

// ---------- 게임(Pu) 전용 데이터: 엔진이 아니라 게임 쪽에 둔다 ----------
// using: 긴 이름에 짧은 별명
using Engine::Rect;
using Engine::Color;

// 뿌요 색 (흰색 시트에 곱해지는 틴트)
// constexpr: 컴파일할 때 값이 정해지는 상수 (#define 대신)
constexpr Color kPuyoRed{ 1.00f, 0.35f, 0.35f, 1.0f };
constexpr Color kPuyoGreen{ 0.40f, 1.00f, 0.40f, 1.0f };
constexpr Color kPuyoBlue{ 0.40f, 0.55f, 1.00f, 1.0f };
constexpr Color kPuyoYellow{ 1.00f, 0.95f, 0.35f, 1.0f };
constexpr Color kPuyoPurple{ 0.80f, 0.45f, 1.00f, 1.0f };

// 시트 안 프레임 위치 (픽셀). 한 칸 40x40 이 가로로 4개
constexpr float kFrameSize = 40.0f;
// 몇 번째 프레임인지 → 시트의 잘라 올 영역
// constexpr 함수: 컴파일 시점에도 계산할 수 있는 함수
constexpr Rect PuyoFrame(int index)
{
	return Rect{ index * kFrameSize, 0.0f, kFrameSize, kFrameSize };
}
enum PuyoFace { FaceNormal = 0, FaceBlink = 1, FaceHappy = 2, FaceSurprised = 3 };  // enum: 이름 붙은 정수 상수 묶음

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {

	Engine::Window window;
	if (!window.Create(L"Pu", 800, 600))
		return -1;

	// 지역 변수는 선언의 "역순"으로 소멸된다 → graphics 가 먼저 정리되고 window 가 나중에 파괴된다.
	// 반대로 하면 창이 먼저 사라진 뒤 스왑체인이 정리되어 문제가 될 수 있다
	Engine::Graphics graphics;
	if (!graphics.Init(window.GetHwnd(), window.GetWidth(), window.GetHeight()))
		return -1;

	Engine::SpriteRenderer spriteRenderer;
	if (!spriteRenderer.Init(graphics))
	{
		MessageBoxW(nullptr, L"SpriteRenderer 초기화 실패 (출력 창의 셰이더 에러 확인)", L"Error", MB_OK);
		return -1;
	}

	// graphics 보다 뒤에 선언 → 먼저 소멸 (spriteRenderer 와 같은 이유)
	Engine::Texture puyoSheet;
	// 경로 구분자는 / 를 써도 Windows 가 알아듣는다.
	//  \ 를 쓰려면 문자열 안에서는 L"Assets\\puyo_sheet.png" 처럼 두 번 써야 한다 (\ 하나는 이스케이프 문자라서)
	if (!puyoSheet.Load(graphics.GetDevice(), L"Assets/puyo_sheet.png"))
	{
		MessageBoxW(nullptr, L"Assets/puyo_sheet.png 를 읽지 못했습니다", L"Error", MB_OK);
		return -1;
	}

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

		// 3초마다 0.15초 동안 눈 깜빡임
		// std::fmod(a, b): 실수 나머지 (a % b 의 float 버전). t 가 0~3 사이를 반복하게 만든다
		int face = (std::fmod(t, 3.0f) < 0.15f) ? FaceBlink : FaceNormal;

		// 보드 배경
		spriteRenderer.DrawRect(Rect{ boardX, boardY, cell * 6, cell * 12 }, Color{ 0.05f, 0.05f, 0.1f, 1.0f });
		// 떨어지는 2개짜리 (빨강 아래, 초록 위)
		spriteRenderer.DrawSprite(puyoSheet, Rect{ boardX + cell * 2, boardY + fallY,        cell, cell }, PuyoFrame(face), kPuyoRed);
		spriteRenderer.DrawSprite(puyoSheet, Rect{ boardX + cell * 2, boardY + fallY - cell, cell, cell }, PuyoFrame(face), kPuyoGreen);

		// 바닥 줄: 모든 프레임 × 다른 색 미리보기
		float bottomY = boardY + cell * 11;
		spriteRenderer.DrawSprite(puyoSheet, Rect{ boardX + cell * 0, bottomY, cell, cell }, PuyoFrame(FaceNormal), kPuyoBlue);
		spriteRenderer.DrawSprite(puyoSheet, Rect{ boardX + cell * 1, bottomY, cell, cell }, PuyoFrame(FaceHappy), kPuyoYellow);
		spriteRenderer.DrawSprite(puyoSheet, Rect{ boardX + cell * 3, bottomY, cell, cell }, PuyoFrame(FaceSurprised), kPuyoPurple);
		// source 없이 → 시트 전체(4프레임)가 한 칸에 납작하게 들어간다 (오버로드 차이 확인용)
		spriteRenderer.DrawSprite(puyoSheet, Rect{ boardX + cell * 4, bottomY, cell * 2, cell / 2 });



		graphics.EndFrame();
	}

	return 0;
}