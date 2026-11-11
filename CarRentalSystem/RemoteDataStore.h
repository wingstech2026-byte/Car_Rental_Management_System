// =============================================================================
// RemoteDataStore.h
// -----------------------------------------------------------------------------
// POLYMORPHISM: an IDataStore that keeps all data in the SHARED ONLINE
// DATABASE, so several users on different computers work with the same data.
// It talks to the web service /api/data of the project's Vercel site over
// HTTPS (WinHTTP, part of Windows - no third-party library) and transfers the
// data as one text bundle (DataFileFormat).
//
// RentalAgency does not know anything about the network: it only calls
// load() and save() through the IDataStore interface, exactly as for the
// text files.
//
// Every request logs in with the user name and password of a system account.
// Saving uses optimistic concurrency: the store remembers the version it
// loaded, and if somebody else saved in the meantime the server refuses the
// save and DataConflictException is thrown (the user reloads and repeats).
//
// The program uses this store when a file "server.txt" next to the .exe
// contains the web address of the site, e.g. https://my-car-rental.vercel.app
// =============================================================================
#pragma once

#include "IDataStore.h"

#include <string>

class RemoteDataStore final : public IDataStore
{
public:
    /// @param baseUrl e.g. "https://my-car-rental.vercel.app"
    explicit RemoteDataStore(std::wstring baseUrl);

    /// The account used to log in to the web service.
    void setCredentials(const std::wstring& username, const std::wstring& password);

    /// True if the last load() found an empty database (nothing saved yet).
    bool wasEmpty() const noexcept;

    // ---- IDataStore -----------------------------------------------------------
    LoadResult load() override;
    void save(const AgencyData& data) override;
    std::wstring describeLocation() const override;

private:
    struct Response
    {
        unsigned long status = 0;
        std::string body;
        std::wstring version;
    };

    /// Sends one HTTPS request to <baseUrl>/api/data. @throws DataStoreException on network errors.
    Response send(const wchar_t* method, const std::string& body) const;

    /// Turns an error response into the right exception.
    [[noreturn]] void throwForStatus(const Response& response) const;

    std::wstring m_baseUrl;
    std::wstring m_username;
    std::wstring m_password;
    std::wstring m_version = L"0";
    bool m_wasEmpty = false;
};
