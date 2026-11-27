#include "SpriteRenderer.h"
#include "SpriteShader.h"
#include <d3dcompiler.h> // D3DCompile: HLSL 문자열 → GPU 기계어(바이트코드)
#include <cstring>
#include <cstdint> // std::uint16_t (부호 없는 16비트 정수)


#pragma comment(lib, "d3dcompiler.lib")

namespace Engine {

	// 이름 없는 namespace: 안의 것들은 "이 cpp 파일 안에서만" 보인다.
	//  다른 cpp 에 같은 이름(Vertex 등)이 있어도 충돌하지 않는다. (파일 전용 static 과 같은 효과)

	namespace {
		// 위치 + UV. float 4개 = 16바이트
		struct Vertex {
			float x, y; // 위치 (오프셋 0)
			float u, v; // 텍스처 좌표 (오프셋 8)
		};

		// HLSL 의 cbuffer SpriteCB 와 똑같은 모양
		struct SpriteCB {
			DirectX::XMFLOAT4X4 transform; // 64바이트
			DirectX::XMFLOAT4 color; // 16바이트
			DirectX::XMFLOAT4 uvRect; // 16바이트 (x, y, width, height : UV 단위)  → 합계 96 = 16의 배수
		};

		// 상수 버퍼 크기는 반드시 16바이트의 배수여야 한다 (아니면 CreateBuffer 실패).
		// static_assert: 조건이 거짓이면 "컴파일 에러"를 낸다 → 실행 전에 실수를 잡는다
		static_assert(sizeof(SpriteCB) % 16 == 0, "상수 버퍼 크기는 16바이트 배수여야 합니다");

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
		//  "앞 8바이트는 POSITION(float2)
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			// 시맨틱 이름 | 시맨틱 번호 | 형식                          | 슬롯 | 오프셋 | 정점마다 읽음            | (인스턴싱용)
			{ "POSITION",  0,            DXGI_FORMAT_R32G32_FLOAT,       0,     0,       D3D11_INPUT_PER_VERTEX_DATA, 0 },
			//  R32G32_FLOAT       = 32비트 float 2개 (= float2)

			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },  // u,v 는 x,y 다음(8바이트)
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
		// 크기 1 사각형이라 위치와 UV 가 똑같다 (왼쪽 위 = 텍스처의 왼쪽 위)
		Vertex vertices[] =
		{
			{ 0.0f, 0.0f,   0.0f, 0.0f },   // 0: 왼쪽 위
			{ 1.0f, 0.0f,   1.0f, 0.0f },   // 1: 오른쪽 위
			{ 1.0f, 1.0f,   1.0f, 1.0f },   // 2: 오른쪽 아래
			{ 0.0f, 1.0f,   0.0f, 1.0f },   // 3: 왼쪽 아래
		};

		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = sizeof(vertices); // 버퍼 크기 (8바이트 × 4 = 32)
		bd.Usage = D3D11_USAGE_IMMUTABLE; // GPU 전용. CPU → GPU 복사만 가능
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 정점 버퍼 용도

		D3D11_SUBRESOURCE_DATA initData{};
		initData.pSysMem = vertices; // 초기 데이터가 담긴 CPU 메모리 주소

		hr = device->CreateBuffer(&bd, &initData, &m_vertexBuffer); // CPU → GPU 메모리로 복사됨

		if (FAILED(hr))
			return false;

		// ---------- 4) 인덱스 버퍼 ----------
		// 두 삼각형 모두 화면에서 시계 방향 (0→1→2 : 오른쪽→아래, 0→2→3 : 대각선→왼쪽)
		// uint16_t: 정점 번호가 65535 이하라 2바이트로 충분. 메모리 절약
		std::uint16_t indices[] = {
			0, 1, 2, // 삼각형 1
			0, 2, 3, // 삼각형 2
		};

		D3D11_BUFFER_DESC ibd{};
		ibd.ByteWidth = sizeof(indices); // 버퍼 크기 (2바이트 × 6 = 12)
		ibd.Usage = D3D11_USAGE_IMMUTABLE; // GPU 전용. CPU → GPU 복사만 가능
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER; // 인덱스 버퍼 용도

		D3D11_SUBRESOURCE_DATA iInit{};
		iInit.pSysMem = indices;

		hr = device->CreateBuffer(&ibd, &iInit, &m_indexBuffer);
		if (FAILED(hr))
			return false;

		// ---------- 5) 상수 버퍼 ----------
		D3D11_BUFFER_DESC cbd{};
		cbd.ByteWidth = sizeof(SpriteCB); // 상수 버퍼 크기, 96바이트 (16의 배수)
		cbd.Usage = D3D11_USAGE_DEFAULT; // DEFAULT: GPU 메모리지만 UpdateSubresource 로 내용 교체 가능
		//  (IMMUTABLE 은 생성 후 못 바꾸므로 안 된다)
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // 상수 버퍼 용도

		hr = device->CreateBuffer(&cbd, nullptr, &m_constantBuffer); // 초기 데이터 nullptr: 그릴 때마다 채운다
		if (FAILED(hr))
			return false;

		// ---------- 6) 직교 투영 행렬 ----------
		// OrthographicOffCenterLH(왼쪽, 오른쪽, 아래, 위, 가까운 z, 먼 z)
		// 왼쪽=0, 오른쪽=화면 너비 → x 가 -1 ~ +1 로
		// "아래"=화면 높이, "위"=0  → y 를 뒤집어서 위쪽(0)이 +1, 아래쪽(높이)이 -1 이 되게
		// LH = Left-Handed (D3D 기본 좌표계)
		float w = static_cast<float>(graphics.GetWidth());
		float h = static_cast<float>(graphics.GetHeight());
		DirectX::XMMATRIX proj = DirectX::XMMatrixOrthographicOffCenterLH(0.0f, w, h, 0.0f, 0.0f, 1.0f);
		DirectX::XMStoreFloat4x4(&m_projection, proj); // 계산용 XMMATRIX → 저장용 XMFLOAT4X4


		// ---------- 7) 샘플러 ----------
		D3D11_SAMPLER_DESC sd{};
		// POINT: 가장 가까운 픽셀 하나를 그대로 읽는다 → 확대해도 각진 도트 느낌 (픽셀아트에 적합)
		// LINEAR 로 바꾸면 주변 픽셀을 섞어서 부드럽게(대신 흐릿하게) 확대된다
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		// CLAMP: UV 가 0~1 을 벗어나면 가장자리 색을 쓴다 (WRAP 은 타일처럼 반복)
		sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; // 3D 텍스처용이지만 0 이면 에러라 채워 둔다

		sd.ComparisonFunc = D3D11_COMPARISON_NEVER; // 그림자 비교용 (안 씀)
		sd.MaxLOD = D3D11_FLOAT32_MAX; // 밉맵 단계 제한 없음 (기본값 관례)

		hr = device->CreateSamplerState(&sd, &m_sampler);
		if (FAILED(hr)) return false;

		// ---------- 8) 1x1 흰색 텍스처 ----------
		//  0xFFFFFFFF = 4바이트 모두 255 → R=G=B=A=255 (흰색, 불투명)
		//  0x 는 16진수 표기. FF = 255
		const std::uint32_t white = 0xFFFFFFFF;
		if (!m_whiteTexture.Create(device, 1, 1, &white)) return false;

		// ---------- 9) 블렌드 스테이트 (알파 블렌딩) ----------
		// 결과 = Src × SrcBlend  (BlendOp)  Dest × DestBlend
		// Src  = 지금 PS 가 낸 새 색,  Dest = 백버퍼에 이미 있는 색
		D3D11_BLEND_DESC bld{};
		bld.AlphaToCoverageEnable = FALSE; // MSAA 용 기법 (안 씀)
		bld.IndependentBlendEnable = FALSE; // FALSE = RenderTarget[0] 설정을 모든 렌더 타겟에 공통 적용

		// rt 를 고치면 bld.RenderTarget[0] 이 직접 바뀐다 (& 를 빼면 복사본만 바뀌고 원본은 그대로 → 블렌딩 안 됨)
		auto& rt = bld.RenderTarget[0];
		rt.BlendEnable = TRUE;

		// RGB 섞는 식:  새색 × a  +  기존색 × (1 - a)
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA; // × 새 색의 알파
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA; // × (1 - 새 색의 알파)   INV = inverse(1 에서 뺀 값)
		rt.BlendOp = D3D11_BLEND_OP_ADD; // 두 항을 더한다

		// 알파 채널 자체를 섞는 식 (백버퍼 알파). 화면 표시에는 거의 영향 없음, 관례적인 값
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;

		// 어느 채널(R,G,B,A)에 쓸지. ALL = 전부 (0 이면 아무것도 안 그려진다)
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		hr = device->CreateBlendState(&bld, &m_blendState);
		if (FAILED(hr))
			return false;

		// 참고: WIC 에서 32bppRGBA 로 읽었으니 "스트레이트 알파"(RGB 와 A 가 따로) 이미지다.
		//  RGB 에 미리 A 를 곱해 둔 "프리멀티플라이드 알파"(PRGBA) 는 SrcBlend 를 ONE 으로 써야 한다
		//  → 이미지 형식과 블렌드 식이 짝이 안 맞으면 가장자리가 어둡거나 밝게 테두리가 생긴다


		return true;
	}

	void SpriteRenderer::DrawQuad(ID3D11ShaderResourceView* srv, const Rect& dest, const Rect& uv, const Color& color)   // 기본 인자(= 1.0f)는 여기 다시 쓰지 않는다
	{
		// 이 함수 안에서만 DirectX:: 를 생략 
		// 헤더에 using namespace 를 쓰면
		// include 하는 모든 파일로 퍼지므로, 범위를 좁게 함수 안에서만 쓴다
		using namespace DirectX;

		// ---------- 변환 행렬 만들기 ----------
		//  크기 1 사각형을 → (width, height) 로 늘리고 → (x, y) 로 옮기고 → NDC 로 투영
		//  DirectXMath 는 "왼쪽부터 순서대로" 적용된다: Scale * Translate * Proj
		//  순서를 바꿔서 Translate 먼저 하면 이동한 거리까지 같이 늘어나 버린다
		XMMATRIX world = XMMatrixScaling(dest.width, dest.height, 1.0f) * XMMatrixTranslation(dest.x, dest.y, 0.0f);
		XMMATRIX proj = XMLoadFloat4x4(&m_projection);

		SpriteCB cb;
		// Transpose(전치: 행과 열 바꾸기)해서 보낸다.
		// DirectXMath 는 행렬을 "가로줄(행) 단위"로 메모리에 두고,
		// HLSL 은 기본적으로 "세로줄(열) 단위"로 읽는다. 그대로 보내면 뒤집혀 읽혀서 엉뚱한 곳에 그려진다
		XMStoreFloat4x4(&cb.transform, XMMatrixTranspose(world * proj));
		cb.color = XMFLOAT4(color.r, color.g, color.b, color.a);
		cb.uvRect = XMFLOAT4(uv.x, uv.y, uv.width, uv.height);

		// CPU 의 cb 내용을 GPU 상수 버퍼로 복사
		// (대상, 서브리소스 번호, 일부만 복사할 범위(nullptr=전체), 원본 주소, 행 간격, 면 간격) — 뒤 둘은 텍스처용이라 0
		m_context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);

		// ---------- IA ----------
		UINT stride = sizeof(Vertex); // 이제 16바이트
		UINT offset = 0;
		m_context->IASetInputLayout(m_inputLayout.Get());
		m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
		// 인덱스 형식 R16_UINT = uint16_t 와 짝. 마지막 0 = 버퍼 시작 오프셋
		m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
		m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 삼각형 2개로 사각형

		// ---------- 셰이더 + 상수 버퍼 연결 ----------
		m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0); // 클래스 링키지 안 씀
		m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf()); // b0 ← transform 을 VS 가 씀
		m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
		m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());  // b0 ← color 를 PS 가 씀
		// VS 와 PS 는 따로따로 연결해야 한다. PS 쪽을 빼먹으면 color 가 0(검정·투명)으로 읽힌다
		// t0 ← 텍스처. 배열을 받는 함수라 "포인터의 주소"를 넘긴다 (srv 는 지역 생포인터라 & 로 OK)
		m_context->PSSetShaderResources(0, 1, &srv);
		// s0 ← 샘플러 (ComPtr 이라 GetAddressOf)
		m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

		// ---------- OM: 블렌딩 ----------
		// (블렌드 스테이트, 블렌드 팩터, 샘플 마스크)
		// 블렌드 팩터: BLEND_FACTOR 를 쓸 때만 의미 있음. nullptr = (1,1,1,1)
		// 0xFFFFFFFF: 샘플 마스크. MSAA 샘플 중 어느 것에 쓸지 → 전부
		// 매 Draw 마다 꽂는 건 낭비지만, 다른 코드가 상태를 바꿔도 안전하다 (나중에 배칭 때 정리)
		m_context->OMSetBlendState(m_blendState.Get(), nullptr, 0xFFFFFFFF);




		// ---------- 그리기 ----------
		//  Draw 대신 DrawIndexed: (인덱스 개수, 시작 인덱스, 정점 번호에 더할 값)
		m_context->DrawIndexed(6, 0, 0);

	}

	// 텍스처 전체 = UV (0,0) ~ (1,1)
	// 이름 없는 namespace 안이 아니라 여기 둬도 되지만, 이 파일 전용 상수라 static constexpr 로 범위를 좁힌다
	static constexpr Rect kFullUV{ 0.0f, 0.0f, 1.0f, 1.0f };

	void SpriteRenderer::DrawRect(const Rect& dest, const Color& color)
	{
		DrawQuad(m_whiteTexture.GetSRV(), dest, kFullUV, color); // 흰 텍스처 × color = 단색
	}

	void SpriteRenderer::DrawSprite(const Texture& texture, const Rect& dest, const Color& color)
	{
		DrawQuad(texture.GetSRV(), dest, kFullUV, color);
	}

	void SpriteRenderer::DrawSprite(const Texture& texture, const Rect& dest, const Rect& source, const Color& color)
	{
		// 픽셀 단위 source → UV 단위 (0~1) 로 변환: 텍스처 크기로 나눈다
		// GetWidth() 는 UINT(정수) → float 로 바꿔야 정수 나눗셈(40 / 160 = 0)이 되지 않는다
		float texW = static_cast<float>(texture.GetWidth());
		float texH = static_cast<float>(texture.GetHeight());

		Rect uv{
			source.x / texW,
			source.y / texH,
			source.width / texW,
			source.height / texH
		};
		DrawQuad(texture.GetSRV(), dest, uv, color);
	}

}