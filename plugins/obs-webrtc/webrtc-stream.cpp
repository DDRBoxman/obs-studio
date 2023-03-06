#include "webrtc-stream.h"
#include "util/base.h"

OBSWebRTCStream::OBSWebRTCStream() {
    peerConnection_ = std::make_unique<rtc::PeerConnection>();
}

const char* OBSWebRTCStream::Setup() {
    peerConnection_->onStateChange(
                                   [](rtc::PeerConnection::State state) { std::cout << "State: " << state << std::endl; });
    
    /*peerConnection_->onGatheringStateChange([this->peerConnection_](rtc::PeerConnection::GatheringState state) {
     std::cout << "Gathering State: " << state << std::endl;
     if (state == rtc::PeerConnection::GatheringState::Complete) {
     auto description = pc->localDescription();
     json message = {{"type", description->typeString()},
     {"sdp", std::string(description.value())}};
     std::cout << message << std::endl;
     }
     });*/
    
    const rtc::string cname = "video-stream";
    const rtc::string msid = "stream1";
    
    const rtc::SSRC ssrc = 42;
    rtc::Description::Video media(cname, rtc::Description::Direction::SendOnly);
    media.addH264Codec(96); // Must match the payload type of the external h264 RTP stream
    media.addSSRC(ssrc, cname, msid, cname);
    videoTrack_ = peerConnection_->addTrack(media);
    
    // create RTP configuration
    auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, cname, 96, rtc::H264RtpPacketizer::defaultClockRate);
    // create packetizer
    auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(rtc::H264RtpPacketizer::Separator::Length, rtpConfig);
    // create H264 handler
    auto h264Handler = std::make_shared<rtc::H264PacketizationHandler>(packetizer);
    // add RTCP SR handler
    auto srReporter = std::make_shared<rtc::RtcpSrReporter>(rtpConfig);
    h264Handler->addToChain(srReporter);
    // add RTCP NACK handler
    auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
    h264Handler->addToChain(nackResponder);
    // set handler
    videoTrack_->setMediaHandler(h264Handler);
    
    peerConnection_->setLocalDescription();
    
    auto description = peerConnection_->localDescription();
    auto sdp = new std::string(description->generateSdp());
    return sdp->c_str();
}

void OBSWebRTCStream::Connect(const char* sdp)
{
    blog(LOG_DEBUG, "sdp: %s", sdp);

    rtc::Description answer(sdp, rtc::Description::Type::Answer);
    peerConnection_->setRemoteDescription(answer);
}

void OBSWebRTCStream::SendVideo(const uint8_t *data, uintptr_t size, uint64_t duration)
{
    UNUSED_PARAMETER(duration);
    //auto rtpConfig = trackData->sender->rtpConfig;

/*
    // sample time is in us, we need to convert it to seconds
    auto elapsedSeconds = double(sampleTime) / (1000 * 1000);
    // get elapsed time in clock rate
    uint32_t elapsedTimestamp = rtpConfig->secondsToTimestamp(elapsedSeconds);
    // set new timestamp
    rtpConfig->timestamp = rtpConfig->startTimestamp + elapsedTimestamp;
    */
    
    videoTrack_->send((const rtc::byte*)data, size);
    
}

void OBSWebRTCStream::SendAudio(const uint8_t *data, uintptr_t size, uint64_t duration)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(size);
    UNUSED_PARAMETER(duration);
}
