/* Copyright (C) 2023 - 2024 Gleb Bezborodov - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the MIT license.
 *
 * You should have received a copy of the MIT license with
 * this file. If not, please write to: bezborodoff.gleb@gmail.com, or visit : https://github.com/glensand/daedalus-proto-lib
 */

#include <array>
#include <format>
#include <iostream>

#include "hope-io/coredefs.h"
#include "openssl/bio.h"
#include "openssl/buffer.h"
#include "openssl/evp.h"

#ifdef ICARUS_WIN

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#include <stdexcept>

#include <algorithm>
#include <ranges>
#include <sstream>
#include <string_view>
#include <vector>

#include "hope-io/net/stream.h"
#include "hope-io/net/init.h"
#include "hope-io/net/factory.h"

// For internal use, since windows one is not acceptable
#undef INVALID_SOCKET
#define INVALID_SOCKET 0

namespace {

    class win_stream final : public hope::io::stream {
    public:
        explicit win_stream(unsigned long long in_socket) {
            m_socket = in_socket;
        }

        virtual ~win_stream() override {
            win_stream::disconnect();
        }

    private:
        virtual std::string get_endpoint() const override {
            assert(false && "not implemented");
            return "";
        }

        [[nodiscard]] int32_t platform_socket() const override {
            return (int32_t)m_socket;
        }

        virtual void connect(const std::string_view ip, std::size_t port) override {
            // just clear entire structures
            if (m_socket != INVALID_SOCKET)
                throw std::runtime_error("hope-io/win_stream: had already been connected");

            addrinfo* result_addr_info{ nullptr };
            addrinfo hints_addr_info{ };
            ZeroMemory(&hints_addr_info, sizeof(hints_addr_info));
            hints_addr_info.ai_family = AF_INET;
            hints_addr_info.ai_socktype = SOCK_STREAM;
            hints_addr_info.ai_protocol = IPPROTO_TCP;

            // Resolve the server address and port
            const int32_t result = getaddrinfo(ip.data(), std::to_string(port).c_str(), &hints_addr_info, &result_addr_info);
            struct free_address_info final {  // NOLINT(cppcoreguidelines-special-member-functions)
                ~free_address_info() {
                    if (addr_info) {
                        freeaddrinfo(addr_info);
                    }
                }
                addrinfo* addr_info;
            } free_addr_info{ result_addr_info };

            if (result != 0) {
                // todo:: add addr to the exception, add log
                throw std::runtime_error("hope-io/win_stream: could not resolve address");
            }

            m_socket = INVALID_SOCKET;

            // Attempt to connect to an address until one succeeds
            for (const auto* address_info = result_addr_info;
                address_info != nullptr && m_socket == INVALID_SOCKET; address_info = address_info->ai_next) {
                m_socket = ::socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
                if (m_socket != INVALID_SOCKET)
                {
                    int on = 1;
                    int error = setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));
                    if (error == 0) {
                        error = ::connect(m_socket, address_info->ai_addr, (int)address_info->ai_addrlen);
                    }

                    if (error == SOCKET_ERROR) {
                        closesocket(m_socket);
                        m_socket = INVALID_SOCKET;
                    }
                }
            }

            if (m_socket == INVALID_SOCKET) {
                // todo:: add addr to the exception, add log
                throw std::runtime_error("hope-io/win_stream: Could not connect socket");
            }
        }

        virtual void disconnect() override {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        virtual void write(const void* data, std::size_t length) override {
            // todo a
            const auto sent = send(m_socket, (const char*)data, (int)length, 0);
            if (sent == SOCKET_ERROR) {
                // TODO use WSAGetLastError
                throw std::runtime_error("hope-io/win_stream: Failed to send data");
            }

            assert((std::size_t)sent == length);
        }

        virtual void read(void* data, std::size_t length) override {
            auto* buffer = (char*)data;
            while (length != 0) {
                const auto received = recv(m_socket, buffer, (int)length, 0);
                if (received < 0) {
                    // TODO use WSAGetLastError
                    throw std::runtime_error("hope-io/win_stream: Failed to receive data");
                }
                length -= received;
                buffer += received;
            }
        }

        virtual void stream_in(std::string& buffer) override {
            assert(false && "Not implemented");
        }

        SOCKET m_socket{ INVALID_SOCKET };
    };

    class win_websockets_stream final : public hope::io::stream {
    public:
        explicit win_websockets_stream(hope::io::read_function_t in_read_function, hope::io::write_function_t in_write_function, unsigned long long in_socket) {
            read_function = std::forward<hope::io::read_function_t>(in_read_function);
            write_function = std::forward<hope::io::write_function_t>(in_write_function);
            m_socket = in_socket;
        }

        virtual ~win_websockets_stream() override {
            win_websockets_stream::disconnect();
        }

    private:
        void connect(std::string_view ip, std::size_t port) override {
            // just clear entire structures
            if (m_socket != INVALID_SOCKET)
                throw std::runtime_error("hope-io/win_websockets_stream: had already been connected");

            addrinfo* result_addr_info{ nullptr };
            addrinfo hints_addr_info{ };
            ZeroMemory(&hints_addr_info, sizeof(hints_addr_info));
            hints_addr_info.ai_family = AF_UNSPEC;
            hints_addr_info.ai_socktype = SOCK_STREAM;

            // Resolve the server address and port
            const int32_t result = getaddrinfo(ip.data(), std::to_string(port).c_str(), &hints_addr_info, &result_addr_info);
            struct free_address_info final {  // NOLINT(cppcoreguidelines-special-member-functions)
                ~free_address_info() {
                    if (addr_info) {
                        freeaddrinfo(addr_info);
                    }
                }
                addrinfo* addr_info;
            } free_addr_info{ result_addr_info };

            if (result != 0) {
                // todo:: add addr to the exception, add log
                throw std::runtime_error("hope-io/win_websockets_stream: could not resolve address");
            }

            m_socket = INVALID_SOCKET;

            // Attempt to connect to an address until one succeeds
            for (const auto* address_info = result_addr_info;
                address_info != nullptr && m_socket == INVALID_SOCKET; address_info = address_info->ai_next) {
                m_socket = ::socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
                if (m_socket != INVALID_SOCKET)
                {
                    const int error = ::connect(m_socket, address_info->ai_addr, (int)address_info->ai_addrlen);
                    if (error == SOCKET_ERROR) {
                        closesocket(m_socket);
                        m_socket = INVALID_SOCKET;
                    }
                }
            }

            if (m_socket == INVALID_SOCKET) {
                // todo:: add addr to the exception, add log
                throw std::runtime_error("hope-io/win_websockets_stream: Could not connect web socket");
            }

            host = ip; 
        }
        void disconnect() override {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        void send_handshake(std::string_view request) {
            accept_handshake = false;

            constexpr auto web_version = "HTTP/1.1";
            constexpr auto socket_version = "13";

            constexpr auto key_length = 0x10;

            constexpr auto request_format =
                "GET {} {}\r\n"
                "Host: {}\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Key: {}\r\n"
                "Sec-WebSocket-Version: {}\r\n"
                "\r\n";

            const auto generated_key = base64_encode(random_bytes(key_length));

            generated_header = std::format(request_format, request, web_version, host, generated_key, socket_version);

            write_function(generated_header.data(), generated_header.length());
        }

        void try_to_accept_handshake() {

            assert(!accept_handshake && "Already accepted");

            char header_buffer[8192];
            if (const auto read_bytes = read_function(header_buffer, sizeof(header_buffer)))
            {
                static auto&& split_headers = [](const std::string_view& in_value, char in_delimiter = '\n') {
                    std::unordered_map<std::string_view, std::string_view> out_values;
                    for (const auto value : std::views::split(in_value, in_delimiter)) {
                        const std::string_view key_value(value.begin(), value.end());
                        if (key_value.length() > 1) {
                            const auto key_value_separator_index = key_value.find_first_of(':');
                            if (key_value_separator_index != std::string_view::npos) {
                                const auto value_source_offset = std::min<size_t>(key_value_separator_index + 2, key_value.length() - 1);

                                const std::string_view result_key(key_value.data(), key_value_separator_index);
                                const std::string_view result_value(key_value.data() + value_source_offset, key_value.length() - value_source_offset - 1);

                                out_values.insert({ result_key, result_value });
                            }
                        }
                    }
                    return out_values;
                };

                static auto&& check_header_value = [](auto&& headers, auto&& key, auto&& value)
                {
                    auto&& it = headers.find(key);
                    return it != headers.cend() && std::ranges::equal(it->second, value);
                };

                static auto&& check_header_has_value = [](auto&& headers, auto&& key)
                {
                    auto&& it = headers.find(key);
                    return it != headers.cend() && !it->second.empty();
                };

                auto&& headers = split_headers(std::string_view(header_buffer, read_bytes));

                using namespace std::literals;
                if (!check_header_value(headers, "Connection"sv, "upgrade"sv)) {
                    // TODO: Write something to log
                    return;
                }
                if (!check_header_value(headers, "Upgrade"sv, "websocket"sv)) {
                    // TODO: Write something to log
                    return;
                }
                if (!check_header_has_value(headers, "Sec-WebSocket-Accept"sv)) {
                    // TODO: Write something to log
                    return;
                }

                accept_handshake = true;
            }
        }


        void write(const void* data, std::size_t length) override {
            const std::string_view request(static_cast<std::string_view::const_pointer>(data), length);
            send_handshake(request);

            try_to_accept_handshake();
        }

        void read(void* data, std::size_t length) override {
            assert(false && "Not implemented");
        }
        void stream_in(std::string& buffer) override {
            assert(accept_handshake && "Need handshake to read data");

            if (failed_opt_code) {
	            return;
            }

            constexpr auto web_socket_header_bytes = 0x2;
            std::uint8_t header_data[web_socket_header_bytes];
            		
			if (read_function(header_data, web_socket_header_bytes) < web_socket_header_bytes) {
                // TODO: Add log or something
                return;
			}

         	constexpr auto EOF_BIT = 0x80;
         	constexpr auto OP_CODE_BIT = 0x0F;
         	constexpr auto MASK_BIT = 0x80;
         	constexpr auto SOURCE_LEN_BIT = 0x7F;

			constexpr auto OPCODE_TEXT = 0x1;

			const std::uint8_t is_eof = header_data[0] & EOF_BIT;
			const std::uint8_t op_code = header_data[0] & OP_CODE_BIT;
			const std::uint8_t mask = header_data[1] & MASK_BIT;

			if (op_code != OPCODE_TEXT)
			{
                // TODO: Add log or something
                failed_opt_code = true;
                return;
			}

			static auto&& read_package_length = [&](size_t& out_package_length) {

				out_package_length = header_data[1] & SOURCE_LEN_BIT;

				std::uint8_t extra_length_bytes = 0;
				const std::uint8_t package_length = header_data[1] & SOURCE_LEN_BIT;
				if (package_length == 126u) {
					extra_length_bytes = 0x2;
				}
				else if (package_length == 127u) {
					extra_length_bytes = 0x8;
				}

				if (extra_length_bytes > 0) {
					std::vector<std::uint8_t> data_length_buffer(extra_length_bytes);
                    if (read_function(data_length_buffer.data(), data_length_buffer.size()) != data_length_buffer.size()) {
	                    return false;
                    }

					out_package_length = 0ull;
					for (auto&& i = 0; i < extra_length_bytes; i++) {
						out_package_length = (out_package_length << 0x8) + data_length_buffer[i];
					}
				}
				else {
					out_package_length = package_length;
				}

				return true;
			};

            size_t package_length;
            if (!read_package_length(package_length)) {
                // TODO: Add log or something
	            return;
            }

            std::array<char, 1024> read_buffer;
            while (package_length > 0) {
	            const size_t read_chunk = std::min<size_t>(package_length, read_buffer.size());
                const size_t read_bytes = read_function(read_buffer.data(), read_chunk);

                buffer.append(read_buffer.data(), read_bytes);

                package_length = package_length < read_bytes ? 0 : package_length - read_bytes;
            }
        }

        [[nodiscard]] std::string get_endpoint() const override {
            assert(false && "not implemented");
            return "";
        }
        [[nodiscard]] int32_t platform_socket() const override {
            return static_cast<int32_t>(m_socket);
        }

        // TODO: Use thread safe
        static std::string random_bytes(const size_t bytes) {
            static std::once_flag init_srand_flag;
            std::call_once(init_srand_flag, [] { std::srand(std::time(nullptr)); });

            std::string out_result;
            out_result.reserve(bytes);

            using namespace std::literals;
            constexpr auto values = "0123456789abcdefABCDEFGHIJKLMNOPQRSTUVEXYZ"sv;

            for (size_t i = 0; i < bytes; i++) {
                out_result += values[std::rand() % values.length()];
            }

            return out_result;
        }

        static std::string base64_encode(const std::string& input) {
	        BUF_MEM* bufferPtr;

            BIO* b64 = BIO_new(BIO_f_base64());
            BIO* bio = BIO_new(BIO_s_mem());
            bio = BIO_push(b64, bio);

            BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines - write everything in one line
            BIO_write(bio, input.c_str(), input.length());
            BIO_flush(bio);
            BIO_get_mem_ptr(bio, &bufferPtr);
            BIO_set_close(bio, BIO_NOCLOSE);
            BIO_free_all(bio);

            std::string encoded(bufferPtr->data, bufferPtr->length);
            BUF_MEM_free(bufferPtr);
            return encoded;
        }

        std::string host;
        std::string generated_header;

        hope::io::read_function_t read_function;
        hope::io::write_function_t write_function;

        bool failed_opt_code = false;
        bool accept_handshake = false;

        SOCKET m_socket{ INVALID_SOCKET };
    };
}

namespace hope::io {

    stream* create_stream(unsigned long long socket) {
        return new win_stream(socket);
    }

    stream* create_stream(read_function_t&& in_read_function, write_function_t&& in_write_function, unsigned long long socket) {
        return new win_websockets_stream(std::forward<read_function_t>(in_read_function), std::forward<write_function_t>(in_write_function), socket);
    }

}

#endif
