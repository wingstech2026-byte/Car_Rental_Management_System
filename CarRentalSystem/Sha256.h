// =============================================================================
// Sha256.h
// -----------------------------------------------------------------------------
// SHA-256 hash function (FIPS 180-4), implemented in standard C++ so that it
// works in the desktop program and in the WebAssembly build without any
// library. Used to store user passwords as salted hashes instead of plain text.
// =============================================================================
#pragma once

#include <string>

namespace Sha256
{
    /// SHA-256 of the given bytes as 64 lower-case hexadecimal characters.
    std::string hashHex(const std::string& bytes);
}
