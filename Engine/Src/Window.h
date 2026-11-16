#pragma once
#include <Windows.h>

/*
윈도우 생성
1. 창 클래스 등록 (RegisterClassExW)
2. 창 생성 (CreateWindowExW)
3. WndProc: OS가 "닫기 버튼 눌림", "키 눌림" 같은 메시지를 보낼 때 호출되는 함수
4. 메시지 루프: 쌓인 메시지를 꺼내서 WndProc로 전달하는 반복문

AdjustWindowRect:
CreateWindow에 800×600을 넣으면 제목 표시줄과 테두리까지 포함해서 800×600
실제로 그림이 그려지는 영역(클라이언트 영역)은 더 작아짐
이 함수로 테두리 크기를 미리 계산해서, 그리는 영역이 정확히 800×600이 되게
*/

namespace Engine {

	class Window {

	public:

		Window() = default; 
		~Window();

		// 창은 하나뿐인 자원이라 복사X
		Window(const Window&) = delete; // 복사 생성자 금지
		Window& operator=(const Window&) = delete; // 복사 대입 금지

		bool Create(const wchar_t* title, int width, int height);

		// 쌓인 메시지를 모두 처리, 종료 요청이 오면 false
		bool ProcessMessage();

		HWND GetHwnd() const { return m_hwnd; }
		int GetWidth() const { return m_width; }
		int GetHeight() const { return m_height; }

	private:
		// OS 는 Window 객체(this)를 모르므로 인자 4개만 넘겨준다.
		// 멤버 함수는 this 를 숨은 인자로 받아야 하니 안 맞음 → this 가 없는 static 으로 만든다.
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

		HWND m_hwnd = nullptr;
		HINSTANCE m_hinstance = nullptr;
		int m_width = 0;
		int m_height = 0;
	};

}
