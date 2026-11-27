#pragma once
#include "..\Graphics\Graphics.h"
#include <DirectXMath.h> // MS 의 SIMD 수학 라이브러리 (벡터·행렬). 헤더만 있어서 lib 링크 불필요
#include "..\Texture\Texture.h"

/*
단계	내용										새로 배우는 것
5-1		색깔 삼각형 1개 그리기						렌더링 파이프라인, 셰이더, 정점 버퍼, 입력 레이아웃
5-2		사각형을 픽셀 좌표로 그리기					인덱스 버퍼, 상수 버퍼, 직교 투영
5-3		이미지 파일 붙이기							텍스처 로드(WIC), 샘플러, UV
5-4		DrawSprite(텍스처, x, y, ...) API 만들기	엔진 API 설계
5-5		투명한 부분 처리							알파 블렌딩
*/

/*
렌더링 파이프라인
GPU는 정해진 순서의 공장 라인로 그림을 그림
우리는 각 공정에 재료와 설정을 꽂아 준 다음 Draw를 호출
[정점 데이터]
	↓
IA  (Input Assembler)   정점들을 읽어서 삼각형으로 묶는다      ← 정점 버퍼, 입력 레이아웃
	↓
VS  (Vertex Shader)     정점 하나하나의 "화면 위치"를 계산
	↓
RS  (Rasterizer)        삼각형을 픽셀 단위로 쪼갠다            ← 뷰포트
	↓
PS  (Pixel Shader)      픽셀 하나하나의 "색"을 계산
	↓
OM  (Output Merger)     결과를 백버퍼에 쓴다                  ← RTV

셰이더는 GPU에서 실행되는 작은 프로그램
좌표 (NDC): 셰이더가 내보내는 위치는 픽셀이 아니라
	화면 가운데가 (0,0), 왼쪽 끝이 -1, 오른쪽 끝이 +1, 위가 +1, 아래가 -1인 좌표
*/

/*
사각형을 픽셀 좌표로 그리기

① 인덱스 버퍼: 사각형은 삼각형 2개라서 정점이 6개 필요
그런데 그중 2개는 겹침. 그래서 정점은 4개만 만들고,
"몇 번 정점들로 삼각형을 만들지" 번호 목록(인덱스)을 따로 부여
0 ──── 1        삼각형 A: 0 → 1 → 2  (시계 방향)
│ ╲     │        삼각형 B: 0 → 2 → 3  (시계 방향)
│   ╲   │        인덱스 = {0,1,2, 0,2,3}
3 ──── 2

② 상수 버퍼 (Constant Buffer):
CPU에서 셰이더로 "이번 그리기에 쓸 값"(위치, 크기, 색)을 보내는 작은 버퍼
정점 버퍼는 그대로 두고, 이 값만 바꿔 가며 같은 사각형을 여러 번 그림

③ 직교 투영 (Orthographic): 픽셀 좌표를 NDC로 바꾸는 행렬
픽셀:  x 0 ~ 800 (오른쪽 +)     y 0 ~ 600 (아래 +)
NDC:  x -1 ~ +1                 y +1 ~ -1  (위가 +, 뒤집힘)

전체 방식: 정점 버퍼에는 (0,0)~(1,1) 크기 1짜리 사각형을 한 번만 올려 두고,
그릴 때마다 행렬로 "크기 조절 → 이동 → 투영" 을 해서 원하는 위치와 크기로 바꿈
*/

namespace Engine {

	class SpriteRenderer {

	public:
		SpriteRenderer() = default;
		SpriteRenderer(const SpriteRenderer&) = delete;
		SpriteRenderer& operator=(const SpriteRenderer&) = delete;

		bool Init(Graphics& graphics);

		// (x, y) = 사각형 왼쪽 위 픽셀 좌표, width/height = 픽셀 크기, rgba = 0.0~1.0
		//  float a = 1.0f : 기본 인자. 호출할 때 a 를 생략하면 1.0f(불투명)가 들어간다.
		//                   기본 인자는 선언(헤더)에만 쓰고, cpp 정의에는 다시 쓰지 않는다
		void DrawRect(float x, float y, float width, float height,
			float r, float g, float b, float a = 1.0f);

		// 텍스처를 (x, y) 에 width x height 크기로 그린다. rgba = 틴트 (기본 흰색 = 원본 색)
		//  const Texture& : 복사 없이 빌려 보기만 한다 (const = 안 바꾼다는 약속)
		void DrawTexture(const Texture& texture, float x, float y, float width, float height,
			float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);


	private:
		//빌린 포인터(소유하지 않음).그래서 ComPtr 이 아니라 생포인터로 둔다.
		//  Graphics 가 이 객체보다 먼저 사라지면 안 된다 → main 에서 선언 순서로 보장
		ID3D11DeviceContext* m_context = nullptr;

		ComPtr<ID3D11VertexShader> m_vertexShader; // 컴파일된 VS
		ComPtr<ID3D11PixelShader> m_pixelShader; // 컴파일된 PS
		ComPtr<ID3D11InputLayout> m_inputLayout; // "정점 버퍼의 바이트를 어떻게 읽을지" 설명서
		ComPtr<ID3D11Buffer> m_vertexBuffer; // GPU 메모리에 올린 정점 데이터

		ComPtr<ID3D11Buffer> m_indexBuffer; // 사각형 = 삼각형 2개를 만들 정점 번호 목록
		ComPtr<ID3D11Buffer> m_constantBuffer; // "이번 그리기에 쓸 값"을 담는 작은 버퍼(변환 행렬, 색)

		// 행렬을 멤버로 둘 때는 XMMATRIX 가 아니라 XMFLOAT4X4 로 저장한다.
		// XMMATRIX 는 SIMD 계산용이라 메모리 주소가 16바이트 단위로 맞아야(정렬) 한다.
		// 클래스 멤버나 new 로 만든 객체 안에서는 이게 보장되지 않아 크래시가 날 수 있다.
		// → 저장은 XMFLOAT4X4 (일반 float 16개), 계산할 때만 XMMATRIX 로 불러온다(Load)
		DirectX::XMFLOAT4X4 m_projection{}; // 픽셀 좌표 → NDC 직교 투영 행렬 (Init 에서 한 번 계산)

		// DrawRect / DrawTexture 가 공통으로 쓰는 실제 그리기 함수.
		//  밖에서 쓸 일이 없어서 private (사용자는 위 두 함수만 알면 된다)
		void DrawQuad(ID3D11ShaderResourceView* srv, float x, float y, float width, float height,
			float r, float g, float b, float a);

		ComPtr<ID3D11SamplerState> m_sampler; // 텍스처 읽는 규칙
		Texture m_whiteTexture; // 1x1 흰색. DrawRect(단색) 용
	};

}