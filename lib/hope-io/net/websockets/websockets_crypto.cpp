#include <cassert>

#include "websockets.h"

#include <random>

#ifdef HOPE_IO_USE_OPENSSL
#include "openssl/bio.h"
#include "openssl/buffer.h"
#include "openssl/evp.h"
#endif

namespace {
	std::string random_bytes(const size_t bytes) {
		thread_local std::random_device rd;
		thread_local std::mt19937 generator(rd);

        std::string out_result;
        out_result.reserve(bytes);

        using namespace std::literals;
        constexpr auto values = "0123456789abcdefABCDEFGHIJKLMNOPQRSTUVEXYZ"sv;

        std::uniform_int_distribution<> distribution(0, values.length() - 1);

        for (size_t i = 0; i < bytes; i++) {
            out_result += values[distribution(generator)];
        }

        return out_result;
    }

	std::string base64_encode(const std::string& input) {
#ifdef HOPE_IO_USE_OPENSSL
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
#else
        assert(false && "hope-io/ OpenSSL is not available");
        return {};
#endif
    }
}

namespace hope::io::crypto {
	std::string base64_key_encode(size_t length) {
		return base64_encode(random_bytes(length));
	}
}