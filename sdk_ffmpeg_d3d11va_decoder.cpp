//
// Created by RGAA on 2023/8/11.
//

#include "sdk_ffmpeg_d3d11va_decoder.h"

#include "tc_common_new/data.h"
#include "tc_message.pb.h"
#include "tc_common_new/log.h"
#include "tc_client_sdk_new/gl/raw_image.h"
#include "tc_common_new/time_util.h"
#include "tc_common_new/file.h"
#include <iostream>
#include <thread>
#include "thunder_sdk.h"
#include "sdk_statistics.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <atlbase.h>
#include <iostream>
#if 000
#include <libyuv.h>
#endif


static AVBufferRef *hw_device_ctx = NULL;
static enum AVPixelFormat hw_pix_fmt;
static FILE *output_file = NULL;

static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type) {
    int err = 0;
    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL, 0)) < 0) {
        LOGE("Failed to create specified HW device: {}", err);
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    return err;
}


static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

static int decode_write(AVCodecContext *avctx, AVPacket *packet)
{
    AVFrame *frame = NULL, *sw_frame = NULL;
    AVFrame *tmp_frame = NULL;
    uint8_t *buffer = NULL;
    int size;
    int ret = 0;

    ret = avcodec_send_packet(avctx, packet);
    if (ret < 0) {
        fprintf(stderr, "Error during decoding\n");
        return ret;
    }

    while (1) {
        if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
            fprintf(stderr, "Can not alloc frame\n");
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        ret = avcodec_receive_frame(avctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            av_frame_free(&sw_frame);
            return 0;
        } else if (ret < 0) {
            fprintf(stderr, "Error while decoding\n");
            goto fail;
        }

        // 在通用解码流程之后 转换 解码格式，转换成常用格式，FFmpeg 解码H264一般都是解码成 YUV420，AVFrame 的视频格式转换可以使用 libyuv 或者 SwsContext。
        if (frame->format == hw_pix_fmt) {
            /* retrieve data from GPU to CPU */
            if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                fprintf(stderr, "Error transferring the data to system memory\n");
                goto fail;
            }
            tmp_frame = sw_frame;
        } else
            tmp_frame = frame;

        size = av_image_get_buffer_size(static_cast<AVPixelFormat>(tmp_frame->format), tmp_frame->width, tmp_frame->height, 1);
        buffer = static_cast<uint8_t *>(av_malloc(size));
        if (!buffer) {
            fprintf(stderr, "Can not alloc buffer\n");
            ret = AVERROR(ENOMEM);
            goto fail;
        }
        ret = av_image_copy_to_buffer(buffer, size,
                                      (const uint8_t * const *)tmp_frame->data,
                                      (const int *)tmp_frame->linesize, static_cast<AVPixelFormat>(tmp_frame->format),
                                      tmp_frame->width, tmp_frame->height, 1);
        if (ret < 0) {
            fprintf(stderr, "Can not copy image to buffer\n");
            goto fail;
        }

        if ((ret = fwrite(buffer, 1, size, output_file)) < 0) {
            fprintf(stderr, "Failed to dump raw data.\n");
            goto fail;
        }

        fail:
        av_frame_free(&frame);
        av_frame_free(&sw_frame);
        av_freep(&buffer);
        if (ret < 0)
            return ret;
    }
}

namespace tc
{

    FFmpegD3D11VADecoder::FFmpegD3D11VADecoder(const std::shared_ptr<ThunderSdk>& sdk) : VideoDecoder(sdk) {

    }

    FFmpegD3D11VADecoder::~FFmpegD3D11VADecoder() {

    }

    int FFmpegD3D11VADecoder::Init(const std::string& mon_name, int codec_type, int width, int height, const std::string& frame, void* surface, int img_format) {
        if (inited_) {
            return 0;
        }
        monitor_name_ = mon_name;
        img_format_ = img_format;
        av_log_set_level(AV_LOG_INFO);
        av_log_set_callback([](void* ptr, int level, const char* fmt, va_list vl) {
            char buffer[1024] = {0};
            sprintf(buffer, fmt, vl);
            LOGI("FFMPEG: {}", buffer);
        });

        bool found_target_codec = false;
        const AVCodec *codec = NULL;
        void *opaque = NULL;

        LOGI("Available codecs:");
        while ((codec = av_codec_iterate(&opaque)) != NULL) {
            if (codec->type != AVMEDIA_TYPE_VIDEO) {
                continue;
            }
            if (!av_codec_is_decoder(codec)) {
                continue;
            }

            if (GetAVCodecCapabilities(codec) & AV_CODEC_CAP_HARDWARE) {
                continue;
            }

            if (codec_type == VideoType::kNetH264) {
                if (codec->id != AV_CODEC_ID_H264) {
                    continue;
                }
            }
            else if (codec_type == VideoType::kNetHevc) {
                if (codec->id != AV_CODEC_ID_HEVC) {
                    continue;
                }
            }
            else {
                // AV_CODEC_ID_AV1
            }

            for (int i = 0;; i++) {
                const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
                if (!config) {
                    break;
                }

                LOGI(" ==> HW device type: {}", (int)config->device_type);
                if (config->device_type == AV_HWDEVICE_TYPE_D3D11VA) {
                    LOGI("Found the D3D11VA");
                    found_target_codec = true;
                    decoder_ = const_cast<AVCodec*>(codec);
                    break;
                }
            }
        }

        if (codec && found_target_codec) {
            LOGI("Found codec, name: {}, id: {}, long name: {}", codec->name, (int)codec->id, codec->long_name);
        }

        auto format_num = [](int val) -> int {
            auto t = val % 2;
            return val + t;
        };

        this->codec_type_ = codec_type;
        this->frame_width_ = width;//format_num(width);
        this->frame_height_ = height;//format_num(height);
        AVCodecID codec_id = AV_CODEC_ID_NONE;
        if (codec_type == VideoType::kNetH264) {
            sdk_stat_->video_format_.Update("H264");
            sdk_stat_->video_decoder_.Update("X264");
        }
        else if (codec_type == VideoType::kNetHevc) {
            sdk_stat_->video_format_.Update("HEVC");
            sdk_stat_->video_decoder_.Update("X265");
        }
        /*else if (codec_type == VideoType::kVp9) {
            codec_id = AV_CODEC_ID_VP9;
        }*/

        inited_ = true;

        int i;

//        m_RequiredPixelFormat = requiredFormat;
//        m_StreamFps = params->frameRate;
//        m_VideoFormat = params->videoFormat;

        // Don't bother initializing Pacer if we're not actually going to render
//        if (!testFrame) {
//            m_Pacer = new Pacer(m_FrontendRenderer, &m_ActiveWndVideoStats);
//            if (!m_Pacer->initialize(params->window, params->frameRate,
//                                     params->enableFramePacing || (params->enableVsync && (m_FrontendRenderer->getRendererAttributes() & RENDERER_ATTRIBUTE_FORCE_PACING)))) {
//                return false;
//            }
//        }

        m_VideoDecoderCtx = avcodec_alloc_context3(decoder_);
        if (!m_VideoDecoderCtx) {
//            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
//                         "Unable to allocate video decoder context");
            return false;
        }

        // Always request low delay decoding
        m_VideoDecoderCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

        // Allow display of corrupt frames and frames missing references
        m_VideoDecoderCtx->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
        m_VideoDecoderCtx->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;

        // Report decoding errors to allow us to request a key frame
        //
        // With HEVC streams, FFmpeg can drop a frame (hwaccel->start_frame() fails)
        // without telling us. Since we have an infinite GOP length, this causes artifacts
        // on screen that persist for a long time. It's easy to cause this condition
        // by using NVDEC and delaying 100 ms randomly in the render path so the decoder
        // runs out of output buffers.
        m_VideoDecoderCtx->err_recognition = AV_EF_EXPLODE;

        // Enable slice multi-threading for software decoding
//        if (!isHardwareAccelerated()) {
//            m_VideoDecoderCtx->thread_type = FF_THREAD_SLICE;
//            m_VideoDecoderCtx->thread_count = qMin(MAX_SLICES, SDL_GetCPUCount());
//        }
//        else {
//            // No threading for HW decode
//            m_VideoDecoderCtx->thread_count = 1;
//        }

        m_VideoDecoderCtx->thread_count = 1;

        // Setup decoding parameters
        m_VideoDecoderCtx->width = width;
        m_VideoDecoderCtx->height = height;
        m_VideoDecoderCtx->pix_fmt = AV_PIX_FMT_D3D11;//requiredFormat != AV_PIX_FMT_NONE ? requiredFormat : m_FrontendRenderer->getPreferredPixelFormat(params->videoFormat);
        //m_VideoDecoderCtx->get_format = ffGetFormat;

        AVDictionary* options = nullptr;

        // Allow the backend renderer to attach data to this decoder
//        if (!m_BackendRenderer->prepareDecoderContext(m_VideoDecoderCtx, &options)) {
//            return false;
//        }

        // Nobody must override our ffGetFormat
//        SDL_assert(m_VideoDecoderCtx->get_format == ffGetFormat);

        // Stash a pointer to this object in the context
//        SDL_assert(m_VideoDecoderCtx->opaque == nullptr);
        m_VideoDecoderCtx->opaque = this;

        int err = avcodec_open2(m_VideoDecoderCtx, decoder_, &options);
        av_dict_free(&options);
        if (err < 0) {
//            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
//                         "Unable to open decoder for format: %x",
//                         params->videoFormat);
            return false;
        }

        if (hw_decoder_init(m_VideoDecoderCtx, AV_HWDEVICE_TYPE_D3D11VA) != 0) {

        }

        packet = av_packet_alloc();
        av_frame = av_frame_alloc();

        return 0;
    }

    int FFmpegD3D11VADecoder::GetAVCodecCapabilities(const AVCodec *codec) {
        int caps = codec->capabilities;
        // There are a bunch of out-of-tree OMX decoder implementations
        // from various SBC manufacturers that all seem to forget to set
        // AV_CODEC_CAP_HARDWARE (probably because the upstream OMX code
        // also doesn't set it). Avoid a false decoder warning on startup
        // by setting it ourselves.
        if (QString::fromUtf8(codec->name).endsWith("_omx", Qt::CaseInsensitive)) {
            caps |= AV_CODEC_CAP_HARDWARE;
        }
        return caps;
    }

#if 000
    static void I420ToRGB24(unsigned char* yuv_data, unsigned char* rgb24, int width, int height) {
        unsigned char* y = yuv_data;
        unsigned char* u = &yuv_data[width * height];
        unsigned char* v = &yuv_data[width * height * 5 / 4];
        libyuv::I420ToRGB24(y, width, u, width / 2, v, width / 2,
                            rgb24,
                            width * 3, width, height);
    }
#endif

    Result<std::shared_ptr<RawImage>, int> FFmpegD3D11VADecoder::Decode(const uint8_t* data, int size) {
        if (!m_VideoDecoderCtx || !av_frame || stop_) {
            return TRError(-1);
        }
        std::lock_guard<std::mutex> guard(decode_mtx_);

        auto beg = TimeUtil::GetCurrentTimestamp();
        av_frame_unref(av_frame);

        packet->data = (uint8_t*)data;//frame->CStr();
        packet->size = size;//frame->Size();

        int ret = avcodec_send_packet(m_VideoDecoderCtx, packet);
        if (ret == AVERROR(EAGAIN)) {
            LOGW("EAGAIN...");
            return TRError(0);
        }
        else if (ret != 0) {
            LOGE("avcodec_send_packet err: {}", ret);
            return TRError(ret);
        }

        bool has_received_frame = false;
        auto last_result = 0;
        while (true) {
            ret = avcodec_receive_frame(m_VideoDecoderCtx, av_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                last_result = has_received_frame ? 0 : ret;
                break;
            } else if (ret != 0) {
                LOGE("avcodec_receive_frame error: {}", ret);
                last_result = ret;
                break;
            }
            has_received_frame = true;

            auto width = av_frame->width;
            auto height = av_frame->height;

            auto x1 = av_frame->linesize[0];
            auto x2 = av_frame->linesize[1];
            auto x3 = av_frame->linesize[2];
            //width = x1;
            //LOGI("frame width: {}, x1: {}", av_frame->width, x1);

            auto format = m_VideoDecoderCtx->pix_fmt;

            auto resource = (ID3D11Resource*)av_frame->data[0];
            auto d = (int)(intptr_t)av_frame->data[1];
            LOGI("resources: {:p}, index: {}", (void*)resource, d);

            D3D11_RESOURCE_DIMENSION type = D3D11_RESOURCE_DIMENSION_UNKNOWN;
            resource->GetType(&type);
            if (D3D11_RESOURCE_DIMENSION_TEXTURE2D != type) {
                return TRError(ERROR_NOT_SUPPORTED);
            }

            CComPtr<ID3D11Texture2D> acquired_texture;
            HRESULT hr = resource->QueryInterface(IID_PPV_ARGS(&acquired_texture));
            if (FAILED(hr)) {
                return TRError(hr);
            }

            D3D11_TEXTURE2D_DESC desc;
            acquired_texture->GetDesc(&desc);
            LOGI("resources size: {}x{}, format: {}", desc.Width, desc.Height, (int)desc.Format);

            if (decoded_image_ && !stop_) {
                auto end = TimeUtil::GetCurrentTimestamp();
                sdk_->PostMiscTask([=, this]() {
                    sdk_stat_->AppendDecodeDuration(monitor_name_, end-beg);
                });
                //LOGI("FFmpeg decode YUV420p(I420) used : {}ms, {}x{}", (end-beg), frame_width_, frame_height_);
                return decoded_image_;
            }

            //
            av_frame_unref(av_frame);
        }
        av_packet_unref(packet);
        return TRError(last_result);
    }

    void FFmpegD3D11VADecoder::Release() {
        std::lock_guard<std::mutex> guard(decode_mtx_);
        stop_ = true;

//        while (true) {
//            auto ret = avcodec_receive_frame(codec_context, av_frame);
//            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
//                break;
//            }
//        }
//
//        if (codec_context != nullptr) {
//            avcodec_close(codec_context);
//            avcodec_free_context(&codec_context);
//            codec_context = nullptr;
//        }
//
//av_packet_unref(packet);
        if (av_frame != nullptr) {
            av_free(av_frame);
            av_frame = nullptr;
        }

        if (packet != nullptr) {
            av_packet_free(&packet);
            packet = nullptr;
        }
        LOGI("FFmpeg video decoder release.");
    }

    bool FFmpegD3D11VADecoder::Ready() {
        return inited_;
    }
}