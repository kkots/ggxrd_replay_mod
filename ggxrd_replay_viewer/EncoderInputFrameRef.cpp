#include "EncoderInputFrameRef.h"

EncoderInputFrameRef::EncoderInputFrameRef(EncoderInputFrame& refIn) : ptr(&refIn) {
	++refIn.refCount;
}

EncoderInputFrameRef::EncoderInputFrameRef(EncoderInputFrame* ptr) : ptr(ptr) {
	if (ptr) ++ptr->refCount;
}

EncoderInputFrameRef::EncoderInputFrameRef(const EncoderInputFrameRef& source) : ptr(source.ptr) {
	if (ptr) ++ptr->refCount;
}

EncoderInputFrameRef::EncoderInputFrameRef(EncoderInputFrameRef&& source) noexcept : ptr(source.ptr) {
	source.ptr = nullptr;
}

EncoderInputFrameRef::~EncoderInputFrameRef() {
	clear();
}

EncoderInputFrameRef& EncoderInputFrameRef::operator=(const EncoderInputFrameRef& source) {
	clear();
	ptr = source.ptr;
	if (ptr) ++ptr->refCount;
	return *this;
}

EncoderInputFrameRef& EncoderInputFrameRef::operator=(EncoderInputFrameRef&& source) noexcept {
	clear();
	ptr = source.ptr;
	source.ptr = nullptr;
	return *this;
}

EncoderInputFrame* EncoderInputFrameRef::operator->() {
	return ptr;
}

void EncoderInputFrameRef::clear() {
	if (ptr) {
		--ptr->refCount;
		ptr = nullptr;
	}
}
