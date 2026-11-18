#pragma once

namespace Engine{
	class Timer {
	public:
		
		Timer(); // 생성될 때 시계의 "1초당 눈금 수"를 한 번 구해 둔다

		void Reset(); // 지금을 기준점으로 다시 잡는다 (루프 시작 직전에 호출)

		void Tick(); // 매 프레임 맨 처음 1번 호출 → dt 계산

		// const 멤버 함수: "이 함수는 멤버 변수를 바꾸지 않는다"는 약속.
		// 읽기만 하는 Get 함수에 붙인다. const 객체에서도 호출할 수 있게 된다
		float GetDeltaTime() const { return m_deltaTime; }
		float GetTotalTime() const { return m_totalTime; }

	private:
		// QPC 는 "초"가 아니라 "눈금(count)"을 센다. 눈금 1개가 몇 초인지 저장해 둔다
		double m_secondsPerCount = 0.0;

		long long m_prevCount = 0; // 직전 Tick 때의 눈금

		float m_deltaTime = 0.0f; // 지난 프레임 → 이번 프레임 (초)
		float m_totalTime = 0.0f; // Reset 이후 누적 시간 (초)
	};
}