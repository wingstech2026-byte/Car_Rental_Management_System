// =============================================================================
// InMemoryDataStore.h
// -----------------------------------------------------------------------------
// POLYMORPHISM: a second implementation of IDataStore that simply keeps the
// data in memory. The built-in self-test saves a RentalAgency into it and
// loads it back - through an IDataStore& reference - to prove that the agency
// works with ANY data store, not just text files.
// =============================================================================
#pragma once

#include "IDataStore.h"

#include <string>
#include <vector>

class InMemoryDataStore final : public IDataStore
{
public:
    LoadResult load() override;
    void save(const std::vector<Vehicle>& vehicles, const Money& revenue,
              const std::vector<RentalRecord>& history) override;
    std::wstring describeLocation() const override;

private:
    std::vector<Vehicle> m_vehicles;
    Money m_revenue;
    std::vector<RentalRecord> m_history;
};
