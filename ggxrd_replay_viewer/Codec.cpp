#include "Codec.h"
#include "WinError.h"
#include "debugMsg.h"
#include "EncodedImageSize.h"

Codec codec;
const unsigned int encoderBitrate = 10000000;
extern HWND mainWindowHandle;

bool Codec::initializePreWindow() {
    HRESULT errorCode = MFStartup(MF_VERSION);
    if (FAILED(errorCode)) {
        WinError winErr;
        debugMsg("MFStartup error %.8x: %s\n", errorCode, winErr.getMessageA().c_str());
        return false;
    }

    if (!initializeEncoder()) return false;

    if (!setEncoderOutputType()) return false;

    CComPtr<IMFMediaType> inputType;
    findAvailableInputType(inputType);
    if (!inputType) {
        debugMsg("Could not find IYUV in the list of available input media types of the encoder.\n");
        return false;
    }

    errorCode = encoder->SetInputType(0, inputType, NULL);
    if (FAILED(errorCode)) {
        debugMsg("SetInputType (on encoder) failed: %.8x\n", errorCode);
        return false;
    }

    if (!initializeEncoderOutputSample()) return false;

    errorCode = encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    if (FAILED(errorCode)) {
        debugMsg("Encoder doesn't want to begin streaming: %.8x\n", errorCode);
        return false;
    }

    if (!initializeDecoder()) {
        return false;
    }

    if (!setDecoderInputType()) {
        return false;
    }

    CComPtr<IMFMediaType> decoderOutputType;
    findAvailableOutputType(decoderOutputType);
    if (!decoderOutputType) {
        debugMsg("Could not find YUY2 in the list of available output media types of the decoder.\n");
        return false;
    }

    errorCode = decoder->SetOutputType(0, decoderOutputType, NULL);
    if (FAILED(errorCode)) {
        debugMsg("SetOutputType (on decoder) failed: %.8x\n", errorCode);
        return false;
    }

    if (!initializeDecoderOutputSample()) {
        return false;
    }

    errorCode = decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    if (FAILED(errorCode)) {
        debugMsg("Decoder doesn't want to begin streaming: %.8x\n", errorCode);
        return false;
    }

    return true;
}

bool Codec::initializeEncoder() {
    CLSID H264CLSID{ 0x0, 0x0, 0x0, 0x0 };
    bool H264CLSIDFound = false;

    CLSID* clsids = nullptr;
    UINT32 clsidsCount = 0;
    HRESULT errorCode = MFTEnum(MFT_CATEGORY_VIDEO_ENCODER, NULL, NULL, NULL, NULL, &clsids, &clsidsCount);
    if (FAILED(errorCode)) {
        debugMsg("MFTEnum (encoder) failed: %.8x\n", errorCode);
        return false;
    }
    CLSID* clsidPtr = clsids;
    for (UINT32 i = 0; i < clsidsCount; ++i) {
        LPWSTR name = nullptr;
        errorCode = MFTGetInfo(*clsidPtr, &name, NULL, NULL, NULL, NULL, NULL);
        if (FAILED(errorCode)) {
            debugMsg("MFTGetInfo (encoder) failed: %.8x\n", errorCode);
            return false;
        }
        if (name) {
            if (lstrcmpW(name, L"H264 Encoder MFT") == 0) {
                H264CLSID = *clsidPtr;
                H264CLSIDFound = true;
            }
            CoTaskMemFree(name);
        }
        if (H264CLSIDFound) break;
        ++clsidPtr;
    }
    if (clsids) {
        CoTaskMemFree(clsids);
    }

    if (!H264CLSIDFound) {
        debugMsg("H.264 encoder not found\n");
        return false;
    }

    errorCode = CoCreateInstance(H264CLSID, NULL, CLSCTX_INPROC_SERVER, IID_IMFTransform, (LPVOID*)&encoder);
    if (FAILED(errorCode)) {
        debugMsg("CoCreateInstance (encoder, H264CLSID, IID_IMFTransform) failed: %.8x\n", errorCode);
        return false;
    }

    errorCode = encoder->QueryInterface(IID_ICodecAPI, (LPVOID*)&encoderCodecApi);
    if (FAILED(errorCode)) {
        debugMsg("QueryInterface (IID_ICodecAPI) failed: %.8x\n", errorCode);
        return false;
    }

    return true;
}

bool Codec::setEncoderOutputType() {
    CComPtr<IMFMediaType> mediaType;
    HRESULT errorCode = encoder->GetOutputAvailableType(0, 0, &mediaType);
    if (FAILED(errorCode)) {
        debugMsg("GetOutputAvailableType failed: %.8x\n", errorCode);
        return false;
    }

    CComPtr<IMFAttributes> attribs;
    errorCode = mediaType->QueryInterface(IID_IMFAttributes, (LPVOID*)&attribs);
    if (FAILED(errorCode)) {
        debugMsg("QueryInterface (in setEncoderOutputType) failed: %.8x\n", errorCode);
        return false;
    }
    errorCode = attribs->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(errorCode)) {
        debugMsg("SetGUID (MF_MT_MAJOR_TYPE) failed: %.8x\n", errorCode);
        return false;
    }
    errorCode = attribs->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (FAILED(errorCode)) {
        debugMsg("SetGUID (MF_MT_SUBTYPE) failed: %.8x\n", errorCode);
        return false;
    }
    errorCode = attribs->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
    if (FAILED(errorCode)) {
        debugMsg("SetUINT32 (MF_MT_MPEG2_PROFILE) failed: %.8x\n", errorCode);
        return false;
    }
    errorCode = MFSetAttributeSize(attribs, MF_MT_FRAME_SIZE, encodedImageWidth, encodedImageHeight);
    if (FAILED(errorCode)) {
        debugMsg("MFSetAttributeSize (MF_MT_FRAME_SIZE) failed: %.8x\n", errorCode);
        return false;
    }
    errorCode = MFSetAttributeRatio(attribs, MF_MT_FRAME_RATE, 60, 1);
    if (FAILED(errorCode)) {
        debugMsg("MFSetAttributeRatio (MF_MT_FRAME_RATE) failed: %.8x\n", errorCode);
        return false;
    }
    errorCode = attribs->SetUINT32(MF_MT_AVG_BITRATE, encoderBitrate);
    if (FAILED(errorCode)) {
        debugMsg("SetUINT32 (MF_MT_AVG_BITRATE) failed: %.8x\n", errorCode);
        return false;
    }
    errorCode = attribs->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (FAILED(errorCode)) {
        debugMsg("SetUINT32 (MF_MT_INTERLACE_MODE) failed: %.8x\n", errorCode);
        return false;
    }

    errorCode = encoder->SetOutputType(0, mediaType, NULL);
    if (FAILED(errorCode)) {
        debugMsg("SetOutputType (on encoder) failed: %.8x\n", errorCode);
        return false;
    }
    return true;
}

void Codec::findAvailableInputType(CComPtr<IMFMediaType>& inputType) {
    for (unsigned int inputMediaTypeCounter = 0; ; ++inputMediaTypeCounter) {
        CComPtr<IMFMediaType> mediaType;
        HRESULT errorCode = encoder->GetInputAvailableType(0, inputMediaTypeCounter, &mediaType);

        if (errorCode == MF_E_NO_MORE_TYPES) {
            break;
        }
        if (FAILED(errorCode)) {
            debugMsg("GetInputAvailableType failed: %.8x\n", errorCode);
            return;
        }

        CComPtr<IMFAttributes> attribs;
        errorCode = mediaType->QueryInterface(IID_IMFAttributes, (LPVOID*)&attribs);
        if (FAILED(errorCode)) {
            debugMsg("QueryInterface (in findAvailableInputType) failed: %.8x\n", errorCode);
            return;
        }

        GUID guid;
        errorCode = attribs->GetGUID(MF_MT_SUBTYPE, &guid);
        if (FAILED(errorCode)) {
            debugMsg("Failed to get MF_MT_SUBTYPE from available input media type: %.8x\n", errorCode);
            return;
        }

        if (guid == MFVideoFormat_IYUV) {
            inputType = mediaType;
            break;
        }
    }
}

bool Codec::initializeEncoderOutputSample() {
    HRESULT errorCode = encoder->GetOutputStreamInfo(0, &encoderOutputStreamInfo);
    if (FAILED(errorCode)) {
        debugMsg("encoder->GetOutputStreamInfo failed: %.8x\n", errorCode);
        return false;
    }

    encoderOutputSample.dwStreamID = 0;

    return true;
}

bool Codec::initializeDecoder() {
    CLSID H264CLSID{ 0x0, 0x0, 0x0, 0x0 };
    bool H264CLSIDFound = false;

    CLSID* clsids = nullptr;
    UINT32 clsidsCount = 0;
    HRESULT errorCode = MFTEnum(MFT_CATEGORY_VIDEO_DECODER, NULL, NULL, NULL, NULL, &clsids, &clsidsCount);
    if (FAILED(errorCode)) {
        debugMsg("MFTEnum (decoder) failed: %.8x\n", errorCode);
        return false;
    }
    CLSID* clsidPtr = clsids;
    for (UINT32 i = 0; i < clsidsCount; ++i) {
        LPWSTR name = nullptr;
        errorCode = MFTGetInfo(*clsidPtr, &name, NULL, NULL, NULL, NULL, NULL);
        if (FAILED(errorCode)) {
            debugMsg("MFTGetInfo (decoder) failed: %.8x\n", errorCode);
            return false;
        }
        if (name) {
            if (lstrcmpW(name, L"Microsoft H264 Video Decoder MFT") == 0) {
                H264CLSID = *clsidPtr;
                H264CLSIDFound = true;
            }
            CoTaskMemFree(name);
        }
        if (H264CLSIDFound) break;
        ++clsidPtr;
    }
    if (clsids) {
        CoTaskMemFree(clsids);
    }

    if (!H264CLSIDFound) {
        debugMsg("H.264 decoder not found\n");
        return false;
    }

    errorCode = CoCreateInstance(H264CLSID, NULL, CLSCTX_INPROC_SERVER, IID_IMFTransform, (LPVOID*)&decoder);
    if (FAILED(errorCode)) {
        debugMsg("CoCreateInstance (decoder, H264CLSID, IID_IMFTransform) failed: %.8x\n", errorCode);
        return false;
    }

    return true;
}

bool Codec::setDecoderInputType() {

    CComPtr<IMFMediaType> encoderOutputMediaType;
    HRESULT errorCode = encoder->GetOutputCurrentType(0, &encoderOutputMediaType);
    if (FAILED(errorCode)) {
        debugMsg("GetOutputCurrentType (on encoder) failed: %.8x\n", errorCode);
        return false;
    }

    errorCode = decoder->SetInputType(0, encoderOutputMediaType, NULL);
    if (FAILED(errorCode)) {
        debugMsg("SetInputType (on decoder) failed: %.8x\n", errorCode);
        return false;
    }
    return true;

    CComPtr<IMFMediaType> mediaType;
    errorCode = decoder->GetInputAvailableType(0, 0, &mediaType);
    if (FAILED(errorCode)) {
        debugMsg("GetInputAvailableType failed: %.8x\n", errorCode);
        return false;
    }

    CComPtr<IMFAttributes> attribs;
    errorCode = mediaType->QueryInterface(IID_IMFAttributes, (LPVOID*)&attribs);
    if (FAILED(errorCode)) {
        debugMsg("QueryInterface (in setDecoderInputType) failed: %.8x\n", errorCode);
        return false;
    }
    errorCode = attribs->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(errorCode)) {
        debugMsg("SetGUID (MF_MT_MAJOR_TYPE) failed: %.8x\n", errorCode);
        return false;
    }
    errorCode = attribs->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (FAILED(errorCode)) {
        debugMsg("SetGUID (MF_MT_SUBTYPE) failed: %.8x\n", errorCode);
        return false;
    }

    errorCode = decoder->SetInputType(0, mediaType, NULL);
    if (FAILED(errorCode)) {
        debugMsg("SetInputType (on decoder) failed: %.8x\n", errorCode);
        return false;
    }
    return true;
}

void Codec::findAvailableOutputType(CComPtr<IMFMediaType>& outputType) {
    for (unsigned int outputMediaTypeCounter = 0; ; ++outputMediaTypeCounter) {
        CComPtr<IMFMediaType> mediaType;
        HRESULT errorCode = decoder->GetOutputAvailableType(0, outputMediaTypeCounter, &mediaType);

        if (errorCode == MF_E_NO_MORE_TYPES) {
            break;
        }
        if (FAILED(errorCode)) {
            debugMsg("GetOutputAvailableType failed: %.8x\n", errorCode);
            return;
        }

        CComPtr<IMFAttributes> attribs;
        errorCode = mediaType->QueryInterface(IID_IMFAttributes, (LPVOID*)&attribs);
        if (FAILED(errorCode)) {
            debugMsg("QueryInterface (in findAvailableOutputType) failed: %.8x\n", errorCode);
            return;
        }

        GUID guid;
        errorCode = attribs->GetGUID(MF_MT_SUBTYPE, &guid);
        if (FAILED(errorCode)) {
            debugMsg("Failed to get MF_MT_SUBTYPE from available output media type: %.8x\n", errorCode);
            return;
        }

        if (guid == MFVideoFormat_YUY2) {
            outputType = mediaType;
            break;
        }
    }
}

bool Codec::initializeDecoderOutputSample() {

    HRESULT errorCode = decoder->GetOutputStreamInfo(0, &decoderOutputStreamInfo);
    if (FAILED(errorCode)) {
        debugMsg("decoder->GetOutputStreamInfo failed: %.8x\n", errorCode);
        return false;
    }

    decoderOutputSample.dwStreamID = 0;

    IMFSample* sample = nullptr;
    errorCode = MFCreateSample(&sample);
    if (FAILED(errorCode)) {
        debugMsg("Failed to create a sample: %.8x\n", errorCode);
        return false;
    }

    decoderOutputSample.pSample = sample;

    return true;
}

void Codec::shutdown() {
    MFShutdown();
}

bool Codec::encodeInput(CComPtr<IMFSample>& sample) {
    HRESULT errorCode;
    if (!encoderStreamGoing) {
        errorCode = encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        if (FAILED(errorCode)) {
            MessageBox(mainWindowHandle, debugStr(TEXT("Encoder doesn't want to get notified of start of stream: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
            return false;
        }
        encoderStreamGoing = true;
        VARIANT value{ 0 };
        value.vt = VT_UI4;
        value.ulVal = 1;
        errorCode = encoderCodecApi->SetValue(&CODECAPI_AVEncVideoForceKeyFrame, &value);
        if (FAILED(errorCode)) {
            MessageBox(mainWindowHandle, debugStr(TEXT("SetValue (CODECAPI_AVEncVideoForceKeyFrame) failed: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
            return false;
        }
    }
    errorCode = encoder->ProcessInput(0, sample, NULL);
    if (FAILED(errorCode)) {
        if (errorCode == E_INVALIDARG) {
            MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessInput failed: E_INVALIDARG\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_INVALIDSTREAMNUMBER) {
            MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessInput failed: MF_E_INVALIDSTREAMNUMBER\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_NO_SAMPLE_DURATION) {
            MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessInput failed: MF_E_NO_SAMPLE_DURATION\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_NO_SAMPLE_TIMESTAMP) {
            MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessInput failed: MF_E_NO_SAMPLE_TIMESTAMP\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_NOTACCEPTING) {
            MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessInput failed: MF_E_NOTACCEPTING\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_TRANSFORM_TYPE_NOT_SET) {
            MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessInput failed: MF_E_TRANSFORM_TYPE_NOT_SET\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_UNSUPPORTED_D3D_TYPE) {
            MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessInput failed: MF_E_UNSUPPORTED_D3D_TYPE\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else {
            MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessInput failed: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
        }
        return false;
    }
    return true;
}

bool Codec::encoderOutput(IMFSample* sample) {
    HRESULT errorCode;
    if (encoderStreamGoing) {
        errorCode = encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        if (FAILED(errorCode)) {
            MessageBox(mainWindowHandle, debugStr(TEXT("Failed to process message MFT_MESSAGE_NOTIFY_END_OF_STREAM on the encoder: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
            return false;
        }
        encoderStreamGoing = false;

        errorCode = encoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        if (FAILED(errorCode)) {
            MessageBox(mainWindowHandle, debugStr(TEXT("Failed to process message MFT_MESSAGE_COMMAND_DRAIN on the encoder: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
            return false;
        }
    }

    encoderOutputSample.dwStatus = 0;
    if (encoderOutputSample.pEvents) {
        encoderOutputSample.pEvents->Release();
        encoderOutputSample.pEvents = NULL;
    }

    encoderOutputSample.pSample = sample;
    DWORD outputFlags = 0;
    errorCode = encoder->ProcessOutput(NULL, 1, &encoderOutputSample, &outputFlags);
    if (errorCode == MF_E_TRANSFORM_NEED_MORE_INPUT) return true;
    if (FAILED(errorCode)) {
        switch (errorCode) {
        case E_UNEXPECTED: MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessOutput failed: E_UNEXPECTED\n")).c_str(), TEXT("Error"), MB_OK); break;
        case E_FAIL: MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessOutput failed: E_FAIL\n")).c_str(), TEXT("Error"), MB_OK); break;
        case MF_E_INVALIDSTREAMNUMBER: MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessOutput failed: MF_E_INVALIDSTREAMNUMBER\n")).c_str(), TEXT("Error"), MB_OK); break;
        case MF_E_TRANSFORM_STREAM_CHANGE: MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessOutput failed: MF_E_TRANSFORM_STREAM_CHANGE\n")).c_str(), TEXT("Error"), MB_OK); break;
        case MF_E_TRANSFORM_TYPE_NOT_SET: MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessOutput failed: MF_E_TRANSFORM_TYPE_NOT_SET\n")).c_str(), TEXT("Error"), MB_OK); break;
        default: MessageBox(mainWindowHandle, debugStr(TEXT("encoder->ProcessOutput failed: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
        }
        return false;
    }
    return true;
}

void Codec::destroy() {
    encoder = nullptr;
    encoderCodecApi = nullptr;
    decoder = nullptr;
    decoderOutputSampleBuffer = nullptr;
}

bool Codec::decodeInput(CComPtr<IMFSample>& sample) {
    HRESULT errorCode;
    if (!decoderStreamGoing) {
        errorCode = decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        if (FAILED(errorCode)) {
            MessageBox(mainWindowHandle, debugStr(TEXT("Decoder doesn't want to get notified of start of stream: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
            return false;
        }
        decoderStreamGoing = true;
    }
    errorCode = decoder->ProcessInput(0, sample, NULL);
    if (FAILED(errorCode)) {
        if (errorCode == E_INVALIDARG) {
            MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessInput failed: E_INVALIDARG\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_INVALIDSTREAMNUMBER) {
            MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessInput failed: MF_E_INVALIDSTREAMNUMBER\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_NO_SAMPLE_DURATION) {
            MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessInput failed: MF_E_NO_SAMPLE_DURATION\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_NO_SAMPLE_TIMESTAMP) {
            MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessInput failed: MF_E_NO_SAMPLE_TIMESTAMP\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_NOTACCEPTING) {
            MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessInput failed: MF_E_NOTACCEPTING\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_TRANSFORM_TYPE_NOT_SET) {
            MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessInput failed: MF_E_TRANSFORM_TYPE_NOT_SET\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else if (errorCode == MF_E_UNSUPPORTED_D3D_TYPE) {
            MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessInput failed: MF_E_UNSUPPORTED_D3D_TYPE\n")).c_str(), TEXT("Error"), MB_OK);
        }
        else {
            MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessInput failed: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
        }
        return false;
    }
    return true;
}

bool Codec::decoderOutput(IMFSample* sample) {
    HRESULT errorCode;
    if (decoderStreamGoing) {
        errorCode = decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        if (FAILED(errorCode)) {
            MessageBox(mainWindowHandle, debugStr(TEXT("Failed to process message MFT_MESSAGE_NOTIFY_END_OF_STREAM on the decoder: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
            return false;
        }
        decoderStreamGoing = false;

        errorCode = decoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        if (FAILED(errorCode)) {
            MessageBox(mainWindowHandle, debugStr(TEXT("Failed to process message MFT_MESSAGE_COMMAND_DRAIN on the decoder: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
            return false;
        }
    }

    decoderOutputSample.dwStatus = 0;
    if (decoderOutputSample.pEvents) {
        decoderOutputSample.pEvents->Release();
        decoderOutputSample.pEvents = NULL;
    }

    decoderOutputSample.pSample = sample;
    DWORD outputFlags = 0;
    errorCode = decoder->ProcessOutput(NULL, 1, &decoderOutputSample, &outputFlags);
    if (errorCode == MF_E_TRANSFORM_NEED_MORE_INPUT) return true;
    if (FAILED(errorCode)) {
        switch (errorCode) {
        case E_UNEXPECTED: MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessOutput failed: E_UNEXPECTED\n")).c_str(), TEXT("Error"), MB_OK); break;
        case E_FAIL: MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessOutput failed: E_FAIL\n")).c_str(), TEXT("Error"), MB_OK); break;
        case MF_E_INVALIDSTREAMNUMBER: MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessOutput failed: MF_E_INVALIDSTREAMNUMBER\n")).c_str(), TEXT("Error"), MB_OK); break;
        case MF_E_TRANSFORM_STREAM_CHANGE: MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessOutput failed: MF_E_TRANSFORM_STREAM_CHANGE\n")).c_str(), TEXT("Error"), MB_OK); break;
        case MF_E_TRANSFORM_TYPE_NOT_SET: MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessOutput failed: MF_E_TRANSFORM_TYPE_NOT_SET\n")).c_str(), TEXT("Error"), MB_OK); break;
        default: MessageBox(mainWindowHandle, debugStr(TEXT("decoder->ProcessOutput failed: %.8x\n"), errorCode).c_str(), TEXT("Error"), MB_OK);
        }
        return false;
    }
    return true;
}
