#pragma once
#include "Graphics.h"

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

namespace Engine {

    class SpriteRenderer {

    public:
        SpriteRenderer() = default;
        SpriteRenderer(const SpriteRenderer&) = delete;
        SpriteRenderer& operator=(const SpriteRenderer&) = delete;

        bool Init(Graphics& graphics);
        void Draw();

    private:
        //빌린 포인터(소유하지 않음).그래서 ComPtr 이 아니라 생포인터로 둔다.
        //  Graphics 가 이 객체보다 먼저 사라지면 안 된다 → main 에서 선언 순서로 보장
        ID3D11DeviceContext* m_context = nullptr;

        ComPtr<ID3D11VertexShader> m_vertexShader; // 컴파일된 VS
        ComPtr<ID3D11PixelShader> m_pixelShader; // 컴파일된 PS
        ComPtr<ID3D11InputLayout> m_inputLayout; // "정점 버퍼의 바이트를 어떻게 읽을지" 설명서
        ComPtr<ID3D11Buffer> m_vertexBuffer; // GPU 메모리에 올린 정점 데이터
    };

}