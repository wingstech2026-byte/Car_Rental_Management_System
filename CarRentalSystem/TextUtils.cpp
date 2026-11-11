// =============================================================================
// TextUtils.cpp - implementation of the text helper functions.
// =============================================================================
#include "TextUtils.h"

#include "Win32Include.h"

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
            character = static_cast<wchar_t>(std::towupper(character));
        }
        return text;
    }

    std::wstring toLower(std::wstring text)
    {
        for (wchar_t& character : text)
        {
            character = static_cast<wchar_t>(std::towlower(character));
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
}
