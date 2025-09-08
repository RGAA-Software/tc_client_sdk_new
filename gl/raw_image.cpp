#include "raw_image.h"
#include "tc_common_new/log.h"

namespace tc
{

	std::shared_ptr<RawImage> RawImage::Make(char* data, int size, int width, int height, int ch, RawImageFormat format) {
		return std::make_shared<RawImage>(data, size, width, height, ch, format);
	}

	std::shared_ptr<RawImage> RawImage::MakeRGB(char* data, int size, int width, int height) {
		return std::make_shared<RawImage>(data, size, width, height, 3, RawImageFormat::kRawImageRGB);
	}

	std::shared_ptr<RawImage> RawImage::MakeRGBA(char* data, int size, int width, int height) {
		return std::make_shared<RawImage>(data, size, width, height, 4, RawImageFormat::kRawImageRGBA);
	}

	std::shared_ptr<RawImage> RawImage::MakeNV12(char* data, int size, int width, int height) {
		return std::make_shared<RawImage>(data, size, width, height, -1, RawImageFormat::kRawImageNV12);
	}

	std::shared_ptr<RawImage> RawImage::MakeI420(char* data, int size, int width, int height) {
		return std::make_shared<RawImage>(data, size, width, height, -1, RawImageFormat::kRawImageI420);
	}
	
	std::shared_ptr<RawImage> RawImage::MakeI444(char* data, int size, int width, int height) {
		return std::make_shared<RawImage>(data, size, width, height, -1, RawImageFormat::kRawImageI444);
	}

#ifdef WIN32
    std::shared_ptr<RawImage> RawImage::MakeD3D11Texture(ComPtr<ID3D11Texture2D> texture, int src_subresource) {
        auto image = std::make_shared<RawImage>(nullptr, 0, 0, 0, 0, RawImageFormat::kRawImageD3D11Texture);
        image->texture_ = texture;
        image->src_subresource_ = src_subresource;
        return image;
    }
#endif

	RawImage::RawImage(char* data, int size, int width, int height, int ch, RawImageFormat format) {
        if (size > 0) {
            img_buf = (char *) malloc(size);
        }
		if (img_buf && data) {
			memcpy(img_buf, data, size);
		}
		img_buf_size = size;
		img_width = width;
		img_height = height;
		img_ch = ch;
		img_format = format;
	}

	RawImage::~RawImage() {
		if (img_buf) {
			free(img_buf);
		}
	}

	char* RawImage::Data() {
		return img_buf;
	}

	int RawImage::Size() {
		return img_buf_size;
	}

	RawImageFormat RawImage::Format() {
		return img_format;
	}

	std::shared_ptr<RawImage> RawImage::Clone() {
		return std::make_shared<RawImage>(img_buf, img_buf_size, img_width, img_height, img_ch, img_format);
	}

	void RawImage::CopyTo(const std::shared_ptr<RawImage>& target) {
		if (target->Size() >= this->Size()) {
			memcpy(target->Data(), this->Data(), this->Size());
		}
	}

}