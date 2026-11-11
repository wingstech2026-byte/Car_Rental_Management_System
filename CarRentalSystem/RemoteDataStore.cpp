// =============================================================================
// RemoteDataStore.cpp
// =============================================================================
#include "RemoteDataStore.h"

#include "DataFileFormat.h"
#include "RentalExceptions.h"
#include "TextUtils.h"
#include "Win32Include.h"

#include <winhttp.h>

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace
{
    /// RAII: closes a WinHTTP handle automatically.
    struct InternetHandleCloser
    {
        void operator()(HINTERNET handle) const noexcept
        {
            if (handle != nullptr)
            {
                WinHttpCloseHandle(handle);
            }
        }
    };
    using InternetHandle = std::unique_ptr<std::remove_pointer_t<HINTERNET>, InternetHandleCloser>;

    std::string base64(const std::string& input)
    {
        static const char* const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string output;
        std::size_t index = 0;
        while (index + 2 < input.size())
        {
            const unsigned value = (static_cast<unsigned char>(input[index]) << 16) |
                                   (static_cast<unsigned char>(input[index + 1]) << 8) |
                                   static_cast<unsigned char>(input[index + 2]);
            output += alphabet[(value >> 18) & 63u];
            output += alphabet[(value >> 12) & 63u];
            output += alphabet[(value >> 6) & 63u];
            output += alphabet[value & 63u];
            index += 3;
        }
        const std::size_t rest = input.size() - index;
        if (rest > 0)
        {
            unsigned value = static_cast<unsigned>(static_cast<unsigned char>(input[index])) << 16;
            if (rest == 2)
            {
                value |= static_cast<unsigned>(static_cast<unsigned char>(input[index + 1])) << 8;
            }
            output += alphabet[(value >> 18) & 63u];
            output += alphabet[(value >> 12) & 63u];
            output += rest == 2 ? alphabet[(value >> 6) & 63u] : '=';
            output += '=';
        }
        return output;
    }

    std::wstring lastErrorText(const wchar_t* step)
    {
        return std::wstring(L"Could not reach the shared online database (") + step + L", error " +
               std::to_wstring(GetLastError()) + L"). Check the internet connection and the address in server.txt.";
    }

    /// Extracts "error" from a small JSON reply like {"error":"..."}.
    std::wstring errorMessage(const std::string& body)
    {
        const std::string key = "\"error\":\"";
        const std::size_t start = body.find(key);
        if (start == std::string::npos)
        {
            return TextUtils::fromUtf8Lenient(body);
        }
        const std::size_t end = body.find('"', start + key.size());
        return TextUtils::fromUtf8Lenient(body.substr(start + key.size(), end == std::string::npos ? std::string::npos
                                                                                                   : end - start - key.size()));
    }
}

RemoteDataStore::RemoteDataStore(std::wstring baseUrl)
    : m_baseUrl(TextUtils::trim(baseUrl))
{
    while (!m_baseUrl.empty() && m_baseUrl.back() == L'/')
    {
        m_baseUrl.pop_back();
    }
}

void RemoteDataStore::setCredentials(const std::wstring& username, const std::wstring& password)
{
    m_username = TextUtils::toLower(TextUtils::trim(username));
    m_password = password;
}

bool RemoteDataStore::wasEmpty() const noexcept
{
    return m_wasEmpty;
}

LoadResult RemoteDataStore::load()
{
    const Response response = send(L"GET", std::string());
    if (response.status != 200)
    {
        throwForStatus(response);
    }
    m_version = response.version.empty() ? L"0" : response.version;
    m_wasEmpty = response.body.empty();

    std::wstring bundle;
    if (!TextUtils::tryFromUtf8(response.body, bundle))
    {
        bundle = TextUtils::fromUtf8Lenient(response.body);
    }
    return DataFileFormat::parseBundle(bundle);
}

void RemoteDataStore::save(const AgencyData& data)
{
    const Response response = send(L"PUT", TextUtils::toUtf8(DataFileFormat::formatBundle(data)));
    if (response.status != 200)
    {
        throwForStatus(response);
    }
    if (!response.version.empty())
    {
        m_version = response.version;
    }
    m_wasEmpty = false;
}

std::wstring RemoteDataStore::describeLocation() const
{
    return L"shared online database at " + m_baseUrl;
}

void RemoteDataStore::throwForStatus(const Response& response) const
{
    switch (response.status)
    {
    case 401:
        throw AuthenticationException();
    case 403:
        throw RentalException(errorMessage(response.body));
    case 409:
        throw DataConflictException();
    case 503:
        throw DataStoreException(L"The shared online database is not set up on the web site yet "
                                 L"(connect an Upstash Redis database in the Vercel project).");
    default:
        throw DataStoreException(L"The shared online database answered with error " + std::to_wstring(response.status) +
                                 L": " + errorMessage(response.body));
    }
}

RemoteDataStore::Response RemoteDataStore::send(const wchar_t* method, const std::string& body) const
{
    const std::wstring url = m_baseUrl + L"/api/data";

    // Split the address into host, port and path.
    std::vector<wchar_t> host(256), path(2048);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host.data();
    parts.dwHostNameLength = static_cast<DWORD>(host.size());
    parts.lpszUrlPath = path.data();
    parts.dwUrlPathLength = static_cast<DWORD>(path.size());
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts))
    {
        throw DataStoreException(L"The address in server.txt is not a valid web address: " + m_baseUrl);
    }

    const InternetHandle session(WinHttpOpen(L"CarRentalSystem/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session)
    {
        throw DataStoreException(lastErrorText(L"open"));
    }
    WinHttpSetTimeouts(session.get(), 10000, 10000, 20000, 20000);

    const InternetHandle connection(WinHttpConnect(session.get(), host.data(), parts.nPort, 0));
    if (!connection)
    {
        throw DataStoreException(lastErrorText(L"connect"));
    }

    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    const InternetHandle request(WinHttpOpenRequest(connection.get(), method, path.data(), nullptr,
                                                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request)
    {
        throw DataStoreException(lastErrorText(L"request"));
    }

    const std::string credentials = TextUtils::toUtf8(m_username + L":" + m_password);
    const std::wstring headers = L"Authorization: Basic " + TextUtils::fromUtf8Lenient(base64(credentials)) + L"\r\n" +
                                 L"Content-Type: text/plain; charset=utf-8\r\n" +
                                 L"X-CRMS-Version: " + m_version + L"\r\n";

    void* const bodyData = body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data());
    const DWORD bodySize = static_cast<DWORD>(body.size());
    if (!WinHttpSendRequest(request.get(), headers.c_str(), static_cast<DWORD>(headers.size()), bodyData, bodySize,
                            bodySize, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
    {
        throw DataStoreException(lastErrorText(L"send"));
    }

    Response response;
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    response.status = status;

    wchar_t version[32] = {};
    DWORD versionSize = sizeof(version);
    if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CUSTOM, L"X-CRMS-Version", version, &versionSize,
                            WINHTTP_NO_HEADER_INDEX))
    {
        response.version = version;
    }

    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available))
        {
            throw DataStoreException(lastErrorText(L"receive"));
        }
        if (available == 0)
        {
            break;
        }
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), chunk.data(), available, &read))
        {
            throw DataStoreException(lastErrorText(L"read"));
        }
        response.body.append(chunk.data(), read);
    }
    return response;
}
