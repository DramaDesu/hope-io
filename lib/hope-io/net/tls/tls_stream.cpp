#include "tls_stream.h"

#ifdef HOPE_IO_USE_OPENSSL

namespace {

    class client_tls_stream final : public hope::io::base_tls_stream {
    public:
        using base_tls_stream::base_tls_stream;

        virtual void connect(std::string_view ip, std::size_t port) override {
            m_tcp_stream->connect(ip, port);
            auto* context_method = TLS_client_method();
            m_context = SSL_CTX_new(context_method);
            if (m_context == nullptr) {
                throw std::runtime_error("hope-io/client_tls_stream: cannot create context");
            }
            m_ssl = SSL_new(m_context);
            SSL_set_fd(m_ssl, (int32_t)m_tcp_stream->platform_socket());

            if (SSL_connect(m_ssl) <= 0) {
                throw std::runtime_error("hope-io/client_tls_stream: cannot establish connection");
            }
        }

        virtual void disconnect() override {
            base_tls_stream::disconnect();
            SSL_CTX_free(m_context);
        }

        virtual void write(const void* data, std::size_t length) override {

            m_tcp_stream->preprocess_write(data, length);

            std::size_t total = 0;
            // TODO:: do we need cycle here?
            do
            {
                const auto sent = SSL_write(m_ssl, (char*)data + total, length - total);
                if (sent <= 0)
                    throw std::runtime_error("hope-io/tls_stream: cannot write to socket");
                total += sent;
            } while (total < length);
        }

        virtual void read(void* data, std::size_t length) override {
            std::size_t total = 0;
            // TODO:: do we need cycle here?
            do
            {
                const auto received = SSL_read(m_ssl, (char*)data + total, length - total);
                if (received <= 0)
                    throw std::runtime_error("hope-io/tls_stream: cannot read from socket");

                total += received;
            } while (total < length);
        }

        virtual void stream_in(std::string& out_stream) override {
            constexpr static std::size_t BufferSize{ 1024 };
            char buffer[BufferSize];
            size_t bytes_read = 0;
            while ((bytes_read = SSL_read(m_ssl, buffer, BufferSize)) > 0) {
                // TODO: Check if zero bytes_read
                size_t offset = 0;
                switch (m_tcp_stream->preprocess_read(buffer, offset, bytes_read))
                {
					case e_read_result::none:
					case e_read_result::accept: continue;

                    case e_read_result::data: {
                        out_stream.append(buffer + offset, bytes_read);
						continue;
					}

                    case e_read_result::data_eof: {
                        out_stream.append(buffer + offset, bytes_read);
                        return;
                    }

					case e_read_result::error: return;
                }
            }
        }

        virtual bool read_more() override
        {
	        return m_tcp_stream->read_more();
        }
    };

}

namespace hope::io {

    stream* create_tls_stream(stream* tcp_stream) {
        return new client_tls_stream(tcp_stream);
    }

}

#else

    stream* create_tls_stream(stream* tcp_stream) {
        assert(false && "hope-io/ OpenSSL is not available");
        return nullptr;
    }

#endif