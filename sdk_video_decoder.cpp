//
// Created by RGAA on 2024/1/26.
//

#include "sdk_video_decoder.h"

#include "tc_common_new/data.h"

namespace tc
{

    VideoDecoder::VideoDecoder() {

    }

    VideoDecoder::~VideoDecoder() {

    }

    int VideoDecoder::Decode(const std::shared_ptr<Data>& frame, DecodedCallback&& cbk) {
        return this->Decode((uint8_t*)frame->CStr(), frame->Size(), std::move(cbk));
    }

    int VideoDecoder::Decode(const std::string& frame, DecodedCallback&& cbk) {
        return this->Decode((uint8_t*)frame.data(), frame.size(), std::move(cbk));
    }

    void VideoDecoder::Release() {

    }

    bool VideoDecoder::NeedReConstruct(int codec_type, int width, int height, int img_format) {
        return codec_type != this->codec_type_ || width != this->frame_width_ || height != this->frame_height_ || img_format != this->img_format_;
    }


}
