// =============================================================================
// InMemoryDataStore.cpp
// =============================================================================
#include "InMemoryDataStore.h"

LoadResult InMemoryDataStore::load()
{
    LoadResult result;
    result.vehicles = m_vehicles;
    result.revenue = m_revenue;
    return result;
}

void InMemoryDataStore::save(const std::vector<Vehicle>& vehicles, const Money& revenue)
{
    m_vehicles = vehicles;
    m_revenue = revenue;
}

std::wstring InMemoryDataStore::describeLocation() const
{
    return L"(memory)";
}
