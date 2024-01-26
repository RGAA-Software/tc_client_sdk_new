//
// Created by hy on 2024/1/26.
//

#include "mediacodec_video_decoder.h"

#ifdef ANDROID

namespace tc
{
    std::shared_ptr<MediacodecVideoDecoder> MediacodecVideoDecoder::Make() {
        return std::make_shared<MediacodecVideoDecoder>();
    }

    MediacodecVideoDecoder::MediacodecVideoDecoder() {

    }

    MediacodecVideoDecoder::~MediacodecVideoDecoder() {

    }

    int MediacodecVideoDecoder::Init(int codec_type, int width, int height) {

    }

    int MediacodecVideoDecoder::Decode(const std::shared_ptr<Data>& frame, DecodedCallback&& cbk) {

    }

    int MediacodecVideoDecoder::Decode(const std::string& frame, DecodedCallback&& cbk) {

    }

    int MediacodecVideoDecoder::Decode(const uint8_t* data, int size, DecodedCallback&& cbk) {

    }

    void MediacodecVideoDecoder::Release() {

    }

}

#endif