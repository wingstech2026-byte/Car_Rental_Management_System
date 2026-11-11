// =============================================================================
// BusinessRuleSelfTest.h
// -----------------------------------------------------------------------------
// Runs every business rule against a separate, temporary RentalAgency and
// reports which checks passed. It is started from Help > Run Business Rule
// Self-Test and never touches the real data.
//
// It shows that the rules are enforced by the BUSINESS LOGIC itself, including
// rules the GUI makes impossible to break (for example renting a vehicle that
// is already rented - the GUI never lists it as rentable).
// =============================================================================
#pragma once

#include <string>
#include <vector>

/// Outcome of one self-test check.
struct SelfTestResult
{
    std::wstring name;   ///< What was checked.
    bool passed = false; ///< Did the business logic behave correctly?
    std::wstring detail; ///< Error message produced, or what went wrong.
};

class BusinessRuleSelfTest
{
public:
    /// Runs all checks and returns one result per check.
    std::vector<SelfTestResult> run() const;
};
