#include "Window.h"

namespace Engine {

	static const wchar_t* kClassName = L"EngineWindowClass";

	Window::~Window() {
		if (m_hwnd)
			DestroyWindow(m_hwnd);
		UnregisterClassW(kClassName, m_hinstance);
	}

	bool Window::Create(const wchar_t* title, int width, int height) {

		// 현재 exe 의 인스턴스
		m_hinstance = GetModuleHandleW(nullptr);
		m_width = width;
		m_height = height;

		// 1. 창 클래스 등록
		WNDCLASSEXW wc{}; // 설계도 구조체. {} 로 모든 칸을 0 으로 초기화
		wc.cbSize = sizeof(wc); // 구조체 크기. 버전 확인용이라 반드시 넣어야 함
		wc.style = CS_HREDRAW | CS_VREDRAW; // 가로/세로 크기가 바뀌면 다시 그리기
		wc.lpfnWndProc = WndProc; // 이 창에 메시지가 오면 호출할 함수
		wc.hInstance = m_hinstance; // 이 설계도의 소속 프로그램
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); // 창 위 마우스 커서 = 기본 화살표
		wc.lpszClassName = kClassName; // 설계도 이름
		if (!RegisterClassExW(&wc)) return false; // OS 에 등록. 실패하면 0

		// 2. 클라이언트 영역이 width x height 가 되도록 창 전체 크기 계산
		DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX); // 크기 조절 금지
		RECT rc{ 0,0,width,height };
		AdjustWindowRect(&rc, style, FALSE);

		// 3. 창 생성
		m_hwnd = CreateWindowExW(
			0, kClassName, title, style,
			CW_USEDEFAULT, CW_USEDEFAULT,
			rc.right - rc.left, rc.bottom - rc.top,
			nullptr, nullptr, m_hinstance, nullptr);
		if (!m_hwnd) return false;

		ShowWindow(m_hwnd, SW_SHOW);
		return true;

	}

	bool Window::ProcessMessage() {
		MSG msg;
		// PeekMessage: 메시지가 없으면 기다리지 않고 바로 돌아옴
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) return false;
			TranslateMessage(&msg);
			DispatchMessageW(&msg); // WndProc 호출
		}
		return true;
	}

	LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		switch (msg) {
		case WM_DESTROY: // 창이 파괴됨 (X 버튼)
			PostQuitMessage(0); // 메시지 큐에 WM_QUIT 을 넣음
			return 0;
		}
		return DefWindowProcW(hwnd, msg, wparam, lparam); // 나머지는 OS 기본 처리
	}

	void Window::SetTitle(const wchar_t* title) {
		SetWindowTextW(m_hwnd, title);
	}
}