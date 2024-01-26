//
// Created by hy on 2024/1/26.
//

#include "mediacodec_video_decoder.h"

#ifdef ANDROID

#include "tc_common/log.h"
#include "raw_image.h"

namespace tc
{

    static int64_t getTimeNsec() {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return (int64_t) now.tv_sec*1000*1000*1000 + now.tv_nsec;
    }
    static int64_t getTimeSec() {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return (int64_t)now.tv_sec;
    }
    static int64_t getTimeMsec(){ //毫秒
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return now.tv_sec*1000 +(int64_t)now.tv_nsec/(1000*1000);
    }
    static int64_t getTimeUsec(){ //us
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return now.tv_sec*1000*1000 +(int64_t)now.tv_nsec/(1000);
    }

    std::shared_ptr<MediacodecVideoDecoder> MediacodecVideoDecoder::Make() {
        return std::make_shared<MediacodecVideoDecoder>();
    }

    MediacodecVideoDecoder::MediacodecVideoDecoder() {

    }

    MediacodecVideoDecoder::~MediacodecVideoDecoder() {

    }

    int MediacodecVideoDecoder::Init(int codec_type, int width, int height) {
        media_codec_ = AMediaCodec_createDecoderByType("video/avc");//h264
        media_format_ = AMediaFormat_new();
        AMediaFormat_setString(media_format_, "mime", "video/avc");
        AMediaFormat_setInt32(media_format_, AMEDIAFORMAT_KEY_WIDTH, width);
        AMediaFormat_setInt32(media_format_, AMEDIAFORMAT_KEY_HEIGHT, height);

        media_status_t status = AMediaCodec_configure(media_codec_, media_format_,
                                                      NULL,/*可以在这制定native surface, 直接渲染*/
                                                      NULL,
                                                      0);//解码，flags 给0，编码给AMEDIACODEC_CONFIGURE_FLAG_ENCODE
        if (status != AMEDIA_OK) {
            LOGE("error config {}", (int) status);
            return -1;
        }

        //启动
        status = AMediaCodec_start(media_codec_);
        if (status != AMEDIA_OK) {
            LOGE("error start: {}", (int) status);
            return -1;
        }

        this->codec_type_ = codec_type;
        this->frame_width_ = width;
        this->frame_height_ = height;
        inited_ = true;

        return AMEDIA_OK;
    }

    int MediacodecVideoDecoder::Decode(const uint8_t *in_data, int in_size, DecodedCallback &&cbk) {
        ssize_t buf_idx = AMediaCodec_dequeueInputBuffer(media_codec_, 2000);
        if (buf_idx >= 0) {
            size_t buf_size = 0;
            uint8_t* buf = AMediaCodec_getInputBuffer(media_codec_, buf_idx, &buf_size);
            if (buf_size <= 0) {
                return -1;
            }
            memcpy(buf, in_data, in_size);
            uint64_t presentationTimeUs = getTimeUsec();
            AMediaCodec_queueInputBuffer(media_codec_, buf_idx, 0, in_size, presentationTimeUs, 0);
        }

        AMediaCodecBufferInfo info;
        do {
            buf_idx = AMediaCodec_dequeueOutputBuffer(media_codec_, &info, 2000);
            if (buf_idx >= 0) {
                size_t out_buf_size;
                uint8_t *buf = NULL;
                int real_frame_size = 0;
                int mWidth, mHeight;
                auto format = AMediaCodec_getOutputFormat(media_codec_);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &mWidth);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &mHeight);
                int32_t localColorFMT;
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &localColorFMT);
                real_frame_size = info.size;
                buf = AMediaCodec_getOutputBuffer(media_codec_, buf_idx, &out_buf_size);

                //LOGI("out:[{}]X[{}], format: {}, real_frame_size:{}, buf_size: {} ", mWidth, mHeight, localColorFMT, real_frame_size, out_buf_size); //21 == nv21
                if (buf && cbk && real_frame_size > 0) {
                    auto image = RawImage::Make((char*)buf, real_frame_size, mWidth, mHeight, -1, RawImageFormat::kNV12);
                    cbk(image);
                }
                AMediaCodec_releaseOutputBuffer(media_codec_, buf_idx, false);

            } else if (buf_idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                int mWidth, mHeight;
                auto format = AMediaCodec_getOutputFormat(media_codec_);
                AMediaFormat_getInt32(format, "width", &mWidth);
                AMediaFormat_getInt32(format, "height", &mHeight);
                int32_t localColorFMT;

                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                                      &localColorFMT);
            } else if (buf_idx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {

            } else {

            }
        } while (buf_idx > 0);

        return 0;
    }

    void MediacodecVideoDecoder::Release() {
        VideoDecoder::Release();
        if (media_codec_) {
            AMediaCodec_flush(media_codec_);
            AMediaCodec_stop(media_codec_);
            AMediaCodec_delete(media_codec_);
        }

        if (media_format_) {
            AMediaFormat_delete(media_format_);
        }
    }

}

#endif