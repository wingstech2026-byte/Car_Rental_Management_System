// =============================================================================
// TextUtils.cpp - implementation of the text helper functions.
// -----------------------------------------------------------------------------
// On Windows the Win32 API is used for UTF-8 conversion and case-insensitive
// comparison. On other platforms (the WebAssembly build of the business logic
// used by the web demo) portable standard C++ code is used instead, so the
// business classes compile everywhere without any change.
// =============================================================================
#include "TextUtils.h"

#ifdef _WIN32
#include "Win32Include.h"
#endif

#include <cwctype>

namespace TextUtils
{
    bool isAsciiDigit(wchar_t character) noexcept
    {
        return character >= L'0' && character <= L'9';
    }

    bool isAsciiLetter(wchar_t character) noexcept
    {
        return (character >= L'A' && character <= L'Z') || (character >= L'a' && character <= L'z');
    }

    std::wstring trim(const std::wstring& text)
    {
        const wchar_t* const whitespace = L" \t\r\n\f\v";
        const std::wstring::size_type first = text.find_first_not_of(whitespace);
        if (first == std::wstring::npos)
        {
            return std::wstring();
        }
        const std::wstring::size_type last = text.find_last_not_of(whitespace);
        return text.substr(first, last - first + 1);
    }

    std::wstring toUpper(std::wstring text)
    {
        for (wchar_t& character : text)
        {
            character = static_cast<wchar_t>(std::towupper(static_cast<std::wint_t>(character)));
        }
        return text;
    }

    std::wstring toLower(std::wstring text)
    {
        for (wchar_t& character : text)
        {
            character = static_cast<wchar_t>(std::towlower(static_cast<std::wint_t>(character)));
        }
        return text;
    }

    bool containsIgnoreCase(const std::wstring& text, const std::wstring& fragment)
    {
        if (fragment.empty())
        {
            return true;
        }
        return toLower(text).find(toLower(fragment)) != std::wstring::npos;
    }

    bool equalsIgnoreCase(const std::wstring& left, const std::wstring& right)
    {
        return compareIgnoreCase(left, right) == 0;
    }

    std::vector<std::wstring> split(const std::wstring& text, wchar_t separator)
    {
        std::vector<std::wstring> parts;
        std::wstring::size_type start = 0;
        for (;;)
        {
            const std::wstring::size_type position = text.find(separator, start);
            if (position == std::wstring::npos)
            {
                parts.push_back(text.substr(start));
                break;
            }
            parts.push_back(text.substr(start, position - start));
            start = position + 1;
        }
        return parts;
    }

#ifdef _WIN32
    // ---- Windows implementation (Win32 API) ---------------------------------

    int compareIgnoreCase(const std::wstring& left, const std::wstring& right)
    {
        // CompareStringOrdinal returns CSTR_LESS_THAN (1), CSTR_EQUAL (2) or
        // CSTR_GREATER_THAN (3); 0 means failure.
        const int result = CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()),
                                                right.c_str(), static_cast<int>(right.size()), TRUE);
        if (result == 0)
        {
            return 0;
        }
        return result - CSTR_EQUAL;
    }

    std::string toUtf8(const std::wstring& text)
    {
        if (text.empty())
        {
            return std::string();
        }
        const int inputLength = static_cast<int>(text.size());
        const int needed = WideCharToMultiByte(CP_UTF8, 0, text.data(), inputLength, nullptr, 0, nullptr, nullptr);
        if (needed <= 0)
        {
            return std::string();
        }
        std::string result(static_cast<std::size_t>(needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), inputLength, result.data(), needed, nullptr, nullptr);
        return result;
    }

    bool tryFromUtf8(const std::string& bytes, std::wstring& result)
    {
        result.clear();
        if (bytes.empty())
        {
            return true;
        }
        const int inputLength = static_cast<int>(bytes.size());
        const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), inputLength, nullptr, 0);
        if (needed <= 0)
        {
            return false;
        }
        result.assign(static_cast<std::size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), inputLength, result.data(), needed);
        return true;
    }

    std::wstring fromUtf8Lenient(const std::string& bytes)
    {
        if (bytes.empty())
        {
            return std::wstring();
        }
        const int inputLength = static_cast<int>(bytes.size());
        const int needed = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), inputLength, nullptr, 0);
        if (needed <= 0)
        {
            return std::wstring();
        }
        std::wstring result(static_cast<std::size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), inputLength, result.data(), needed);
        return result;
    }

#else
    // ---- Portable implementation (used by the WebAssembly build) ------------

    namespace
    {
        constexpr char32_t ReplacementCharacter = 0xFFFD;

        void appendUtf8(std::string& out, char32_t codePoint)
        {
            if (codePoint < 0x80)
            {
                out += static_cast<char>(codePoint);
            }
            else if (codePoint < 0x800)
            {
                out += static_cast<char>(0xC0 | (codePoint >> 6));
                out += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
            else if (codePoint < 0x10000)
            {
                out += static_cast<char>(0xE0 | (codePoint >> 12));
                out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xF0 | (codePoint >> 18));
                out += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
        }

        void appendWide(std::wstring& out, char32_t codePoint)
        {
            if constexpr (sizeof(wchar_t) == 2)
            {
                if (codePoint >= 0x10000)
                {
                    const char32_t value = codePoint - 0x10000;
                    out += static_cast<wchar_t>(0xD800 + (value >> 10));
                    out += static_cast<wchar_t>(0xDC00 + (value & 0x3FF));
                    return;
                }
            }
            out += static_cast<wchar_t>(codePoint);
        }

        /// Decodes UTF-8. Invalid sequences make it fail (strict) or become U+FFFD (lenient).
        bool decodeUtf8(const std::string& bytes, std::wstring& result, bool lenient)
        {
            result.clear();
            std::size_t index = 0;
            while (index < bytes.size())
            {
                const unsigned char lead = static_cast<unsigned char>(bytes[index]);
                char32_t codePoint = 0;
                std::size_t length = 0;
                char32_t minimum = 0;
                if (lead < 0x80) { codePoint = lead; length = 1; minimum = 0; }
                else if ((lead & 0xE0) == 0xC0) { codePoint = lead & 0x1F; length = 2; minimum = 0x80; }
                else if ((lead & 0xF0) == 0xE0) { codePoint = lead & 0x0F; length = 3; minimum = 0x800; }
                else if ((lead & 0xF8) == 0xF0) { codePoint = lead & 0x07; length = 4; minimum = 0x10000; }

                bool valid = length != 0 && index + length <= bytes.size();
                for (std::size_t offset = 1; valid && offset < length; ++offset)
                {
                    const unsigned char next = static_cast<unsigned char>(bytes[index + offset]);
                    if ((next & 0xC0) != 0x80)
                    {
                        valid = false;
                    }
                    else
                    {
                        codePoint = (codePoint << 6) | (next & 0x3F);
                    }
                }
                if (valid && (codePoint < minimum || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)))
                {
                    valid = false;
                }

                if (!valid)
                {
                    if (!lenient)
                    {
                        result.clear();
                        return false;
                    }
                    appendWide(result, ReplacementCharacter);
                    ++index;
                    continue;
                }
                appendWide(result, codePoint);
                index += length;
            }
            return true;
        }
    }

    int compareIgnoreCase(const std::wstring& left, const std::wstring& right)
    {
        const int result = toLower(left).compare(toLower(right));
        return (result < 0) ? -1 : (result > 0 ? 1 : 0);
    }

    std::string toUtf8(const std::wstring& text)
    {
        std::string out;
        out.reserve(text.size());
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            char32_t codePoint = static_cast<char32_t>(text[index]);
            if constexpr (sizeof(wchar_t) == 2)
            {
                if (codePoint >= 0xD800 && codePoint <= 0xDBFF && index + 1 < text.size())
                {
                    const char32_t low = static_cast<char32_t>(text[index + 1]);
                    if (low >= 0xDC00 && low <= 0xDFFF)
                    {
                        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                        ++index;
                    }
                }
            }
            appendUtf8(out, codePoint);
        }
        return out;
    }

    bool tryFromUtf8(const std::string& bytes, std::wstring& result)
    {
        return decodeUtf8(bytes, result, false);
    }

    std::wstring fromUtf8Lenient(const std::string& bytes)
    {
        std::wstring result;
        decodeUtf8(bytes, result, true);
        return result;
    }
#endif
}
