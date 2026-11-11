// =============================================================================
// DataFileFormat.h
// -----------------------------------------------------------------------------
// Reading and writing the text format of the data, shared by every data store
// (single responsibility: the stores decide WHERE data lives, this class
// decides HOW it is written). Four files, UTF-8, one record per line, fields
// separated by '|':
//
//   vehicles.txt  plate|model|dailyRate|status|renterName|phone|agreedRate|startDate
//   revenue.txt   3250.00
//   history.txt   number|returnedOn|plate|model|renter|phone|agreedRate|days|charge|startedOn
//   users.txt     username|role|salt|passwordHash
//
// (Older files without the last field of vehicles.txt / history.txt are still
// accepted.) Every line is validated with the same rules as manual entry by
// re-using the constructors of the business classes; invalid lines are
// skipped and reported as warnings.
//
// A "bundle" puts all four files into one text, used for the shared online
// database and the web version:
//
//   CRMS-BUNDLE 1
//   --- vehicles.txt
//   ...lines...
//   --- revenue.txt
//   ...
// =============================================================================
#pragma once

#include "IDataStore.h"

#include <cstddef>
#include <string>
#include <vector>

class DataFileFormat
{
public:
    static constexpr const wchar_t* VehiclesFile = L"vehicles.txt";
    static constexpr const wchar_t* RevenueFile = L"revenue.txt";
    static constexpr const wchar_t* HistoryFile = L"history.txt";
    static constexpr const wchar_t* UsersFile = L"users.txt";
    static constexpr const wchar_t* BundleHeader = L"CRMS-BUNDLE 1";

    // ---- Reading (each adds to 'result' and reports bad lines as warnings) ----
    static void parseVehicles(const std::wstring& text, LoadResult& result);
    static void parseRevenue(const std::wstring& text, LoadResult& result);
    static void parseHistory(const std::wstring& text, LoadResult& result);
    static void parseUsers(const std::wstring& text, LoadResult& result);

    // ---- Writing --------------------------------------------------------------
    static std::wstring formatVehicles(const std::vector<Vehicle>& vehicles);
    static std::wstring formatRevenue(const Money& revenue);
    static std::wstring formatHistory(const std::vector<RentalRecord>& history);
    static std::wstring formatUsers(const std::vector<User>& users);

    // ---- Bundle (all four files in one text) -------------------------------------
    static std::wstring formatBundle(const AgencyData& data);

    /// An empty text gives empty data. @throws DataStoreException if the text is not a bundle.
    static LoadResult parseBundle(const std::wstring& text);

    /// Splits a text into lines (handles "\r\n" and "\n").
    static std::vector<std::wstring> splitLines(const std::wstring& text);

private:
    static Vehicle parseVehicleLine(const std::wstring& line);
    static RentalRecord parseHistoryLine(const std::wstring& line);
    static User parseUserLine(const std::wstring& line);
};
