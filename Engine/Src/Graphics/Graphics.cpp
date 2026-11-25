#include "Graphics.h"

// #pragma comment(lib, ...): 프로젝트 속성의 "추가 종속성"에 lib 를 넣는 것과 같다.
//  정적 라이브러리 안에 써 두면 이 lib 를 쓰는 exe(Pu) 링크 때도 자동으로 따라간다
//  → Pu 프로젝트 설정을 건드리지 않아도 된다
#pragma comment(lib, "d3d11.lib")

namespace Engine {

	bool Graphics::Init(HWND hwnd, int width, int height) {

		m_width = width;
		m_height = height;

		// 1. 스왑체인 설정
		DXGI_SWAP_CHAIN_DESC scd{}; // {} = 모든 칸 0 초기화. 안 하면 쓰레기값으로 실패한다
		scd.BufferDesc.Width = width; // 백버퍼 크기 = 창 클라이언트 크기
		scd.BufferDesc.Height = height;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 픽셀 형식: RGBA 각 8비트
		// UNORM = Unsigned NORMalized. 0~255 를 셰이더에서 0.0~1.0 으로 본다
		scd.SampleDesc.Count = 1; // 멀티샘플링(MSAA) 안 씀. 0 이면 에러 (최소 1)
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 이 버퍼는 "그림 그리는 대상"으로 쓴다
		scd.BufferCount = 2; // 백버퍼 2장: 하나를 화면에 보여주는 동안 다른 하나에 그린다
		scd.OutputWindow = hwnd; // 결과를 보낼 창
		scd.Windowed = TRUE; // 창 모드 (FALSE 면 전체화면 독점)
		scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Flip 모델: 복사 대신 버퍼 포인터만 바꿔서 빠름 (Win10+ 권장)
		//  DISCARD: 보여준 뒤 버퍼 내용은 버려도 됨 → 매 프레임 Clear 전제

// 2. 디바이스 생성 옵션
		UINT flags = 0;
#ifdef _DEBUG
		// Debug 빌드에서만 디버그 레이어를 켠다.
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		// Feature Level: "이 GPU 는 D3D 몇 버전 기능까지 지원해야 한다"는 최소 요구사항
		D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

		// 같은 호출을 두 번 쓸 수 있게 람다로 묶는다.
		//  [&] = 바깥 변수(scd, levels, this 의 멤버)를 참조로 가져다 쓴다
		auto create = [&](UINT f) {

			return D3D11CreateDeviceAndSwapChain(
				nullptr,                    // 어떤 GPU 를 쓸지. nullptr = 기본 GPU
				D3D_DRIVER_TYPE_HARDWARE,   // 실제 GPU 사용 (WARP 는 CPU 로 흉내내는 소프트웨어 렌더러)
				nullptr,                    // 소프트웨어 렌더러 DLL. HARDWARE 면 nullptr
				f,                          // 생성 옵션 (디버그 등)
				levels, 1,                  // 요구 Feature Level 배열과 개수
				D3D11_SDK_VERSION,          // 항상 이 상수. 헤더와 런타임 버전이 맞는지 확인용
				&scd,                       // 위에서 만든 스왑체인 설정
				&m_swapChain,               // [출력] 생성된 객체들을 여기에 받는다
				&m_device,                  //  ComPtr 의 & 연산자 = "기존 것을 Release 하고 빈 칸의 주소를 줌"
				nullptr,                    // [출력] 실제 선택된 Feature Level. 필요 없어서 nullptr
				&m_context);

			};

		// HRESULT: COM/D3D 함수의 결과 코드. 성공/실패를 bool 대신 이걸로 알려준다
		// == 0 으로 비교X, SUCCEEDED() / FAILED() 매크로로 판정한다 (성공 코드도 여러 개라서)
		HRESULT hr = create(flags);

		// 디버그 레이어는 Windows 의 "그래픽 도구" 선택적 기능이 깔려 있어야 동작한다.
		//  없는 PC 면 생성 자체가 실패하므로, 디버그 옵션을 빼고 한 번 더 시도한다
		if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG))
		{
			OutputDebugStringW(L"[Graphics] 디버그 레이어 없음 → 끄고 재시도\n"); // VS 출력 창에 찍힌다
			hr = create(flags & ~D3D11_CREATE_DEVICE_DEBUG);   // ~ = 비트 반전. 그 비트만 끈다
		}
		if (FAILED(hr)) return false;

		// 3. 백버퍼에 RTV(이름표) 붙이기
		// 스왑체인이 만든 백버퍼는 그냥 텍스처라 바로는 그릴 수 없다.
		// "이 텍스처를 렌더 타겟으로 쓰겠다"는 View 를 만들어야 GPU 가 그 위에 그릴 수 있다
		ComPtr<ID3D11Texture2D> backBuffer; // 지역 ComPtr: 함수가 끝나면 자동 Release (RTV 가 따로 참조를 잡고 있어서 괜찮

		// GetBuffer(0, ...): 0번 백버퍼를 달라.
		//  IID_PPV_ARGS(&p): "받을 인터페이스 종류의 ID" + "받을 포인터 주소" 두 인자를 자동으로 만들어 주는 매크로
		//  → 타입을 잘못 적는 실수를 컴파일러가 막아 준다
		hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
		if (FAILED(hr)) return false;

		hr = m_device->CreateRenderTargetView(
			backBuffer.Get(), // 대상 텍스처
			nullptr, // View 설정. nullptr = 텍스처 형식 그대로
			&m_rtv); // [출력]
		if (FAILED(hr)) return false;

		// 4. 뷰포트
		//  NDC(-1~1) 좌표를 화면 픽셀 좌표로 바꿀 때 쓰는 영역. 지금은 화면 전체
		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = static_cast<float>(width);   // static_cast: 안전한 명시적 형 변환 (C 스타일 (float) 대신)
		m_viewport.Height = static_cast<float>(height);
		m_viewport.MinDepth = 0.0f;                        // 깊이 범위. 2D 라도 0~1 로 채워야 한다
		m_viewport.MaxDepth = 1.0f;

		return true;
	}

	void Graphics::BeginFrame(float r, float g, float b) {

		// OM = Output Merger: 파이프라인 마지막 단계. "결과를 어디에 쓸지" 지정
		// 여기서는 &m_rtv 가 아니라 GetAddressOf()를 사용
		// & 는 "Release 하고 빈 칸 주소"를 주므로 RTV 가 지워져 버린다
		// GetAddressOf() 는 그대로 둔 채 주소만 준다
		// Flip 모델은 Present 후 렌더 타겟 연결이 풀리므로 매 프레임 다시 연결

		m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr); // nullptr = 깊이 버퍼 없음 (2D 라 불필요)

		// RS = Rasterizer: 도형을 픽셀로 쪼개는 단계. 뷰포트는 여기에 설정한다
		m_context->RSSetViewports(1, &m_viewport);

		const float color[4] = { r, g, b, 1.0f };            // 마지막 1.0 = 알파(불투명)
		m_context->ClearRenderTargetView(m_rtv.Get(), color); // 백버퍼 전체를 이 색으로 칠한다
	}

	void Graphics::EndFrame()
	{
		// Present(SyncInterval, Flags)
		//  1 = VSync ON: 모니터 주사율(보통 60Hz)에 맞춰 기다린다
		//      → 루프가 초당 60번 정도로 제한되어 Step 2 의 CPU 과다 사용이 사라진다
		//  0 = 기다리지 않음 (티어링 발생 가능, 프레임 무제한)
		m_swapChain->Present(1, 0);
	}

}