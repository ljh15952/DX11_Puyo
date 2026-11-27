#pragma once

namespace Engine {

    // ---------------------------------------------------------------
    // 엔진 공용 기본 타입
    //  struct: 멤버가 기본 public 인 class. "데이터 묶음"에는 struct 를 쓰는 관례
    //  = 0.0f 같은 "멤버 기본값": Rect r; 처럼 값 없이 만들어도 쓰레기값이 아니라 0 이 들어간다
    //  Rect{ 10, 20, 40, 40 } 처럼 중괄호로 멤버 순서대로 채울 수 있다 ("집합체 초기화")
    // ---------------------------------------------------------------

    // 사각형: 왼쪽 위 (x, y) + 크기. 화면 좌표(픽셀)에도, 시트 영역(픽셀)에도, UV(0~1)에도 쓴다
    struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    // 색: 각 0.0 ~ 1.0. 기본값 = 흰색 불투명
    struct Color {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };

    // 자주 쓰는 색 상수
    // namespace Colors 로 묶어서 Colors::White 처럼 쓴다 (이름 충돌 방지 + 어디 소속인지 명확)
    // inline constexpr: 헤더에 둔 상수가 여러 cpp 에서 include 돼도 링크 에러가 나지 않게 (SpriteShader.h 와 같은 이유)
    // 엔진에는 범용 색만 둔다. "뿌요 빨강" 같은 게임 전용 색은 게임(Pu) 쪽에 둔다
    namespace Colors
    {
        inline constexpr Color White{ 1.0f, 1.0f, 1.0f, 1.0f };
        inline constexpr Color Black{ 0.0f, 0.0f, 0.0f, 1.0f };
    }
}