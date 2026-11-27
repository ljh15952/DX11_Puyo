#include "Texture.h"
#include <wincodec.h> // WIC (Windows Imaging Component): 이미지 파일 디코더
#include <vector> // std::vector: 크기가 실행 중에 정해지는 배열 (이미지 크기는 파일을 열어 봐야 안다)
#include <cstdint> // std::uint8_t (0~255, 1바이트)

// CLSID_WICImagingFactory 같은 WIC 식별자의 실제 값이 이 lib 에 들어 있다
#pragma comment(lib, "windowscodecs.lib")

namespace Engine {

	bool Texture::Create(ID3D11Device* device, UINT width, UINT height, const void* rgbaPixels) {
		D3D11_TEXTURE2D_DESC td{};
		td.Width = width;
		td.Height = height;
		td.MipLevels = 1; // 밉맵 = 축소용으로 미리 만든 작은 이미지들. 2D 스프라이트라 원본 1장만
		td.ArraySize = 1; // 텍스처 배열 아님, 1장
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 픽셀 = R,G,B,A 각 1바이트 (WIC 변환 형식과 짝)
		td.SampleDesc.Count = 1; // MSAA 안 씀 (0 이면 에러)
		td.Usage = D3D11_USAGE_IMMUTABLE; // 한 번 만들면 안 바꿈 → 생성 시 데이터 필수
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE; // 셰이더가 읽는 용도 (SRV 를 만들 수 있게)
		D3D11_SUBRESOURCE_DATA init{};
		init.pSysMem = rgbaPixels;
		// SysMemPitch: 이미지 "한 줄"의 바이트 수. GPU 가 다음 줄이 어디서 시작하는지 알기 위해 필요
		// 가로 픽셀 수 × 4바이트(RGBA). 버퍼(1차원)에는 없던 값이고 텍스처(2차원)라서 필요
		init.SysMemPitch = width * 4;

		ComPtr<ID3D11Texture2D> texture; // 지역 변수: 아래 SRV 가 참조를 잡으므로 함수가 끝나도 안 사라진다
		HRESULT hr = device->CreateTexture2D(&td, &init, &texture);
		if (FAILED(hr))
			return false;

		// nullptr = 텍스처 형식 그대로 보는 기본 SRV
		hr = device->CreateShaderResourceView(texture.Get(), nullptr, &m_srv);
		if (FAILED(hr))
			return false;

		m_width = width;
		m_height = height;
		return true;
	}

	bool Texture::Load(ID3D11Device* device, const wchar_t* path) {

		// ---------- 1) WIC 공장 만들기 ----------
		//  CoCreateInstance: COM 객체를 "클래스 ID(CLSID)"로 만든다. (COM 판 new)
		//  CLSCTX_INPROC_SERVER: 우리 프로세스 안에 DLL 로 불러와서 만든다
		//  COM 초기화(CoInitializeEx)가 안 되어 있으면 여기서 실패한다 → Window::Create 에서 해 둔 이유
		ComPtr<IWICImagingFactory> factory;
		HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&factory));
		if (FAILED(hr))
			return false;

		// ---------- 2) 파일 열기 → 디코더 ----------
		//  디코더: 압축된 PNG/JPG 를 풀어 주는 객체. 확장자가 아니라 파일 내용을 보고 형식을 알아서 고른다
		ComPtr<IWICBitmapDecoder> decoder;
		hr = factory->CreateDecoderFromFilename(
			path,
			nullptr, // 특정 디코더 지정 안 함 (자동)
			GENERIC_READ, // 읽기 전용으로 연다
			WICDecodeMetadataCacheOnDemand, // 메타데이터(촬영 정보 등)는 필요할 때만 읽는다
			&decoder
		);

		if (FAILED(hr))
			return false; // ← 경로가 틀리면 대부분 여기서 실패

		// ---------- 3) 첫 번째 프레임 ----------
		//  GIF 처럼 여러 장이 든 형식도 있어서 "몇 번째 장"을 고른다. PNG 는 0번 1장뿐
		ComPtr<IWICBitmapFrameDecode> frame;
		hr = decoder->GetFrame(0, &frame);
		if (FAILED(hr))
			return false;

		// ---------- 4) 픽셀 형식 통일 ----------
		//  PNG 는 24비트, 32비트, 팔레트 등 형식이 제각각이다.
		//  텍스처 형식(R8G8B8A8)에 맞게 무조건 "RGBA 각 8비트"로 변환한다
		ComPtr<IWICFormatConverter> converter;
		hr = factory->CreateFormatConverter(&converter);
		if (FAILED(hr))
			return false;

		hr = converter->Initialize(
			frame.Get(), // 원본
			GUID_WICPixelFormat32bppRGBA, // 목표 형식: 32비트 RGBA (GUID = 형식을 나타내는 고유 ID)
			WICBitmapDitherTypeNone, // 디더링(색 줄일 때 점 섞기) 안 함
			nullptr, 0.0, // 팔레트 관련 (안 씀)
			WICBitmapPaletteTypeCustom
		);
		if (FAILED(hr))
			return false;

		// ---------- 5) 픽셀을 CPU 배열로 복사 ----------
		UINT width = 0, height = 0;
		converter->GetSize(&width, &height);

		// static_cast<size_t>: 곱하기 전에 큰 타입으로 바꿔서, 큰 이미지에서 UINT 곱셈이 넘치지 않게 한다
		std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * height * 4);
		hr = converter->CopyPixels(
			nullptr, // 복사할 영역 (nullptr = 전체)
			width * 4, // 한 줄 바이트 수 (Texture2D 의 SysMemPitch 와 같은 개념)
			static_cast<UINT>(pixels.size()), // 배열 전체 크기
			pixels.data() // .data(): vector 안의 실제 배열 주소
		);
		if (FAILED(hr))
			return false;

		// ---------- 6) GPU 텍스처 생성 ----------
		// pixels(vector) 는 이 함수가 끝나면 사라지지만 괜찮다 → Create 안에서 이미 GPU 로 복사됐다
		return Create(device, width, height, pixels.data());
	}


}
