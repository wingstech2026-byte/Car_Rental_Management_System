// =============================================================================
// RentalAgency.cpp
// =============================================================================
#include "RentalAgency.h"

#include "IDataStore.h"
#include "RentalExceptions.h"
#include "TextUtils.h"

#include <algorithm>
#include <utility>

// ---- Use cases ----------------------------------------------------------------

const Vehicle& RentalAgency::addVehicle(const std::wstring& plate, const std::wstring& model,
                                        const Money& dailyRate)
{
    Vehicle vehicle(plate, model, dailyRate); // validates and normalises (plate -> upper case)
    const std::wstring key = vehicle.getPlate();
    if (m_fleet.find(key) != m_fleet.end())
    {
        throw DuplicatePlateException(key);
    }
    const auto position = m_fleet.emplace(key, std::move(vehicle)).first;
    m_modified = true;
    return position->second;
}

void RentalAgency::editVehicle(const std::wstring& plate, const std::wstring& model, const Money& dailyRate)
{
    findMutableVehicle(plate).edit(model, dailyRate); // Vehicle refuses if it is rented
    m_modified = true;
}

void RentalAgency::removeVehicle(const std::wstring& plate)
{
    const Vehicle& vehicle = findVehicle(plate);
    if (!vehicle.isAvailable())
    {
        throw VehicleNotAvailableException(vehicle.getPlate(), L"removed");
    }
    const std::wstring key = vehicle.getPlate(); // copy: 'vehicle' is destroyed by erase()
    m_fleet.erase(key);
    m_modified = true;
}

std::vector<Vehicle> RentalAgency::searchFleet(const std::wstring& query, StatusFilter filter) const
{
    const std::wstring trimmedQuery = TextUtils::trim(query);
    std::vector<Vehicle> result;
    for (const auto& entry : m_fleet)
    {
        const Vehicle& vehicle = entry.second;
        const bool statusMatches = filter == StatusFilter::All ||
                                   (filter == StatusFilter::Available && vehicle.isAvailable()) ||
                                   (filter == StatusFilter::Rented && vehicle.isRented());
        const bool textMatches = trimmedQuery.empty() ||
                                 TextUtils::containsIgnoreCase(vehicle.getPlate(), trimmedQuery) ||
                                 TextUtils::containsIgnoreCase(vehicle.getModel(), trimmedQuery);
        if (statusMatches && textMatches)
        {
            result.push_back(vehicle);
        }
    }
    return result;
}

std::vector<Vehicle> RentalAgency::searchAvailableByModel(const std::wstring& query) const
{
    const std::wstring trimmedQuery = TextUtils::trim(query);
    std::vector<Vehicle> result;
    for (const auto& entry : m_fleet)
    {
        const Vehicle& vehicle = entry.second;
        if (vehicle.isAvailable() &&
            (trimmedQuery.empty() || TextUtils::containsIgnoreCase(vehicle.getModel(), trimmedQuery)))
        {
            result.push_back(vehicle);
        }
    }
    return result;
}

void RentalAgency::rentVehicle(const std::wstring& plate, const Renter& renter, const Money& agreedDailyRate)
{
    findMutableVehicle(plate).rent(renter, agreedDailyRate); // Vehicle refuses if already rented
    m_modified = true;
}

Receipt RentalAgency::returnVehicle(const std::wstring& plate, int days)
{
    Vehicle& vehicle = findMutableVehicle(plate);
    Receipt receipt = vehicle.returnVehicle(days); // Vehicle refuses if not rented / bad days
    m_revenue += receipt.getTotalCharge();
    m_history.emplace_back(nextReceiptNumber(), RentalRecord::currentTimestamp(), receipt); // archive it
    m_modified = true;
    return receipt;
}

std::vector<RentalRecord> RentalAgency::getHistory() const
{
    return m_history;
}

std::vector<RentalRecord> RentalAgency::searchHistory(const std::wstring& query) const
{
    std::vector<RentalRecord> result;
    for (const RentalRecord& record : m_history)
    {
        if (record.matches(query))
        {
            result.push_back(record);
        }
    }
    return result;
}

const RentalRecord& RentalAgency::getLastRecord() const
{
    if (m_history.empty())
    {
        throw RentalException(L"The rental history is empty.");
    }
    return m_history.back();
}

Money RentalAgency::totalCharged(const std::vector<RentalRecord>& records)
{
    Money total;
    for (const RentalRecord& record : records)
    {
        total += record.getReceipt().getTotalCharge();
    }
    return total;
}

int RentalAgency::nextReceiptNumber() const noexcept
{
    int highest = 0;
    for (const RentalRecord& record : m_history)
    {
        highest = std::max(highest, record.getNumber());
    }
    return highest + 1;
}

std::vector<Vehicle> RentalAgency::getRentedVehicles() const
{
    std::vector<Vehicle> result;
    for (const auto& entry : m_fleet)
    {
        if (entry.second.isRented())
        {
            result.push_back(entry.second);
        }
    }
    return result;
}

Money RentalAgency::getRevenue() const noexcept
{
    return m_revenue;
}

// ---- Queries --------------------------------------------------------------------

std::vector<Vehicle> RentalAgency::getAllVehicles() const
{
    std::vector<Vehicle> result;
    result.reserve(m_fleet.size());
    for (const auto& entry : m_fleet)
    {
        result.push_back(entry.second);
    }
    return result;
}

const Vehicle& RentalAgency::findVehicle(const std::wstring& plate) const
{
    const auto position = m_fleet.find(makeKey(plate));
    if (position == m_fleet.end())
    {
        throw VehicleNotFoundException(TextUtils::trim(plate));
    }
    return position->second;
}

const Vehicle& RentalAgency::findEditableVehicle(const std::wstring& plate) const
{
    const Vehicle& vehicle = findVehicle(plate);
    if (!vehicle.isAvailable())
    {
        throw VehicleNotAvailableException(vehicle.getPlate(), L"edited");
    }
    return vehicle;
}

const Vehicle& RentalAgency::findRentableVehicle(const std::wstring& plate) const
{
    const Vehicle& vehicle = findVehicle(plate);
    if (!vehicle.isAvailable())
    {
        throw VehicleNotAvailableException(vehicle.getPlate(), L"rented again");
    }
    return vehicle;
}

const Vehicle& RentalAgency::findRentedVehicle(const std::wstring& plate) const
{
    const Vehicle& vehicle = findVehicle(plate);
    if (!vehicle.isRented())
    {
        throw VehicleNotRentedException(vehicle.getPlate());
    }
    return vehicle;
}

std::size_t RentalAgency::getVehicleCount() const noexcept
{
    return m_fleet.size();
}

std::size_t RentalAgency::getAvailableCount() const
{
    return static_cast<std::size_t>(std::count_if(m_fleet.begin(), m_fleet.end(),
        [](const auto& entry) { return entry.second.isAvailable(); }));
}

std::size_t RentalAgency::getRentedCount() const
{
    return m_fleet.size() - getAvailableCount();
}

bool RentalAgency::hasUnsavedChanges() const noexcept
{
    return m_modified;
}

// ---- Persistence ----------------------------------------------------------------

std::vector<std::wstring> RentalAgency::loadFrom(IDataStore& store)
{
    LoadResult result = store.load(); // virtual call -> TextFileDataStore::load(), InMemoryDataStore::load(), ...

    // Build the new fleet first and only then replace the old one (strong exception guarantee).
    std::map<std::wstring, Vehicle> fleet;
    for (Vehicle& vehicle : result.vehicles)
    {
        const std::wstring key = vehicle.getPlate();
        if (!fleet.try_emplace(key, std::move(vehicle)).second)
        {
            result.warnings.push_back(L"Duplicate license plate " + key + L" skipped.");
        }
    }

    m_fleet = std::move(fleet);
    m_revenue = result.revenue;
    m_history = std::move(result.history);
    m_modified = false;
    return result.warnings;
}

void RentalAgency::saveTo(IDataStore& store)
{
    store.save(getAllVehicles(), m_revenue, m_history); // virtual call
    m_modified = false;
}

// ---- Private helpers ------------------------------------------------------------

Vehicle& RentalAgency::findMutableVehicle(const std::wstring& plate)
{
    const auto position = m_fleet.find(makeKey(plate));
    if (position == m_fleet.end())
    {
        throw VehicleNotFoundException(TextUtils::trim(plate));
    }
    return position->second;
}

std::wstring RentalAgency::makeKey(const std::wstring& plate)
{
    return TextUtils::toUpper(TextUtils::trim(plate));
}
