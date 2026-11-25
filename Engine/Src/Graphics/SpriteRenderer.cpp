#include "SpriteRenderer.h"
#include "SpriteShader.h"
#include <d3dcompiler.h> // D3DCompile: HLSL 문자열 → GPU 기계어(바이트코드)
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

namespace Engine {

	// 이름 없는 namespace: 안의 것들은 "이 cpp 파일 안에서만" 보인다.
	//  다른 cpp 에 같은 이름(Vertex 등)이 있어도 충돌하지 않는다. (파일 전용 static 과 같은 효과)

	namespace {
		// 정점 하나의 메모리 모양. HLSL 의 VSInput 과 순서·크기가 맞아야 한다
		//  x,y      = float 2개 = 8바이트  (오프셋 0)
		//  r,g,b,a  = float 4개 = 16바이트 (오프셋 8)
		//  → 정점 하나 = 24바이트

		struct Vertex {
			float x, y;
			float r, g, b, a;
		};

		// HLSL 소스를 컴파일한다.
		//  entry : 진입점 함수 이름 ("VSMain" 등)
		//  target: 셰이더 종류+버전. "vs_5_0" = Vertex Shader 모델 5.0 (D3D11 용)
		//  outBlob: [출력] 컴파일 결과. ComPtr<>& = ComPtr 을 참조로 받아서 함수 안에서 채운다
		bool CompileShader(const char* entry, const char* target, ComPtr<ID3DBlob>& outBlob) {

			// Blob = 크기가 정해진 "바이트 덩어리"를 담는 COM 객체. 셰이더 바이트코드, 에러 메시지 등을 담는다
			UINT flags = D3DCOMPILE_ENABLE_STRICTNESS; // 옛날 문법 금지 (실수 방지)
#ifdef _DEBUG
			flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION; // 디버그용. 최적화하지 않음
#endif

			ComPtr<ID3DBlob> errorBlob; // 에러 메시지 담는 Blob
			HRESULT hr = D3DCompile(
				kSpriteShaderSource, // HLSL 소스 문자열
				std::strlen(kSpriteShaderSource), // 문자열 길이(바이트)
				"SpriteShader", // 디버그용 이름표. 에러 메시지에 이 이름이 붙는다
				nullptr, // #define 매크로 목록 (없음)
				nullptr, // #include 처리기 (없음)
				entry, target, flags, 0,
				&outBlob, // [출력] 성공 시 바이트코드
				&errorBlob // [출력] 실패 시 에러 메시지
			);

			if (FAILED(hr)) {
				// HLSL 문법 에러는 여기 메시지로 나온다 → VS "출력" 창에서 확인
				//  GetBufferPointer(): void* (타입 없는 주소) → 문자열로 쓰려고 const char* 로 변환
				if (errorBlob)
					OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
				return false;
			}
			return true;
		}
	}

	bool SpriteRenderer::Init(Graphics& graphics) {

		ID3D11Device* device = graphics.GetDevice(); // 자원 만들 때만 쓰므로 지역 변수로

		m_context = graphics.GetContext();

		// ---------- 1) 셰이더 컴파일 & 생성 ----------
		ComPtr<ID3DBlob> vsBlob, psBlob;
		if (!CompileShader("VSMain", "vs_5_0", vsBlob))
			return false;
		if (!CompileShader("PSMain", "ps_5_0", psBlob))
			return false;

		HRESULT hr = device->CreateVertexShader(
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), // 바이트코드 주소, 크기
			nullptr, // 클래스 링키지 (고급 기능, 안 씀)
			&m_vertexShader
		);
		if (FAILED(hr))
			return false;

		hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
			nullptr,
			&m_pixelShader
		);
		if (FAILED(hr))
			return false;

		// ---------- 2) 입력 레이아웃 ----------
		//  GPU 는 정점 버퍼를 그냥 "바이트 덩어리"로 본다.
		//  "앞 8바이트는 POSITION(float2), 그다음 16바이트는 COLOR(float4)" 라고 알려 주는 설명서
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			// 시맨틱 이름 | 시맨틱 번호 | 형식                          | 슬롯 | 오프셋 | 정점마다 읽음            | (인스턴싱용)
			{ "POSITION",  0,            DXGI_FORMAT_R32G32_FLOAT,       0,     0,       D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",     0,            DXGI_FORMAT_R32G32B32A32_FLOAT, 0,     8,       D3D11_INPUT_PER_VERTEX_DATA, 0 },
			//  R32G32_FLOAT       = 32비트 float 2개 (= float2)
			//  R32G32B32A32_FLOAT = 32비트 float 4개 (= float4)
			//  오프셋 8 = Vertex 구조체에서 r 이 시작하는 위치 (x,y 가 4바이트×2)
		};
		hr = device->CreateInputLayout(
			layout, _countof(layout),                            // _countof: 배열 원소 개수 (여기선 2)
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), // VS 바이트코드: 시맨틱 이름이 VS 입력과 맞는지 검사용
			&m_inputLayout);
		if (FAILED(hr))
			return false;

		// ---------- 3) 정점 버퍼 ----------
		//  정점 순서 = 시계 방향 (위 → 오른쪽 아래 → 왼쪽 아래)
		//  D3D 는 기본적으로 "시계 방향 = 앞면"으로 보고, 반시계 방향 삼각형은 뒷면이라 안 그린다 (컬링).
		//  순서를 거꾸로 하면 에러 없이 그냥 아무것도 안 보인다 → 흔한 함정
		Vertex vertices[] =
		{
			{  0.0f,  0.5f,   1.0f, 0.0f, 0.0f, 1.0f },  // 위        : 빨강
			{  0.5f, -0.5f,   0.0f, 1.0f, 0.0f, 1.0f },  // 오른쪽 아래: 초록
			{ -0.5f, -0.5f,   0.0f, 0.0f, 1.0f, 1.0f },  // 왼쪽 아래 : 파랑
		};

		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = sizeof(vertices); // 버퍼 크기 (24바이트 × 3 = 72)
		bd.Usage = D3D11_USAGE_IMMUTABLE; // GPU 전용. CPU → GPU 복사만 가능
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 정점 버퍼 용도

		D3D11_SUBRESOURCE_DATA initData{};
		initData.pSysMem = vertices; // 초기 데이터가 담긴 CPU 메모리 주소

		hr = device->CreateBuffer(&bd, &initData, &m_vertexBuffer); // CPU → GPU 메모리로 복사됨

		if (FAILED(hr))
			return false;

		return true;
	}

	void SpriteRenderer::Draw() {
		// ---------- IA: 정점을 어떻게 읽을지 ----------
		UINT stride = sizeof(Vertex); // 정점 하나의 크기. 다음 정점으로 넘어갈 때 몇 바이트 건너뛸지
		UINT offset = 0; // 정점 버퍼 시작 위치. 보통 0
		m_context->IASetInputLayout(m_inputLayout.Get()); // 입력 레이아웃
		m_context->IASetVertexBuffers(
			0, 1, // 0번 슬롯부터 1개
			m_vertexBuffer.GetAddressOf(), // 정점 버퍼 주소, 넘길 때는 & 가 아니라 GetAddressOf() 
			&stride, &offset
		);

		// TRIANGLELIST: 정점 3개씩 끊어서 삼각형 하나로 본다 (6개면 삼각형 2개)
		m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// ---------- VS / PS: 어떤 셰이더로 그릴지 ----------
		m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);  // 뒤 두 인자: 클래스 인스턴스 (안 씀)
		m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

		// ---------- 그려라 ----------
		m_context->Draw(3, 0);   // 정점 3개를, 0번째 정점부터
	}

}