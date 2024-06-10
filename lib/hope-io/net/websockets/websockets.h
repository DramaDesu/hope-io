/*
 * Copyright (C) 2024 - All Rights Reserved
 */

#pragma once

#include <string>

namespace hope::io {
	std::string generate_handshake(const std::string& host, const std::string& uri);
}


namespace hope::io::crypto {
	std::string base64_key_encode(size_t length);
}