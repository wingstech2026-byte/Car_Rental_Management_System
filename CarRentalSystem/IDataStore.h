// =============================================================================
// IDataStore.h
// -----------------------------------------------------------------------------
// ABSTRACTION + INHERITANCE + POLYMORPHISM
//
// IDataStore is an abstract base class (an "interface"): it says WHAT a data
// store can do - load() and save() - but not HOW. It has only pure virtual
// functions, so it cannot be instantiated.
//
// Concrete classes inherit from it and provide the "how":
//   * TextFileDataStore  - stores data in vehicles.txt / revenue.txt
//   * InMemoryDataStore  - keeps data in memory (used by the self-test)
//
// RentalAgency::loadFrom(IDataStore&) and RentalAgency::saveTo(IDataStore&)
// only know this interface. The call store.load() is resolved at RUN TIME to
// the implementation of the actual object (dynamic dispatch through the
// virtual table) - that is polymorphism. A database or JSON store could be
// added later without changing a single line of RentalAgency.
// =============================================================================
#pragma once

#include "Money.h"
#include "RentalRecord.h"
#include "Vehicle.h"

#include <string>
#include <vector>

/// Everything a data store returns when loading.
struct LoadResult
{
    std::vector<Vehicle> vehicles;       ///< Vehicles that passed validation.
    Money revenue;                       ///< Cumulative rental revenue.
    std::vector<RentalRecord> history;   ///< Completed rentals (rental history).
    std::vector<std::wstring> warnings;  ///< One message per skipped/invalid line.
};

class IDataStore
{
public:
    /// Virtual destructor: deleting a derived object through an IDataStore
    /// pointer/reference must run the derived destructor.
    virtual ~IDataStore();

    /// Loads all vehicles and the cumulative revenue. Invalid entries are
    /// skipped and reported in LoadResult::warnings.
    /// @throws DataStoreException if the storage cannot be read at all.
    virtual LoadResult load() = 0;

    /// Replaces the stored data with the given vehicles, revenue and rental history.
    /// @throws DataStoreException if the data cannot be written.
    virtual void save(const std::vector<Vehicle>& vehicles, const Money& revenue,
                      const std::vector<RentalRecord>& history) = 0;

    /// Human-readable description of where the data lives (e.g. a folder).
    virtual std::wstring describeLocation() const = 0;
};
