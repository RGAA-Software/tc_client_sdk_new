#include "webrtc_client.h"

#ifdef WIN32
#include <Windows.h>
#endif

#include "tc_common/log.h"
#include <sstream>

#ifdef WIN32

namespace tc
{

	std::shared_ptr<WebRtcClient> WebRtcClient::Make() {
		return std::make_shared<WebRtcClient>();
	}

	WebRtcClient::WebRtcClient() {
		LoadWebRtcClient();
	}
	
	bool WebRtcClient::Start(const std::string& ip, int port) {
		if (dyn_rtc_client_init_) {
			rtc_client_param param{};
			param.frame_type_ = kApiRtcFrameYUY2;
			memset(param.remote_ip_, 0, sizeof(param.remote_ip_));
			memcpy(param.remote_ip_, ip.c_str(), ip.size());
			param.port_ = port;
			param.frame_callback_ = [](const unsigned char* buffer, int size, int frame_width, int frame_height, rtc_decoded_video_frame_type frame_type, void* priv_data) {
				auto parent = (WebRtcClient*)priv_data;
				if (!parent->video_frame_callback_) {
					return;
				}
				auto frame = parent->MakeVideoFrame(buffer, size, frame_width, frame_height, frame_type);
				parent->video_frame_callback_(std::move(frame));
			};
			dyn_rtc_client_init_(param, this);
		}
		return true;
	}

	void WebRtcClient::Exit() {
		if (dyn_rtc_client_exit_) {
			dyn_rtc_client_exit_();
		}
	}

	RtcVideoFramePtr WebRtcClient::MakeVideoFrame(const uint8_t* buffer, int size, int frame_width, int frame_height, rtc_decoded_video_frame_type type) {
		auto frame = std::make_shared<RtcVideoFrame>();
		frame->buffer_.resize(size);
		memcpy((char*)frame->buffer_.data(), buffer, size);
		frame->frame_width_ = frame_width;
		frame->frame_height_ = frame_height;
		frame->frame_type_ = type;
		frame->frame_idx_ = this->frame_idx_++;
		return frame;
	}

	void WebRtcClient::RegisterRtcVideoFrameCallback(OnRtcVideoFrameCallback&& cbk) {
		video_frame_callback_ = cbk;
	}

	bool WebRtcClient::LoadWebRtcClient() {
#ifdef WIN32
		auto module = LoadLibraryA("tc_rtc.dll");
		if (!module) {
			LOGE("load dlca_webrtc_clientmt.dll failed.");
			return false;
		}

		dyn_rtc_client_init_ = (rtc_client_init_ptr_t)GetProcAddress(module, "rtc_client_init");
		if (!dyn_rtc_client_init_) {
            LOGE("load rtc_client_init failed.");
			return false;
		}

		dyn_rtc_client_exit_ = (rtc_client_exit_ptr_t)GetProcAddress(module, "rtc_client_exit");
		if (!dyn_rtc_client_exit_) {
            LOGE("load rtc_client_exit failed.");
			return false;
		}

        LOGI("load webrtc win client success.");
#endif

		return true;

	}

}

#endif //win32