//
// Created by RGAA on 2023/8/11.
//

#ifndef SAILFISH_CLIENT_PC_FFMPEG_D3D11VA_DECODER_H
#define SAILFISH_CLIENT_PC_FFMPEG_D3D11VA_DECODER_H

extern "C"
{
    #include <libavcodec/codec.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    #include <libavutil/log.h>
    #include <libavutil/pixdesc.h>
    #include <libavutil/hwcontext.h>
    #include <libavutil/avassert.h>
}
#include <libavutil/hwcontext_d3d11va.h>

#include <set>
#include "sdk_video_decoder.h"

namespace tc
{

    class FFmpegDecoder : public VideoDecoder {
    public:
        explicit FFmpegDecoder(const std::shared_ptr<ThunderSdk>& sdk);
        ~FFmpegDecoder() override;

        int Init(const std::string& mon_name, int codec_type, int width, int height, const std::string& frame, void* surface, int img_format) override;
        Result<std::shared_ptr<RawImage>, int> Decode(const uint8_t* data, int size) override;
        void Release() override;
        bool Ready() override;

        static enum AVPixelFormat ffGetFormat(AVCodecContext* context, const enum AVPixelFormat* pixFmts);

    private:
        int GetAVCodecCapabilities(const AVCodec *codec);
        bool IsHardwareAccelerated();

    private:
        AVStream *video = NULL;
        AVCodecContext *m_VideoDecoderCtx = NULL;
        AVCodec* decoder_ = NULL;
        AVPacket* packet = nullptr;
        AVFrame* av_frame = nullptr;
        //enum AVHWDeviceType type;

        AVBufferRef* m_HwDeviceContext;
        AVBufferRef* m_HwFramesContext;
        AVCodecHWConfig* m_HwDecodeCfg;

        std::set<const AVCodec*> terminallyFailedHardwareDecoders;

    };

}

#endif //SAILFISH_CLIENT_PC_FFMPEGVIDEODECODER_H
