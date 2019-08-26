#pragma once

#include "platform.hpp"

class OBSVideoFrame : public IDeckLinkMutableVideoFrame
{
private:
	BMDFrameFlags flags;
	BMDPixelFormat pixelFormat = bmdFormat8BitYUV;

	long width;
	long height;
	long rowBytes;

	unsigned char* data;

public:
	OBSVideoFrame(long width, long height);

	HRESULT SetFlags(BMDFrameFlags newFlags) override;

	HRESULT SetTimecode(BMDTimecodeFormat format, IDeckLinkTimecode* timecode) override;

	HRESULT SetTimecodeFromComponents
			(BMDTimecodeFormat format, uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t frames, BMDTimecodeFlags flags) override;

	HRESULT SetAncillaryData (IDeckLinkVideoFrameAncillary* ancillary) override;

	HRESULT SetTimecodeUserBits
			(BMDTimecodeFormat format, BMDTimecodeUserBits userBits) override;

	long GetWidth () override;

	long GetHeight () override;

	long GetRowBytes () override;

	BMDPixelFormat GetPixelFormat () override;

	BMDFrameFlags GetFlags () override;

	HRESULT GetBytes (void **buffer) override;

	//Dummy implementations of remaining virtual methods
	virtual HRESULT GetTimecode (/* in */ BMDTimecodeFormat format, /* out */ IDeckLinkTimecode **timecode) { return E_NOINTERFACE; };
	virtual HRESULT GetAncillaryData (/* out */ IDeckLinkVideoFrameAncillary **ancillary) { return E_NOINTERFACE; } ;

	// IUnknown interface (dummy implementation)
	virtual HRESULT QueryInterface (REFIID iid, LPVOID *ppv) {return E_NOINTERFACE;}
	virtual ULONG AddRef () {return 1;}
	virtual ULONG Release () {return 1;}
};

