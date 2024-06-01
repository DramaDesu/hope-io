/* Copyright (C) 2023 - 2024 Gleb Bezborodov - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the MIT license.
 *
 * You should have received a copy of the MIT license with
 * this file. If not, please write to: bezborodoff.gleb@gmail.com, or visit : https://github.com/glensand/daedalus-proto-lib
 */

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
        explicit win_websockets_stream(unsigned long long in_socket) {
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
            read_state = e_read_result::none;
            m_socket = INVALID_SOCKET;
        }
        void write(const void* data, std::size_t length) override {
            assert(false && "Not implemented");
        }
        void read(void* data, std::size_t length) override {
            assert(false && "Not implemented");
        }
        void stream_in(std::string& buffer) override {
            assert(false && "Not implemented");
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

        virtual void preprocess_write(const void*& data, size_t& length) override {
			const std::string_view request(static_cast<std::string_view::const_pointer>(data), length);

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

            header = std::format(request_format, request, web_version, host, generated_key, socket_version);

            data = header.data();
            length = header.length();
        }

        virtual bool read_more() override {
			if (read_state != e_read_result::data_eof) {
				return false;
			}
            read_state = e_read_result::accept;
	        return true;
        }

        virtual e_read_result preprocess_read(const void* data, size_t& offset, size_t& length) override {
			// TODO: Check if length == 0
            const std::string_view buffer(static_cast<std::string_view::const_pointer>(data), length);

        	static auto&& split_headers = [](const std::string_view& in_value, char in_delimiter = '\n') {
                std::vector<std::string_view> out_values;
                for (const auto value : std::views::split(in_value, in_delimiter)) {
                    out_values.emplace_back(value.begin(), value.end());
                }
                return out_values;
            };

			switch (read_state) {
				case e_read_result::none: {
                    auto&& headers = split_headers(buffer);
                    for (auto&& header_view : headers) {
                        if (header_view.length() == 1 && header_view == "\r") {
                            read_state = e_read_result::accept;
                            return e_read_result::accept;
                        }
                    }
                    read_state = e_read_result::error;
					break;
				}
                case e_read_result::accept: {
					constexpr auto web_socket_header_bytes = 0x2;
					if (length < web_socket_header_bytes) {
                        return e_read_result::error;
					}

					constexpr auto EOF_BIT = 0x80;
					constexpr auto OP_CODE_BIT = 0x0F;
					constexpr auto MASK_BIT = 0x80;
					constexpr auto SOURCE_LEN_BIT = 0x7F;

                    constexpr auto OPCODE_TEXT = 0x1;

                    const std::uint8_t* data_ptr = static_cast<const std::uint8_t*>(data);

                    const std::uint8_t is_eof = data_ptr[0] & EOF_BIT;
                    const std::uint8_t op_code = data_ptr[0] & OP_CODE_BIT;
                    const std::uint8_t mask = data_ptr[1] & MASK_BIT;

                    if (op_code != OPCODE_TEXT)
                    {
                        return e_read_result::error;
                    }

                    static auto&& read_package_length = [&](size_t& out_package_length) {

                        out_package_length = data_ptr[1] & SOURCE_LEN_BIT;

                        std::uint8_t extra_length_bytes = 0;
                        const std::uint8_t package_length = data_ptr[1] & SOURCE_LEN_BIT;
                        if (package_length == 126u) {
                            extra_length_bytes = 0x2;
                        }
                        else if (package_length == 127u) {
                            extra_length_bytes = 0x8;
                        }

                        if (length < web_socket_header_bytes + extra_length_bytes) {
	                        return false;
                        }

                        if (extra_length_bytes > 0) {
                            const std::uint8_t* data_length_ptr = data_ptr + web_socket_header_bytes;

                            out_package_length = 0ull;
                            for (auto&& i = 0; i < extra_length_bytes; i++) {
                                out_package_length = (out_package_length << 0x8) + data_length_ptr[i];
                            }
                        }
                        else {
							out_package_length = package_length;
                        }

                        offset += extra_length_bytes;
                        return true;
                    };

                    offset = web_socket_header_bytes;

                    size_t package_length;
					if (!read_package_length(package_length))
					{
                        return e_read_result::error;
					}

                    if (package_length + 0x4 != length)
                    {
                        return e_read_result::error;
                    }

                    length = package_length;

					// TODO: Process large messages
					return is_eof ? e_read_result::data_eof : e_read_result::data;
				}

                case e_read_result::data:
				case e_read_result::data_eof:
					return e_read_result::data_eof;

                case e_read_result::error: break;
			}

        	return e_read_result::error;
        }

        std::string host;
        std::string header;

        // TODO: Might better to use not common enum, better to use inner states of websocket (connected, handshake, header, closed)
        e_read_result read_state = e_read_result::none;

        SOCKET m_socket{ INVALID_SOCKET };
    };
}

namespace hope::io {

    stream* create_stream(unsigned long long socket) {
        // return new win_stream(socket);
        return new win_websockets_stream(socket);
    }

}

#endif
