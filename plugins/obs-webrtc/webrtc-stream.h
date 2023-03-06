#include "rtc/rtc.hpp"

class OBSWebRTCStream {
public:
    OBSWebRTCStream();
    const char* Setup();
    void Connect(const char* sdp);
    void SendVideo(const uint8_t *data,
                   uintptr_t size, uint64_t duration);
    void SendAudio(const uint8_t *data, uintptr_t size,
                   uint64_t duration);

private:
    std::unique_ptr<rtc::PeerConnection> peerConnection_;
    std::shared_ptr<rtc::Track> videoTrack_;
    std::shared_ptr<rtc::Track> audioTrack_;
};
