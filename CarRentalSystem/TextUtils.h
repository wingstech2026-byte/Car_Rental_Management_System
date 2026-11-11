// =============================================================================
// TextUtils.h
// -----------------------------------------------------------------------------
// Small, stateless helper functions for text handling (trimming, case-
// insensitive comparison, splitting, UTF-8 conversion).
//
// They are grouped in a namespace instead of a class because they have no
// state and no invariants to protect - a class with only static methods would
// add nothing but noise.
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace TextUtils
{
    /// True for the ASCII characters '0'..'9' only.
    bool isAsciiDigit(wchar_t character) noexcept;

    /// True for the ASCII characters 'A'..'Z' and 'a'..'z' only.
    bool isAsciiLetter(wchar_t character) noexcept;

    /// Returns the text without leading/trailing whitespace.
    std::wstring trim(const std::wstring& text);

    /// Returns an upper-case / lower-case copy of the text.
    std::wstring toUpper(std::wstring text);
    std::wstring toLower(std::wstring text);

    /// Case-insensitive "contains" test (used by the model search).
    bool containsIgnoreCase(const std::wstring& text, const std::wstring& fragment);

    /// Case-insensitive equality test.
    bool equalsIgnoreCase(const std::wstring& left, const std::wstring& right);

    /// Case-insensitive three-way comparison: <0, 0 or >0 (used for sorting).
    int compareIgnoreCase(const std::wstring& left, const std::wstring& right);

    /// Splits the text at every separator. "a||b" gives {"a", "", "b"}.
    std::vector<std::wstring> split(const std::wstring& text, wchar_t separator);

    /// Converts UTF-16 (std::wstring) to UTF-8 bytes (std::string).
    std::string toUtf8(const std::wstring& text);

    /// Converts UTF-8 bytes to UTF-16. Returns false if the bytes are not valid UTF-8.
    bool tryFromUtf8(const std::string& bytes, std::wstring& result);

    /// Converts UTF-8 bytes to UTF-16, replacing invalid sequences instead of failing.
    std::wstring fromUtf8Lenient(const std::string& bytes);
}
