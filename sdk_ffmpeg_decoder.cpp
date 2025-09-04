//
// Created by RGAA on 2023/8/11.
//

#include "sdk_ffmpeg_decoder.h"
#include <iostream>
#include <thread>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <atlbase.h>
#include <fstream>
#include "thunder_sdk.h"
#include "sdk_statistics.h"
#include "tc_message.pb.h"
#include "tc_common_new/log.h"
#include "tc_common_new/data.h"
#include "tc_common_new/file.h"
#include "tc_common_new/time_util.h"
#include "tc_common_new/string_util.h"
#include "tc_client_sdk_new/gl/raw_image.h"
#include "tc_common_new/win32/d3d11_wrapper.h"
#include "tc_common_new/win32/d3d_debug_helper.h"
#if 000
#include <libyuv.h>
#endif

namespace tc
{

    FFmpegDecoder::FFmpegDecoder(const std::shared_ptr<ThunderSdk>& sdk) : VideoDecoder(sdk) {

    }

    FFmpegDecoder::~FFmpegDecoder() {

    }

    static std::string PrintAdapterInfo(ComPtr<ID3D11Device> device) {
        if (!device) return "";

        ComPtr<IDXGIDevice> dxgiDevice;
        HRESULT hr = device.As(&dxgiDevice);
        if (FAILED(hr)) {
            std::cerr << "Failed to get IDXGIDevice" << std::endl;
            return "";
        }

        ComPtr<IDXGIAdapter> adapter;
        hr = dxgiDevice->GetAdapter(&adapter);
        if (FAILED(hr)) {
            std::cerr << "Failed to get IDXGIAdapter" << std::endl;
            return "";
        }

        DXGI_ADAPTER_DESC desc;
        hr = adapter->GetDesc(&desc);
        if (FAILED(hr)) {
            std::cerr << "Failed to get adapter desc" << std::endl;
            return "";
        }

        std::stringstream ss;
        //ss << "Adapter: " <<  desc.Description << std::endl;
        ss << "VendorId: " << desc.VendorId << ", DeviceId: " << desc.DeviceId << std::endl;
        ss << "SubSysId: " << desc.SubSysId << ", Revision: " << desc.Revision << std::endl;

        // 你可以用 VendorId+DeviceId/SubSysId/Revision 组合成唯一的 Adapter UID
        // 或者直接使用 Adapter LUID（Windows 8+）
        ss << "LUID: "
                   << desc.AdapterLuid.HighPart << ":"
                   << desc.AdapterLuid.LowPart << std::endl;
        return ss.str();
    }

    bool supports_yuv444(const AVCodec *codec) {
        //const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        //if (!codec) {
        //    return false;
        //}

        const enum AVPixelFormat* p = codec->pix_fmts;
        if (!p) {
            return false; // 解码器未指定支持的格式
        }

        while (*p != AV_PIX_FMT_NONE) {
            // 检查常见的 YUV444 格式
            switch (*p) {
                case AV_PIX_FMT_YUV444P:
                case AV_PIX_FMT_VULKAN:
                    return true;
                default:
                    break;
            }
            p++;
        }

        return false;
    }

    bool supports_yuv420(const AVCodec *codec) {
        //const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        //if (!codec) {
        //    return false;
        //}

        const enum AVPixelFormat* p = codec->pix_fmts;
        if (!p) {
            LOGI("*****************************Dont have format");
            return false; // 解码器未指定支持的格式
        }

        while (*p != AV_PIX_FMT_NONE) {
            LOGI("Support format: {}", *p);
            switch (*p) {
                case AV_PIX_FMT_YUV420P:
                case AV_PIX_FMT_D3D11:
                    return true;
                default:
                    break;
            }
            p++;
        }

        return false;
    }

    // img_format:
    // kI420 = 0,
    // kI444 = 1,
    int FFmpegDecoder::Init(const std::string& mon_name, int codec_type, int width, int height, const std::string& frame, void* surface, int img_format) {
        if (inited_) {
            return 0;
        }
        monitor_name_ = mon_name;
        img_format_ = img_format;
        av_log_set_callback([](void* ptr, int level, const char* fmt, va_list vl) {
            char buffer[4096] = {0};
            vsnprintf(buffer, sizeof(buffer), fmt, vl);
            const char* level_str;
            switch (level) {
                case AV_LOG_PANIC: level_str = "PANIC"; break;
                case AV_LOG_FATAL: level_str = "FATAL"; break;
                case AV_LOG_ERROR: level_str = "ERROR"; break;
                case AV_LOG_WARNING: level_str = "WARN"; break;
                case AV_LOG_INFO: level_str = "INFO"; break;
                case AV_LOG_VERBOSE: level_str = "VERBOSE"; break;
                case AV_LOG_DEBUG: level_str = "DEBUG"; break;
                default: level_str = "UNKNOWN"; break;
            }
            //LOGI("FFMPEG [{}]: {}", level_str, buffer);
        });
        av_log_set_level(AV_LOG_WARNING);

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

                LOGI(" ==> HW device type: {}, pix format: {}", (int)config->device_type, (int)config->pix_fmt);
                if (config->device_type == AV_HWDEVICE_TYPE_D3D11VA) {
                    LOGI("Found the D3D11VA, Codec name: {}", codec->name);
                    if ((img_format == 0 && config->pix_fmt == AV_PIX_FMT_D3D11) || (img_format == 1 && supports_yuv444(codec))) {
                        found_target_codec = true;
                        decoder_ = const_cast<AVCodec*>(codec);
                        m_HwDecodeCfg = const_cast<AVCodecHWConfig*>(config);
                        LOGW("D3D11VA support image format: {}", (img_format == 0 ? "YUV420" : "YUV444"));
                        break;
                    }
                    else {
                        LOGW("D3D11VA doesn't support image format: {}", (img_format == 0 ? "YUV420" : "YUV444"));
                    }
                }
                else if (config->device_type == AV_HWDEVICE_TYPE_VULKAN) {
                    LOGI("Found the VULKAN");
                    if ((img_format == 0 && supports_yuv420(codec)) || (img_format == 1 && supports_yuv444(codec))) {
                        found_target_codec = true;
                        decoder_ = const_cast<AVCodec*>(codec);
                        m_HwDecodeCfg = const_cast<AVCodecHWConfig*>(config);
                        LOGW("VULKAN support image format: {}", (img_format == 0 ? "YUV420" : "YUV444"));
                    }
                    else {
                        LOGW("VULKAN doesn't support image format: {}", (img_format == 0 ? "YUV420" : "YUV444"));
                    }
                }
            }
        }

        if (codec && found_target_codec) {
            LOGI("Found codec, name: {}, id: {}, long name: {}", codec->name, (int)codec->id, codec->long_name);
        }
        else {
            //
            LOGI("Can't find hardware codec for format: {}, will use software docoder.", img_format);
        }

        this->codec_type_ = codec_type;
        this->frame_width_ = width;
        this->frame_height_ = height;
        AVCodecID codec_id = AV_CODEC_ID_NONE;
        if (codec_type == VideoType::kNetH264) {
            sdk_stat_->video_format_.Update("H264");
            sdk_stat_->video_decoder_.Update("D3D11VA");
        }
        else if (codec_type == VideoType::kNetHevc) {
            sdk_stat_->video_format_.Update("HEVC");
            sdk_stat_->video_decoder_.Update("D3D11VA");
        }

        m_VideoDecoderCtx = avcodec_alloc_context3(decoder_);
        if (!m_VideoDecoderCtx) {
            LOGE("Unable to allocate video decoder context");
            return -1;
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
        if (!IsHardwareAccelerated()) {
            m_VideoDecoderCtx->thread_type = FF_THREAD_SLICE;
            m_VideoDecoderCtx->thread_count = std::min(8, (int)std::thread::hardware_concurrency());
        }
        else {
            // No threading for HW decode
            m_VideoDecoderCtx->thread_count = 1;
        }

        auto fnGetPreferredPixelFormat = [] (int format) -> AVPixelFormat {
//            if (videoFormat & VIDEO_FORMAT_MASK_10BIT) {
//                return (videoFormat & VIDEO_FORMAT_MASK_YUV444) ?
//                       AV_PIX_FMT_YUV444P10 : // 10-bit 3-plane YUV 4:4:4
//                       AV_PIX_FMT_P010;       // 10-bit 2-plane YUV 4:2:0
//            }
//            else {
//                return (videoFormat & VIDEO_FORMAT_MASK_YUV444) ?
//                       AV_PIX_FMT_YUV444P : // 8-bit 3-plane YUV 4:4:4
//                       AV_PIX_FMT_YUV420P;  // 8-bit 3-plane YUV 4:2:0
//            }
            return format == 0 ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_YUV444P;
        };

        // Setup decoding parameters
        m_VideoDecoderCtx->width = width;
        m_VideoDecoderCtx->height = height;
        m_VideoDecoderCtx->pix_fmt = fnGetPreferredPixelFormat(img_format);
        m_VideoDecoderCtx->get_format = ffGetFormat;

        AVDictionary* options = nullptr;
        m_VideoDecoderCtx->opaque = this;
        int err = avcodec_open2(m_VideoDecoderCtx, decoder_, &options);
        av_dict_free(&options);
        if (err < 0) {
            LOGE("Unable to open decoder for format: {}", img_format);
            return err;
        }

        packet = av_packet_alloc();
        av_frame = av_frame_alloc();

        ////

        m_HwDeviceContext = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        if (!m_HwDeviceContext) {
            LOGE("Failed to create D3D11VA device context");
            return -1;
        }

        // 取出 AVHWDeviceContext -> ID3D11Device 并替换成你自己的
        AVHWDeviceContext* hwctx = (AVHWDeviceContext*)m_HwDeviceContext->data;
        AVD3D11VADeviceContext* d3d11ctx = (AVD3D11VADeviceContext*)hwctx->hwctx;

        auto d3d11_wrapper = sdk_->GetSdkParams()->d3d11_wrapper_;
        if (!d3d11_wrapper) {
            LOGE("Don't have d3d11 wrapper, failed to init d3d11va decoder.");
            return -1;
        }
        LOGI("d3d11device adapter uid: {}", d3d11_wrapper->adapter_uid_);

        auto info = PrintAdapterInfo(d3d11ctx->device);
        LOGI("ORIGIN D3D INFO: {}", info);

        d3d11ctx->device = d3d11_wrapper->d3d11_device_.Get();
        d3d11ctx->device_context = d3d11_wrapper->d3d11_device_context_.Get();

        m_VideoDecoderCtx->hw_device_ctx = av_buffer_ref(m_HwDeviceContext);
        err = av_hwdevice_ctx_init(m_HwDeviceContext);
        if (err < 0) {
            LOGE("Failed to initialize D3D11VA device context: {}", err);
            return err;
        }

        ////

        m_HwFramesContext = av_hwframe_ctx_alloc(m_HwDeviceContext);
        if (!m_HwFramesContext) {
            LOGE("Failed to allocate D3D11VA frame context");
            return -1;
        }

        AVHWFramesContext* framesContext = (AVHWFramesContext*)m_HwFramesContext->data;

        framesContext->format = AV_PIX_FMT_D3D11;
//        if (params->videoFormat & VIDEO_FORMAT_MASK_10BIT) {
//            framesContext->sw_format = (params->videoFormat & VIDEO_FORMAT_MASK_YUV444) ?
//                                       AV_PIX_FMT_XV30 : AV_PIX_FMT_P010;
//        }
//        else {
//            framesContext->sw_format = (params->videoFormat & VIDEO_FORMAT_MASK_YUV444) ?
//                                       AV_PIX_FMT_VUYX : AV_PIX_FMT_NV12;
//        }

        framesContext->sw_format = AV_PIX_FMT_NV12;

        // Surfaces must be 16 pixel aligned for H.264 and 128 pixel aligned for everything else
        // https://github.com/FFmpeg/FFmpeg/blob/a234e5cd80224c95a205c1f3e297d8c04a1374c3/libavcodec/dxva2.c#L609-L616
        auto m_TextureAlignment = /*(params->videoFormat & VIDEO_FORMAT_MASK_H264)*/true ? 16 : 128;

        framesContext->width = FFALIGN(width, m_TextureAlignment);
        framesContext->height = FFALIGN(height, m_TextureAlignment);

        // We can have up to 16 reference frames plus a working surface
        framesContext->initial_pool_size = 8;//17;//DECODER_BUFFER_POOL_SIZE;

        AVD3D11VAFramesContext* d3d11vaFramesContext = (AVD3D11VAFramesContext*)framesContext->hwctx;

        d3d11vaFramesContext->BindFlags = D3D11_BIND_DECODER;
//        if (m_BindDecoderOutputTextures) {
//            // We need to override the default D3D11VA bind flags to bind the textures as a shader resources
//            d3d11vaFramesContext->BindFlags |= D3D11_BIND_SHADER_RESOURCE;
//        }

        err = av_hwframe_ctx_init(m_HwFramesContext);
        if (err < 0) {
           LOGE("Failed to initialize D3D11VA frame context: {}", err);
            return err;
        }

        D3D11_TEXTURE2D_DESC textureDesc;
        d3d11vaFramesContext->texture_infos->texture->GetDesc(&textureDesc);
        inited_ = true;

        return 0;
    }

    enum AVPixelFormat FFmpegDecoder::ffGetFormat(AVCodecContext* context, const enum AVPixelFormat* pixFmts) {
        FFmpegDecoder* decoder = (FFmpegDecoder*)context->opaque;
        const AVPixelFormat *p;
        AVPixelFormat desiredFmt;

        if (decoder->m_HwDecodeCfg) {
            desiredFmt = decoder->m_HwDecodeCfg->pix_fmt;
        }
//        else if (decoder->m_RequiredPixelFormat != AV_PIX_FMT_NONE) {
//            desiredFmt = decoder->m_RequiredPixelFormat;
//        }
//        else {
//            desiredFmt = decoder->m_FrontendRenderer->getPreferredPixelFormat(decoder->m_VideoFormat);
//        }
//
        for (p = pixFmts; *p != AV_PIX_FMT_NONE; p++) {
            // Only match our hardware decoding codec or preferred SW pixel
            // format (if not using hardware decoding). It's crucial
            // to override the default get_format() which will try
            // to gracefully fall back to software decode and break us.
            if (*p == desiredFmt/* && decoder->m_BackendRenderer->prepareDecoderContextInGetFormat(context, *p)*/) {
                context->hw_frames_ctx = av_buffer_ref(decoder->m_HwFramesContext);
                return *p;
            }
        }
//
//        // Failed to match the preferred pixel formats. Try non-preferred pixel format options
//        // for non-hwaccel decoders if we didn't have a required pixel format to use.
//        if (decoder->m_HwDecodeCfg == nullptr && decoder->m_RequiredPixelFormat == AV_PIX_FMT_NONE) {
//            for (p = pixFmts; *p != AV_PIX_FMT_NONE; p++) {
//                if (decoder->m_FrontendRenderer->isPixelFormatSupported(decoder->m_VideoFormat, *p) &&
//                    decoder->m_BackendRenderer->prepareDecoderContextInGetFormat(context, *p)) {
//                    return *p;
//                }
//            }
//        }
//
        return AV_PIX_FMT_NONE;
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

    Result<std::shared_ptr<RawImage>, int> FFmpegDecoder::Decode(const uint8_t* data, int size) {
        if (!m_VideoDecoderCtx || !av_frame || stop_) {
            return TRError(-1);
        }
        std::lock_guard<std::mutex> guard(decode_mtx_);

        auto beg = TimeUtil::GetCurrentTimestamp();
        av_frame_unref(av_frame);
        av_packet_unref(packet);

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
        std::shared_ptr<RawImage> decoded_image = nullptr;
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

            if (av_frame->key_frame) {
                LOGI("key frame!!!!!!!");
            }

            // ONLY D3D11 NOW
            if (av_frame->format == AV_PIX_FMT_D3D11) {
                auto resource = (ID3D11Resource*)av_frame->data[0];
                if (!resource) {
                    LOGE("Null texture in AVFrame!");
                    break;
                }
                auto src_subresource = (int)(intptr_t)av_frame->data[1];
                //LOGI("resources: {:p}, index: {}", (void*)resource, src_subresource);

                CComPtr<ID3D11Texture2D> acquired_texture = nullptr;
                HRESULT hr = resource->QueryInterface(IID_PPV_ARGS(&acquired_texture));
                if (FAILED(hr)) {
                    LOGE("Not a d3d11 texture");
                    break;
                }

                D3D11_RESOURCE_DIMENSION type = D3D11_RESOURCE_DIMENSION_UNKNOWN;
                acquired_texture->GetType(&type);
                if (D3D11_RESOURCE_DIMENSION_TEXTURE2D != type) {
                    LOGE("Not a d3d11 texture");
                    break;
                }

                D3D11_TEXTURE2D_DESC desc;
                acquired_texture->GetDesc(&desc);
                //LOGI("resources size: {}x{}, dxgi format: {}, pix format: {}, D3D11: {}, Usage: {}", desc.Width, desc.Height, (int)desc.Format, (int)pix_format, AV_PIX_FMT_D3D11, desc.Usage);

                decoded_image = RawImage::MakeD3D11Texture(acquired_texture, src_subresource);
                decoded_image->img_width = desc.Width;
                decoded_image->img_height = desc.Height;
            }
            else if (av_frame->format == AV_PIX_FMT_YUV420P) {

            }
            else if (av_frame->format == AV_PIX_FMT_YUV444P) {

            }
            else {
                LOGI("Don't AVFrame format: {}", av_frame->format);
            }

            auto end = TimeUtil::GetCurrentTimestamp();
            sdk_->PostMiscTask([=, this]() {
                sdk_stat_->AppendDecodeDuration(monitor_name_, end-beg);
            });

            break;

            //
            //av_frame_unref(av_frame);
        }
        //av_packet_unref(packet);

        if (decoded_image) {
            return decoded_image;
        }

        return TRError(last_result);
    }

    void FFmpegDecoder::Release() {
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

    bool FFmpegDecoder::Ready() {
        return inited_;
    }

    int FFmpegDecoder::GetAVCodecCapabilities(const AVCodec *codec) {
        int caps = codec->capabilities;
        // There are a bunch of out-of-tree OMX decoder implementations
        // from various SBC manufacturers that all seem to forget to set
        // AV_CODEC_CAP_HARDWARE (probably because the upstream OMX code
        // also doesn't set it). Avoid a false decoder warning on startup
        // by setting it ourselves.
        auto codec_name = std::string(codec->name);
        StringUtil::ToLower(codec_name);
        if (codec_name.rfind("_omx") != std::string::npos) {
            caps |= AV_CODEC_CAP_HARDWARE;
        }
        //if (QString::fromUtf8(codec->name).endsWith("_omx", Qt::CaseInsensitive)) {
        //    caps |= AV_CODEC_CAP_HARDWARE;
        //}
        return caps;
    }


    bool FFmpegDecoder::IsHardwareAccelerated() {
        return m_HwDecodeCfg != nullptr ||
               (GetAVCodecCapabilities(m_VideoDecoderCtx->codec) & AV_CODEC_CAP_HARDWARE) != 0;
    }

}