/*
 * Copyright (c) 2015 Mikhail Baranov
 * Copyright (c) 2015 Victor Gaydov
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_pipeline/session.h
//! @brief Session pipeline.

#ifndef ROC_PIPELINE_SESSION_H_
#define ROC_PIPELINE_SESSION_H_

#include "roc_config/config.h"

#include "roc_core/shared_ptr.h"
#include "roc_core/refcnt.h"
#include "roc_core/list_node.h"
#include "roc_core/list.h"
#include "roc_core/array.h"
#include "roc_core/maybe.h"
#include "roc_core/ipool.h"

#include "roc_datagram/idatagram.h"
#include "roc_datagram/address.h"

#include "roc_packet/ipacket_parser.h"
#include "roc_packet/ipacket_writer.h"
#include "roc_packet/packet_queue.h"
#include "roc_packet/packet_router.h"

#include "roc_fec/decoder.h"

#ifdef ROC_TARGET_OPENFEC
#include "roc_fec/ldpc_block_decoder.h"
#endif

#include "roc_audio/ituner.h"
#include "roc_audio/isink.h"
#include "roc_audio/watchdog.h"
#include "roc_audio/delayer.h"
#include "roc_audio/chanalyzer.h"
#include "roc_audio/streamer.h"
#include "roc_audio/resampler.h"
#include "roc_audio/scaler.h"

namespace roc {
namespace pipeline {

struct ServerConfig;

//! Session pipeline.
//! @remarks
//!  Session object is created for every client connected to server.
//!
//! @see Server.
class Session : public core::RefCnt, public core::ListNode {
public:
    //! Create session.
    Session(const ServerConfig&, const datagram::Address&, packet::IPacketParser&);

    //! Get client address.
    const datagram::Address& address() const;

    //! Parse datagram and add it to internal storage.
    //! @returns
    //!  true if datagram was successfully parsed and stored.
    bool store(const datagram::IDatagram&);

    //! Update renderer state.
    //! @returns
    //!  false if session is broken and should be terminated.
    bool update();

    //! Attach renderer to audio sink.
    void attach(audio::ISink& sink);

    //! Detach renderer from audio sink.
    void detach(audio::ISink& sink);

private:
    enum { MaxChannels = ROC_CONFIG_MAX_CHANNELS };

    virtual void free();

    void make_pipeline_();

    audio::IStreamReader* make_stream_reader_(
        audio::IAudioPacketReader*, packet::channel_t);

    packet::IPacketReader* make_packet_reader_();
    packet::IPacketReader* make_fec_decoder_(packet::IPacketReader*);

    const ServerConfig& config_;
    const datagram::Address address_;
    packet::IPacketParser& packet_parser_;

    core::Maybe<packet::PacketQueue> audio_packet_queue_;
    core::Maybe<packet::PacketQueue> fec_packet_queue_;

    core::Maybe<audio::Delayer> delayer_;
    core::Maybe<audio::Watchdog> watchdog_;

#ifdef ROC_TARGET_OPENFEC
    core::Maybe<fec::LDPC_BlockDecoder> fec_ldpc_decoder_;
    core::Maybe<fec::Decoder> fec_decoder_;
#endif

    core::Maybe<audio::Chanalyzer> chanalyzer_;
    core::Array<core::Maybe<audio::Streamer>, MaxChannels> streamers_;
    core::Array<core::Maybe<audio::Resampler>, MaxChannels> resamplers_;
    core::Maybe<audio::Scaler> scaler_;
    packet::PacketRouter router_;

    core::List<audio::ITuner, core::NoOwnership> tuners_;
    core::Array<audio::IStreamReader*, MaxChannels> readers_;
};

//! Session smart pointer.
typedef core::SharedPtr<Session> SessionPtr;

} // namespace pipeline
} // namespace roc

#endif // ROC_PIPELINE_SESSION_H_
