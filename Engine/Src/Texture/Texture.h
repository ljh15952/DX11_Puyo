#pragma once
#include "..\Graphics\Graphics.h"

namespace Engine {

	class Texture {
	public:

		Texture() = default;
		Texture(const Texture&) = delete; // GPU 자원은 주인이 하나
		Texture& operator=(const Texture&) = delete;

		// 이미지 파일(PNG/JPG/BMP...)을 읽어 텍스처를 만든다
		bool Load(ID3D11Device* device, const wchar_t* path);

		// 메모리의 RGBA 픽셀 배열로 텍스처를 만든다 (Load 도 내부에서 이걸 쓴다)
		//  const void*: "타입 상관없이 아무 데이터의 주소, 읽기만 함". 바이트 배열이든 uint32_t 든 받을 수 있다
		bool Create(ID3D11Device* device, UINT width, UINT height, const void* rgbaPixels);

		ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
		UINT GetWidth() const { return m_width; } // UINT = unsigned int
		UINT GetHeight() const { return m_height; }

	private:
		// 텍스처 본체(ID3D11Texture2D)는 따로 들고 있지 않는다.
		// SRV 가 내부에서 본체의 참조(AddRef)를 잡고 있어서, SRV 만 살아 있으면 본체도 살아 있다
		ComPtr<ID3D11ShaderResourceView> m_srv;
		UINT m_width = 0;
		UINT m_height = 0;
	};

}