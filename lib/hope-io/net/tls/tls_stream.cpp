#include "tls_stream.h"

#ifdef HOPE_IO_USE_OPENSSL

namespace {

    class client_tls_stream : public hope::io::base_tls_stream {
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
            std::size_t total = 0;
            // TODO:: do we need cycle here?
            do {
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
            size_t bytes_read;
            while ((bytes_read = SSL_read(m_ssl, buffer, BufferSize)) > 0) {
                out_stream.append(buffer, bytes_read);
            }
        }
    };

    class client_web_sockets_tls_stream final : public client_tls_stream {
    public:
	    explicit client_web_sockets_tls_stream()
	    {
            m_tcp_stream = hope::io::create_stream([&](void* buffer, std::size_t size)
            {
				return static_cast<size_t>(SSL_read(m_ssl, buffer, size));
            }, [&](const void* data, std::size_t size)
            {
            	SSL_write(m_ssl, data, size);
            });
	    }

        virtual void write(const void* data, std::size_t length) override {
            m_tcp_stream->write(data, length);
        }

        virtual void read(void* data, std::size_t length) override {
            assert(false && "client_web_sockets_tls_stream::read doesn't support");
	    }

        virtual void stream_in(std::string& out_stream) override {
            m_tcp_stream->stream_in(out_stream);
        }
    };

}

namespace hope::io {

    stream* create_tls_stream(stream* tcp_stream) {
        return new client_tls_stream(tcp_stream);
    }

    stream* create_tls_stream(e_stream_t tcp_stream_type) {
	    switch (tcp_stream_type)
	    {
			case e_stream_t::ordinary: return create_tls_stream();
		    case e_stream_t::websockets: return new client_web_sockets_tls_stream();
	    }

        return create_tls_stream();
    }

}

#else

    stream* create_tls_stream(stream* tcp_stream) {
        assert(false && "hope-io/ OpenSSL is not available");
        return nullptr;
    }

    stream* create_tls_stream(e_stream_t tcp_stream_type) {
        assert(false && "hope-io/ OpenSSL is not available");
        return nullptr;
    }

#endif