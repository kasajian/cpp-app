#pragma once

#include <string_view>

// Returns a read-only view over the contents of data/sample.json as they were
// embedded into this executable at link time via llvm-objcopy.
// The bytes are NOT null-terminated; always use .size() to determine the length.
std::string_view get_embedded_sample_json();
