//
// Created by hy on 2024/1/26.
//

#include "video_decoder.h"

namespace tc
{

    VideoDecoder::VideoDecoder() {

    }

    VideoDecoder::~VideoDecoder() {

    }

    int VideoDecoder::Init(int codec_type, int width, int height) {
        return -1;
    }

    int VideoDecoder::Decode(const std::shared_ptr<Data>& frame, DecodedCallback&& cbk) {
        return -1;
    }

    int VideoDecoder::Decode(const std::string& frame, DecodedCallback&& cbk) {
        return -1;
    }

    int VideoDecoder::Decode(const uint8_t* data, int size, DecodedCallback&& cbk) {
        return -1;
    }

    void VideoDecoder::Release() {
    }

    bool VideoDecoder::NeedReConstruct(int codec_type, int width, int height) {
        return codec_type != this->codec_type_ || width != this->frame_width_ || height != this->frame_height_;
    }


}
