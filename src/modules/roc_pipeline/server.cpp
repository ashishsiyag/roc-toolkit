/*
 * Copyright (c) 2015 Mikhail Baranov
 * Copyright (c) 2015 Victor Gaydov
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_core/panic.h"
#include "roc_core/log.h"
#include "roc_datagram/address_to_str.h"

#include "roc_pipeline/server.h"

namespace roc {
namespace pipeline {

Server::Server(datagram::IDatagramReader& datagram_reader,
               audio::ISampleBufferWriter& audio_writer,
               const ServerConfig& config)
    : config_(config)
    , n_channels_(packet::num_channels(config_.channels))
    , datagram_reader_(datagram_reader)
    , audio_writer_(&audio_writer)
    , channel_muxer_(config_.channels, *config_.sample_buffer_composer)
    , session_manager_(config_, channel_muxer_) {
    //
    if (n_channels_ == 0) {
        roc_panic("server: channel mask is zero");
    }

    if (config_.samples_per_tick == 0) {
        roc_panic("server: # of samples per tick is zero");
    }

    if (!config_.byte_buffer_composer) {
        roc_panic("server: byte buffer composer is null");
    }

    if (!config_.sample_buffer_composer) {
        roc_panic("server: sample buffer composer is null");
    }

    if (!config_.session_pool) {
        roc_panic("server: session pool is null");
    }

    if (config_.options & EnableTiming) {
        audio_writer_ = new (timed_writer_)
            audio::TimedWriter(*audio_writer_, config_.channels, config_.sample_rate);
    }
}

size_t Server::num_sessions() const {
    return session_manager_.num_sessions();
}

void Server::add_port(const datagram::Address& address, packet::IPacketParser& parser) {
    session_manager_.add_port(address, parser);
}

void Server::run() {
    roc_log(LOG_DEBUG, "server: starting thread");

    const size_t n_datagrams = config_.max_sessions * config_.max_session_packets;
    const size_t n_buffers = 1;
    const size_t n_samples = config_.samples_per_tick;

    while (!stop_) {
        if (!tick(n_datagrams, n_buffers, n_samples)) {
            break;
        }
    }

    roc_log(LOG_DEBUG, "server: finishing thread");

    audio_writer_->write(audio::ISampleBufferConstSlice());
}

void Server::stop() {
    stop_ = true;
}

bool Server::tick(size_t n_datagrams, size_t n_buffers, size_t n_samples) {
    roc_panic_if(n_samples * n_channels_ == 0);

    for (size_t n = 0; n < n_datagrams; n++) {
        if (datagram::IDatagramConstPtr dgm = datagram_reader_.read()) {
            session_manager_.store(*dgm);
        } else {
            break;
        }
    }

    if (!session_manager_.update()) {
        return false;
    }

    for (size_t n = 0; n < n_buffers; n++) {
        audio::ISampleBufferPtr buffer = config_.sample_buffer_composer->compose();
        if (!buffer) {
            roc_log(LOG_ERROR, "server: can't compose sample buffer");
            return false;
        }

        buffer->set_size(n_samples * n_channels_);

        channel_muxer_.read(*buffer);
        audio_writer_->write(*buffer);
    }

    return true;
}

} // namespace pipeline
} // namespace roc
