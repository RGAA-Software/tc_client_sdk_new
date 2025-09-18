#pragma once
#include <memory>

#ifdef WIN32
#include <d3d11.h>
#include <atlbase.h>
#include <wrl/client.h>
#include <string>
#include <dxgi.h>
#include <DXGI1_2.h>

using namespace Microsoft::WRL;

#endif

namespace tc
{
	enum RawImageFormat {
		kRawImageRGB,
		kRawImageRGBA,
		kRawImageNV12,
		kRawImageI420,
		kRawImageI444,
        kRawImageD3D11Texture,
		kRawImageVulkan,
	};

	class RawImage {
	public:
		static std::shared_ptr<RawImage> Make(char* data, int size, int width, int height, int ch, RawImageFormat format);
		static std::shared_ptr<RawImage> MakeRGB(char* data, int size, int width, int height);
		static std::shared_ptr<RawImage> MakeRGBA(char* data, int size, int width, int height);
		static std::shared_ptr<RawImage> MakeNV12(char* data, int size, int width, int height);
		static std::shared_ptr<RawImage> MakeI420(char* data, int size, int width, int height);
		static std::shared_ptr<RawImage> MakeI444(char* data, int size, int width, int height);
#ifdef WIN32
        static std::shared_ptr<RawImage> MakeD3D11Texture(ComPtr<ID3D11Texture2D> texture, int src_subresource);
#endif

		RawImage(char* data, int size, int width, int height, int ch, RawImageFormat format);
		~RawImage();

		char* Data();
		int Size();
		RawImageFormat Format();

		std::shared_ptr<RawImage> Clone();
		void CopyTo(const std::shared_ptr<RawImage>& target);

	public:

		char* img_buf = nullptr;
		int img_buf_size = 0;
		int img_width = 0;
		int img_height = 0;
		int img_ch = -1;
		RawImageFormat img_format;

#ifdef WIN32
        ComPtr<ID3D11Device> device_ = nullptr;
        ComPtr<ID3D11DeviceContext> device_context_ = nullptr;
        ComPtr<ID3D11Texture2D> texture_ = nullptr;
        int src_subresource_ = 0;
#endif

	};

	typedef std::shared_ptr<RawImage> RawImagePtr;
}