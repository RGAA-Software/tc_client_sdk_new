#ifndef _WEBRTC_CLIENT_H_
#define _WEBRTC_CLIENT_H_

#ifdef WIN32

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "tc_webrtc_client/webrtc_client_api.h"

using rtc_client_init_ptr_t = decltype(&rtc_client_init);
using rtc_client_exit_ptr_t = decltype(&rtc_client_exit);

namespace tc
{

	class RtcVideoFrame {
	public:
		std::vector<uint8_t> buffer_;
		int frame_width_;
		int frame_height_;
		rtc_decoded_video_frame_type frame_type_;
		uint64_t frame_idx_;
	};

	using RtcVideoFramePtr = std::shared_ptr<RtcVideoFrame>;
	using OnRtcVideoFrameCallback = std::function<void(RtcVideoFramePtr&&)>;

	class WebRtcClient {
	public:

		static std::shared_ptr<WebRtcClient> Make();

		WebRtcClient();

		bool Start(const std::string& ip, int port);
		void Exit();

		void RegisterRtcVideoFrameCallback(OnRtcVideoFrameCallback&& cbk);

	private:

		bool LoadWebRtcClient();
		RtcVideoFramePtr MakeVideoFrame(const uint8_t* buffer, int size, int frame_width, int frame_height, rtc_decoded_video_frame_type type);

	private:

		rtc_client_init_ptr_t dyn_rtc_client_init_ = nullptr;
		rtc_client_exit_ptr_t dyn_rtc_client_exit_ = nullptr;

		OnRtcVideoFrameCallback video_frame_callback_ = nullptr;
		uint64_t frame_idx_ = 0;

	};

}

#endif // win32

#endif