// =============================================================================
// TextFileDataStore.h
// -----------------------------------------------------------------------------
// INHERITANCE: concrete implementation of the IDataStore interface that uses
// two UTF-8 text files in a folder (normally the folder of the .exe):
//
//   vehicles.txt - one line per vehicle:
//                  plate|model|dailyRate|status|renterName|phone|agreedRate
//                  (the three rental fields are empty for Available vehicles)
//   revenue.txt  - the cumulative revenue, e.g. 3250.00
//   history.txt  - one line per completed rental:
//                  number|returnedOn|plate|model|renterName|phone|agreedRate|days|charge
//
// Every line is validated with the same rules as manual entry (it re-uses the
// Vehicle/Renter/Rental constructors). Bad lines are skipped and reported.
// Files are written to a temporary file first and then renamed, so a crash
// while saving cannot leave a half-written data file behind.
// =============================================================================
#pragma once

#include "IDataStore.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

class TextFileDataStore final : public IDataStore
{
public:
    static constexpr const wchar_t* VehiclesFileName = L"vehicles.txt";
    static constexpr const wchar_t* RevenueFileName = L"revenue.txt";
    static constexpr const wchar_t* HistoryFileName = L"history.txt";
    static constexpr std::size_t FieldCount = 7;
    static constexpr std::size_t HistoryFieldCount = 9;

    explicit TextFileDataStore(std::filesystem::path directory);

    // ---- IDataStore implementation (override = checked by the compiler) ----
    LoadResult load() override;
    void save(const std::vector<Vehicle>& vehicles, const Money& revenue,
              const std::vector<RentalRecord>& history) override;
    std::wstring describeLocation() const override;

private:
    void loadVehicles(LoadResult& result) const;
    void loadRevenue(LoadResult& result) const;
    void loadHistory(LoadResult& result) const;

    /// Converts one line into a Vehicle. @throws RentalException if invalid.
    static Vehicle parseVehicleLine(const std::wstring& line);

    /// Converts a Vehicle into one line of vehicles.txt (without line break).
    static std::wstring formatVehicleLine(const Vehicle& vehicle);

    /// Converts one line of history.txt into a RentalRecord. @throws RentalException if invalid.
    static RentalRecord parseHistoryLine(const std::wstring& line);

    /// Converts a RentalRecord into one line of history.txt (without line break).
    static std::wstring formatHistoryLine(const RentalRecord& record);

    /// Reads a file and returns its lines as raw UTF-8 bytes (BOM and '\r' removed).
    static std::vector<std::string> readLines(const std::filesystem::path& path);

    /// Writes content to "<target>.tmp" and then replaces the target with it.
    static void writeFileAtomically(const std::filesystem::path& target, const std::string& content);

    std::filesystem::path m_directory;
};
