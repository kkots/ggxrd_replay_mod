#pragma once
#include "framework.h"

// the Rpcrt4.lib is needed for UuidCompare
#include <new>
#include <mfapi.h>
#include <mfidl.h>
#include <Shlwapi.h>
#include <stdio.h>
#include <Mferror.h>  // for Media Foundation error codes
#include <codecapi.h>  // for CODECAPI_AVEncVideoForceKeyFrame
#include <strmif.h>  // for ICodecAPI
// the Strmiids.lib is needed for IID_ICodecAPI definition

#include <atlbase.h>  // for CComPtr

#include <vector>

class Codec
{
public:
    bool initializePreWindow();
    void shutdown();
    bool encodeInput(CComPtr<IMFSample>& sample);
    bool encoderOutput(IMFSample* sample);
    bool decodeInput(CComPtr<IMFSample>& sample);
    bool decoderOutput(IMFSample* sample);
    void destroy();
    MFT_OUTPUT_STREAM_INFO encoderOutputStreamInfo;
    MFT_OUTPUT_STREAM_INFO decoderOutputStreamInfo;
private:
    bool initializeEncoder();
    bool setEncoderOutputType();
    bool initializeEncoderOutputSample();
    void findAvailableInputType(CComPtr<IMFMediaType>& inputType);

    bool initializeDecoder();
    bool setDecoderInputType();
    bool initializeDecoderOutputSample();
    void findAvailableOutputType(CComPtr<IMFMediaType>& outputType);

    CComPtr<IMFTransform> encoder;
    CComPtr<ICodecAPI> encoderCodecApi;
    MFT_OUTPUT_DATA_BUFFER encoderOutputSample;
    bool encoderStreamGoing = false;

    CComPtr<IMFTransform> decoder;
    MFT_OUTPUT_DATA_BUFFER decoderOutputSample;
    CComPtr<IMFMediaBuffer> decoderOutputSampleBuffer;
    unsigned int decoderOutputSampleAlignment = 0;
    bool decoderStreamGoing = false;
};

extern Codec codec;
