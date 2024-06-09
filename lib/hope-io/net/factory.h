/* Copyright (C) 2023 - 2024 Gleb Bezborodov - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the MIT license.
 *
 * You should have received a copy of the MIT license with
 * this file. If not, please write to: bezborodoff.gleb@gmail.com, or visit : https://github.com/glensand/daedalus-proto-lib
 */

#pragma once

#include <string_view>

namespace hope::io {

    enum class e_stream_t : uint8_t;

    class acceptor* create_acceptor();
    class udp_builder* create_udp_builder();
    class acceptor* create_tls_acceptor(std::string_view key, std::string_view cert);

    using read_function_t = std::function<size_t(void*, std::size_t)>;
    using write_function_t = std::function<void(const void*, std::size_t)>;

    class stream* create_stream(unsigned long long socket = 0);
    class stream* create_stream(read_function_t&& in_read_function, write_function_t&& in_write_function, unsigned long long socket = 0);
    class stream* create_receiver(unsigned long long socket = 0);
    class stream* create_sender(unsigned long long socket = 0);
    class stream* create_tls_stream(stream* tcp_stream = nullptr);
    class stream* create_tls_stream(e_stream_t tcp_stream_type);

}
