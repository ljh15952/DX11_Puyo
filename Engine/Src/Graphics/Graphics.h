#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

/*
D3D11로 화면 지우기

GPU에 그림을 그리려면 아래 4가지가 필요
Device					GPU 자원(텍스처, 버퍼, 셰이더)을 만드는 객체
DeviceContext			그리기 명령을 실행하는 객체
SwapChain				화면에 보낼 이미지(백버퍼)를 2장 이상 들고 번갈아 보여주는 객체
RenderTargetView (RTV)	"이 텍스처에 그려라"라고 GPU에 알려주는 이름표

매 프레임 흐름: 백버퍼를 단색으로 지우기(Clear) → (여기서 그리기) → Present로 화면에 내보내기
*/

namespace Engine {

	// ComPtr<T>: COM 객체 전용 스마트 포인터.
	//  - D3D 객체는 COM 방식이라 new/delete 가 아니라 참조 카운트(AddRef/Release)로 수명을 관리
	//  - ComPtr 이 소멸될 때 자동으로 Release() 를 불러 주므로 소멸자에서 직접 해제할 필요X
	//    (std::shared_ptr 의 COM 버전)
	//  - using 으로 별칭을 만들어 매번 Microsoft::WRL:: 을 쓰지 않게
	template <class T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	class Graphics {

	public:
		Graphics() = default;
		~Graphics() = default; // ComPtr 들이 알아서 Release 하므로 비워 둔다

		// Window 와 같은 이유: GPU 자원의 주인은 하나
		Graphics(const Graphics&) = delete;
		Graphics& operator=(const Graphics&) = delete;

		// hwnd: 어느 창에 그릴지. D3D 는 이 창의 클라이언트 영역을 화면으로 쓴다
		bool Init(HWND hwnd, int width, int height);

		void BeginFrame(float r, float g, float b); // 화면을 이 색으로 지운다 (0.0 ~ 1.0)
		void EndFrame(); // 완성된 백버퍼를 화면에 내보낸다

		// I 로 시작하는 타입 = Interface. COM 객체는 항상 인터페이스 포인터로만 다룬다
		ID3D11Device* GetDevice() const { return m_device.Get(); } // .Get(): ComPtr 안의 생포인터
		ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

		int GetWidth() const { return m_width; }
		int GetHeight() const { return m_height; }

	private:
		ComPtr<ID3D11Device> m_device; // 자원 생성
		ComPtr<ID3D11DeviceContext> m_context; // 그리기
		ComPtr<IDXGISwapChain> m_swapChain; // 백버퍼 교대
		ComPtr<ID3D11RenderTargetView> m_rtv; // 백버퍼에 붙인 "여기에 그려라" 이름표
		D3D11_VIEWPORT m_viewport{}; // 렌더 타겟 중 실제로 그릴 사각형 영역

		int m_width = 0; // 백버퍼(화면) 크기. 투영 행렬 등에 사용
		int m_height = 0;

	};
}