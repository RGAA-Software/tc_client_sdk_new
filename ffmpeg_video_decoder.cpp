//
// Created by RGAA on 2023/8/11.
//

#include "ffmpeg_video_decoder.h"

#include "raw_image.h"
#include "tc_common/data.h"
#include "tc_message.pb.h"
#include "tc_common/log.h"

#include <libyuv.h>
#include <iostream>
#include <thread>

namespace tc {

    std::shared_ptr<FFmpegVideoDecoder> FFmpegVideoDecoder::Make(bool to_rgb) {
        return std::make_shared<FFmpegVideoDecoder>(to_rgb);
    }

    FFmpegVideoDecoder::FFmpegVideoDecoder(bool to_rgb) {
        this->cvt_to_rgb = to_rgb;
    }

    FFmpegVideoDecoder::~FFmpegVideoDecoder() {

    }

    int FFmpegVideoDecoder::Init(int codec_type, int width, int height) {
        if (inited_) {
            return 0;
        }

        av_log_set_level(AV_LOG_INFO);
        av_log_set_callback([](void* ptr, int level, const char* fmt, va_list vl) {
        #if defined(__ANDROID__)
            #if defined(ENABLE_FFMPEG_LOG)
            if (level == AV_LOG_ERROR) {
                __android_log_vprint(ANDROID_LOG_ERROR, "FFmpeg", fmt, vl);
            }
            else if (level == AV_LOG_WARNING) {
                __android_log_vprint(ANDROID_LOG_WARN, "FFmpeg", fmt, vl);
            }
            else {
                __android_log_vprint(ANDROID_LOG_INFO, "FFmpeg", fmt, vl);
            }
            #endif
        #else
            vprintf(fmt, vl);
        #endif
        });

        //ListCodecs();

        auto format_num = [](int val) -> int {
            auto t = val % 2;
            return val + t;
        };

        this->codec_type_ = codec_type;
        this->frame_width_ = format_num(width);
        this->frame_height_ = format_num(height);
        AVCodecID codec_id = AV_CODEC_ID_NONE;
        if (codec_type == VideoType::kNetH264) {
            codec_id = AV_CODEC_ID_H264;
        }
        else if (codec_type == VideoType::kNetHevc) {
            codec_id = AV_CODEC_ID_H265;
        }
        /*else if (codec_type == VideoType::kVp9) {
            codec_id = AV_CODEC_ID_VP9;
        }*/
        if (codec_id == AV_CODEC_ID_NONE) {
            std::cout << "CodecId not find : " << codec_type << std::endl;
            return -1;
        }

        codec = const_cast<AVCodec*>(avcodec_find_decoder(codec_id));
//        codec = const_cast<AVCodec*>(avcodec_find_decoder_by_name("hevc"));
        if (!codec) {
            LOGE("can not find codec");
            return -1;
        }

        codec_context = avcodec_alloc_context3(codec);
        if (codec_context == NULL) {
            LOGE("Could not alloc video context!");
            return -1;
        }

        AVCodecParameters* codec_params = avcodec_parameters_alloc();
        if (avcodec_parameters_from_context(codec_params, codec_context) < 0) {
            LOGE("Failed to copy av codec parameters from codec context.");
            avcodec_parameters_free(&codec_params);
            avcodec_free_context(&codec_context);
            return -1;
        }

        if (!codec_params) {
            LOGE("Source codec context is NULL.");
            return -1;
        }
        codec_context->thread_count = (int)std::thread::hardware_concurrency();
        codec_context->thread_type = FF_THREAD_SLICE;
        if (avcodec_open2(codec_context, codec, NULL) < 0) {
            LOGE("Failed to open decoder");
            Release();
            return -1;
        }

        LOGI("Decoder thread count: {}", codec_context->thread_count);

        //av_init_packet(&packet);
        packet = av_packet_alloc();
        av_frame = av_frame_alloc();

        avcodec_parameters_free(&codec_params);

        inited_ = true;

        return 0;
    }

    void FFmpegVideoDecoder::ListCodecs() {
        const AVCodec *codec = NULL;
        void *opaque = NULL;

        // 使用 av_codec_iterate 遍历所有编解码器
        LOGI("Available codecs:");
        while ((codec = av_codec_iterate(&opaque)) != NULL) {
            if (codec->type == AVMEDIA_TYPE_VIDEO || codec->type == AVMEDIA_TYPE_AUDIO) {
                if (av_codec_is_encoder(codec)) {
                    //LOGI("Encoder: {}", codec->name);
                }
                if (av_codec_is_decoder(codec)) {
                    LOGI("Decoder: {}", codec->name);
                }
            }
        }
    }

    static void I420ToRGB24(unsigned char* yuvData, unsigned char* rgb24, int width, int height) {

        unsigned char* ybase = yuvData;
        unsigned char* ubase = &yuvData[width * height];
        unsigned char* vbase = &yuvData[width * height * 5 / 4];
        libyuv::I420ToRGB24(ybase, width, ubase, width / 2, vbase, width / 2,
                            rgb24,
                            width * 3, width, height);

    }

    int FFmpegVideoDecoder::Decode(const std::shared_ptr<Data>& frame, DecodedCallback&& cbk) {
        return this->Decode((uint8_t*)frame->CStr(), frame->Size(), std::move(cbk));
    }

    int FFmpegVideoDecoder::Decode(const std::string& frame, DecodedCallback&& cbk) {
        return this->Decode((uint8_t*)frame.data(), frame.size(), std::move(cbk));
    }

    int FFmpegVideoDecoder::Decode(const uint8_t* data, int size, DecodedCallback&& cbk) {
        if (!codec_context || !av_frame || stop_) {
            return -1;
        }

        std::lock_guard<std::mutex> guard(decode_mtx_);

        av_frame_unref(av_frame);

        packet->data = (uint8_t*)data;//frame->CStr();
        packet->size = size;//frame->Size();

        auto format = codec_context->pix_fmt;

        int ret = avcodec_send_packet(codec_context, packet);
        if (ret < 0) {
            LOGE("avcodec_send_packet err: {}", ret);
            return ret;
        }

        while (ret == 0) {
            ret = avcodec_receive_frame(codec_context, av_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                //LOGE("avcodec_receive_frame again...");
                break;
            } else if (ret < 0) {
                LOGI("avcodec_receive_frame error: {}", ret);
                break;
            }

            auto width = av_frame->width;
            auto height = av_frame->height;

            auto x1 = av_frame->linesize[0];
            auto x2 = av_frame->linesize[1];
            auto x3 = av_frame->linesize[2];
            width = x1;

            if (format == AVPixelFormat::AV_PIX_FMT_YUV420P || format == AVPixelFormat::AV_PIX_FMT_NV12) {
                frame_width_ = std::min(frame_width_, width);
                frame_height_ = std::min(frame_height_, height);
                if (!decoded_image_ || frame_width_ != decoded_image_->img_width ||
                    frame_height_ != decoded_image_->img_height) {
                    decoded_image_ = RawImage::MakeI420(nullptr, frame_width_ * frame_height_ * 1.5,
                                                        frame_width_, frame_height_);
                }
                char *buffer = decoded_image_->Data();
                for (int i = 0; i < frame_height_; i++) {
                    memcpy(buffer + frame_width_ * i, av_frame->data[0] + width * i, frame_width_);
                }

                int y_offset = frame_width_ * frame_height_;
                for (int j = 0; j < frame_height_ / 2; j++) {
                    memcpy(buffer + y_offset + (frame_width_ / 2 * j),
                           av_frame->data[1] + width / 2 * j, frame_width_ / 2);
                }

                int yu_offset = y_offset + (frame_width_ / 2) * (frame_height_ / 2);
                for (int k = 0; k < frame_height_ / 2; k++) {
                    memcpy(buffer + yu_offset + (frame_width_ / 2 * k),
                           av_frame->data[2] + width / 2 * k, frame_width_ / 2);
                }
            }

            if (decoded_image_ && !stop_) {
                if (cvt_to_rgb) {
                    auto rgb = RawImage::MakeRGB(nullptr, frame_width_ * frame_height_ * 3,
                                                 frame_width_, frame_height_);
                    auto rgb_data = rgb->Data();
                    auto yuv_data = decoded_image_->Data();
                    I420ToRGB24((uint8_t *) yuv_data, (uint8_t *) rgb_data, frame_width_,
                                frame_height_);
                    cbk(rgb);
                    return 0;
                }
                else {
                    cbk(decoded_image_);
                }
            }

            //
            av_frame_unref(av_frame);
        }
        av_packet_unref(packet);
        return 0;
    }

    void FFmpegVideoDecoder::Release() {
        std::lock_guard<std::mutex> guard(decode_mtx_);
        stop_ = true;
        if (codec_context != nullptr) {
            avcodec_free_context(&codec_context);
            codec_context = nullptr;
        }

        if (av_frame != nullptr) {
            av_packet_unref(packet);
            av_free(av_frame);
            av_frame = nullptr;
        }

        av_packet_free(&packet);
    }

    bool FFmpegVideoDecoder::NeedReConstruct(int codec_type, int width, int height) {
        return codec_type != this->codec_type_ || width != this->frame_width_ || height != this->frame_height_;
    }

}