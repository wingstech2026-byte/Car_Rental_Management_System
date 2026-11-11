// =============================================================================
// InMemoryDataStore.cpp
// =============================================================================
#include "InMemoryDataStore.h"

LoadResult InMemoryDataStore::load()
{
    LoadResult result;
    result.vehicles = m_vehicles;
    result.revenue = m_revenue;
    result.history = m_history;
    return result;
}

void InMemoryDataStore::save(const std::vector<Vehicle>& vehicles, const Money& revenue,
                             const std::vector<RentalRecord>& history)
{
    m_vehicles = vehicles;
    m_revenue = revenue;
    m_history = history;
}

std::wstring InMemoryDataStore::describeLocation() const
{
    return L"(memory)";
}
