#include "Timer.h"
#include <Windows.h> // QPC 함수. 헤더가 아니라 cpp 에서만 include 해서
// Timer.h 를 쓰는 쪽에 Windows.h 가 퍼지지 않게 한다 (컴파일 속도·이름 충돌 방지)

namespace Engine {

	// constexpr: 컴파일 시점에 값이 정해지는 상수. #define 대신 쓰는 타입 있는 상수
	static constexpr double kMaxDeltaTime = 0.1; // 이 시간보다 긴 프레임은 잘라 낸다

	Timer::Timer() {
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq); // 1초에 눈금이 몇 번 올라가는지 (PC 마다 다름, 보통 1000만)
		m_secondsPerCount = 1.0 / static_cast<double>(freq.QuadPart); // 1초를 눈금 수로 나누면 눈금 하나가 몇 초인지 알 수 있다
		Reset();
	}

	void Timer::Reset() {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now); // 현재 눈금
		m_prevCount = now.QuadPart;
		m_deltaTime = 0.0f;
		m_totalTime = 0.0f;
	}

	void Timer::Tick() {

		LARGE_INTEGER now;
		QueryPerformanceCounter(&now); 

		long long elapsed = now.QuadPart - m_prevCount; // 지난 Tick 이후 흐른 눈금 수
		m_prevCount = now.QuadPart; // 다음 프레임을 위해 기준 갱신

		double dt = elapsed * m_secondsPerCount; // 눈금 수를 초로 변환

		// dt 상한 걸기 (clamp)
		// 브레이크포인트에서 10초 멈췄다가 재개하거나, 창을 드래그하는 동안에는
		// dt 가 엄청 커진다 → 뿌요가 한 프레임에 바닥까지 순간이동 / 벽 뚫기 같은 버그가 생긴다.
		// 그래서 아무리 길어도 0.1초로 친다 (= 그동안 게임이 "멈춰 있던" 것으로 취급)
		if (dt > kMaxDeltaTime) {
			dt = kMaxDeltaTime;
		}

		m_deltaTime = static_cast<float>(dt);
		m_totalTime += m_deltaTime; // 잘린 dt 를 더하므로 "실제 시각"이 아니라 "게임 안 시간"

	}

}