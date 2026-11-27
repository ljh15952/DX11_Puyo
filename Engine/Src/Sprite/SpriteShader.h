#pragma once

namespace Engine {
	// ---------------------------------------------------------------
	// 스프라이트용 HLSL 셰이더 소스
	//
	// ■ 왜 .hlsl 파일이 아니라 C++ 문자열인가?
	//   파일로 두면 실행할 때 "exe 기준 어디에 있나" 경로 문제가 생긴다.
	//   엔진 내장 셰이더는 문자열로 넣어 두면 lib 안에 같이 들어가서 경로 걱정이 없다.
	//
	// ■ R"( ... )" = Raw 문자열 리터럴 (C++11)
	//   안에 줄바꿈, " , \ 를 그대로 쓸 수 있다. 여러 줄 코드를 넣기에 좋다.
	//
	// ■ inline constexpr: 헤더에 둔 상수를 여러 cpp 가 include 해도
	//   "같은 것이 여러 개 정의됨" 링크 에러가 나지 않게 한다 (C++17)
	//
	// ---- HLSL 설명 ----
	//  float2 / float4      : float 2개 / 4개짜리 벡터 타입 (x,y / x,y,z,w 또는 r,g,b,a)
	//  struct VSInput       : 정점 버퍼에서 읽어 올 정점 하나의 모양
	//  : POSITION, : COLOR  : "시맨틱(semantic)". 이 변수가 무슨 데이터인지 붙이는 이름표.
	//                         C++ 의 입력 레이아웃(아래 SpriteRenderer.cpp)과 이 이름으로 짝을 맞춘다
	//  : SV_POSITION        : SV = System Value. "이게 화면 위치다"라고 GPU 에 알리는 예약 이름표.
	//                         VS 출력에 반드시 있어야 래스터라이저가 삼각형을 그릴 수 있다
	//  : SV_TARGET          : PS 가 반환한 색을 "렌더 타겟(백버퍼)에 써라"
	//  float4(pos, 0, 1)    : 2D 위치에 z=0, w=1 을 붙여 4D 로 만든다 (w=1 은 "점"이라는 뜻)
	//  VSMain / PSMain      : 진입점(main 함수 같은 것). 이름은 자유이고 컴파일할 때 지정한다
	//  VS 출력의 color      : 정점 3개 사이의 픽셀은 색이 자동으로 "섞여서(보간)" PS 로 들어온다
	// ---------------------------------------------------------------

	// ---- 5-2 추가 설명 ----
	//  cbuffer SpriteCB : register(b0)
	//      상수 버퍼 선언. C++ 쪽 SpriteCB 구조체와 "순서·크기"가 똑같아야 한다.
	//      register(b0) = 상수 버퍼 0번 슬롯. C++ 의 VSSetConstantBuffers(0, ...) 의 0 과 짝
	//  float4x4         : 4x4 행렬
	//  mul(v, M)        : 벡터 v 에 행렬 M 을 곱한다 (행 벡터 방식: v 가 왼쪽)
	//  정점 색(COLOR)을 없애고, 색은 상수 버퍼의 color 하나로 사각형 전체를 칠한다

	// ---- 5-3 추가 설명 ----
	//  Texture2D    spriteTexture : register(t0)  텍스처. t0 = 텍스처(SRV) 0번 슬롯 ← PSSetShaderResources(0, ...)
	//  SamplerState spriteSampler : register(s0)  샘플러. s0 = 샘플러 0번 슬롯    ← PSSetSamplers(0, ...)
	//  : TEXCOORD           UV 좌표용 시맨틱 (TEXture COORDinate)
	//  tex.Sample(s, uv)    uv 위치의 색을 샘플러 규칙대로 읽는다 (float4 RGBA, 0.0~1.0)
	//  * color              틴트(tint). 텍스처 색에 곱한다. 흰색(1,1,1,1)이면 원본 그대로
	//  VS 가 내보낸 uv 도 color 때처럼 정점 사이에서 보간되어 PS 로 들어온다

	// ---- 5-4 추가 설명 ----
	//  uvRect (float4)   텍스처에서 읽을 영역 (UV 단위). xy = 시작 위치, zw = 크기
	//                    전체 이미지 = (0, 0, 1, 1)
	//  .xy / .zw         "스위즐(swizzle)": float4 의 성분을 골라 꺼내는 문법. .zw = 3,4번째 성분
	//  input.uv * uvRect.zw + uvRect.xy
	//                    0~1 의 UV 를 "크기만큼 줄이고 → 시작 위치로 옮긴다" (정점 위치의 Scale→Translate 와 같은 원리)

	inline constexpr const char* kSpriteShaderSource = R"(

cbuffer SpriteCB : register(b0){
	float4x4 transform;
	float4 color;
	float4 uvRect;
};

Texture2D spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);

struct VSInput{
	float2 pos : POSITION;
	float2 uv : TEXCOORD;
};

struct PSInput{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

PSInput VSMain(VSInput input){
	PSInput output;
	output.pos = mul(float4(input.pos, 0.0f, 1.0f), transform);
	output.uv = input.uv * uvRect.zw + uvRect.xy;
	return output;
}

float4 PSMain(PSInput input) : SV_TARGET{
	return spriteTexture.Sample(spriteSampler, input.uv) * color;
}
)";

}