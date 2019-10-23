/* Copyright (c) 2017-2019 Rolf Timmermans */
#include "incoming_msg.h"

#include "util/error.h"

namespace zmq {
IncomingMsg::IncomingMsg() : ref(new Reference()) {}

IncomingMsg::~IncomingMsg() {
    if (!moved && ref != nullptr) {
        delete ref;
        ref = nullptr;
    }
}

Napi::Value IncomingMsg::IntoBuffer(const Napi::Env& env) {
    if (moved) {
        /* If ownership has been transferred, do not attempt to read the buffer
           again in any case. This should not happen of course. */
        ErrnoException(env, EINVAL).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    static auto constexpr zero_copy_threshold = 32;

    auto data = reinterpret_cast<uint8_t*>(zmq_msg_data(*ref));
    auto length = zmq_msg_size(*ref);

    if (length > zero_copy_threshold) {
        /* Reuse existing buffer for external storage. This avoids copying but
           does include an overhead in having to call a finalizer when the
           buffer is GC'ed. For very small messages it is faster to copy. */
        moved = true;
        return Napi::Buffer<uint8_t>::New(env, data, length,
            [](const Napi::Env& env, uint8_t*, Reference* ref) { delete ref; }, ref);
    }

    if (length > 0) {
        return Napi::Buffer<uint8_t>::Copy(env, data, length);
    }

    return Napi::Buffer<uint8_t>::New(env, 0);
}

IncomingMsg::Reference::Reference() {
    auto err = zmq_msg_init(&msg);
    assert(err == 0);
}

IncomingMsg::Reference::~Reference() {
    auto err = zmq_msg_close(&msg);
    assert(err == 0);
}
}
