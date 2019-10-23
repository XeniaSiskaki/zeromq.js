/* Copyright (c) 2017-2019 Rolf Timmermans */
#pragma once

#include "prefix.h"

#include "outgoing_msg.h"

#include "util/arguments.h"
#include "util/error.h"
#include "util/object.h"
#include "util/reaper.h"
#include "util/to_string.h"
#include "util/trash.h"

namespace zmq {
class Context;
class Socket;

struct Terminator {
    constexpr Terminator() noexcept = default;
    void operator()(void* context) {
        assert(context != nullptr);
        auto err = zmq_ctx_term(context);
        assert(err == 0);
    }
};

class Module {
    /* Contains shared global state that will be accessible by all
       agents/threads. */
    class Global {
        using Shared = std::shared_ptr<Global>;
        static Shared Instance();

    public:
        Global();

        /* ZMQ pointer to the global shared context which allows agents/threads
           to communicate over inproc://. */
        void* SharedContext;

        /* A list of ZMQ contexts that will be terminated on a clean exit. */
        ThreadSafeReaper<void, Terminator> ContextTerminator;

        friend class Module;
    };

public:
    explicit Module(Napi::Object exports);

    inline class Global& Global() {
        return *global;
    }

    /* The order of properties defines their destruction in reverse order and is
       very important to ensure a clean process exit. During the destruction of
       other objects buffers might be released, we must delete trash last. */
    Trash<OutgoingMsg::Reference> MsgTrash;

private:
    /* Second to last to be deleted is the global state, which also causes
       context termination (which might block). */
    Global::Shared global = Global::Instance();

public:
    /* Reaper that calls ->Close() on objects that have not been GC'ed so far.
       Some versions of Node will call destructors on environment shutdown,
       while others will *only* call destructors after GC. The reason we need to
       call ->Close() is to ensure proper ZMQ cleanup and releasing underlying
       resources. The versions of Node that do not call destructors *WILL* of
       course leak memory if worker threads are created (in a loop). */
    Reaper<Closable> ObjectReaper;

    /* A JS reference to the default global context. This is a unique object for
       each individual agent/thread, but is in fact a wrapper for the same
       global ZMQ context. */
    Napi::ObjectReference GlobalContext;

    /* JS constructor references. */
    Napi::FunctionReference Context;
    Napi::FunctionReference Socket;
    Napi::FunctionReference Observer;
    Napi::FunctionReference Proxy;
};
}

static_assert(!std::is_copy_constructible<zmq::Module>::value, "not copyable");
static_assert(!std::is_move_constructible<zmq::Module>::value, "not movable");
