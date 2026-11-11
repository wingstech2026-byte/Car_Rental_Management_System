// =============================================================================
// User.cpp
// =============================================================================
#include "User.h"

#include "RentalExceptions.h"
#include "Sha256.h"
#include "TextUtils.h"
#include "Validation.h"

#include <random>
#include <utility>

namespace
{
    constexpr int HashIterations = 1000;

    bool isLowerHex(const std::wstring& text, std::size_t expectedLength)
    {
        if (text.size() != expectedLength)
        {
            return false;
        }
        for (const wchar_t character : text)
        {
            if (!TextUtils::isAsciiDigit(character) && !(character >= L'a' && character <= L'f'))
            {
                return false;
            }
        }
        return true;
    }

    /// Compares two hashes in constant time (does not stop at the first difference).
    bool sameHash(const std::wstring& left, const std::wstring& right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        wchar_t difference = 0;
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            difference = static_cast<wchar_t>(difference | (left[index] ^ right[index]));
        }
        return difference == 0;
    }
}

std::wstring toString(Role role)
{
    return role == Role::Manager ? L"Manager" : L"Clerk";
}

bool tryParseRole(const std::wstring& text, Role& role)
{
    const std::wstring trimmed = TextUtils::trim(text);
    if (TextUtils::equalsIgnoreCase(trimmed, L"Manager"))
    {
        role = Role::Manager;
        return true;
    }
    if (TextUtils::equalsIgnoreCase(trimmed, L"Clerk"))
    {
        role = Role::Clerk;
        return true;
    }
    return false;
}

std::wstring describe(Permission permission)
{
    switch (permission)
    {
    case Permission::ManageFleet: return L"add, edit or remove vehicles";
    case Permission::RentAndReturn: return L"rent or return vehicles";
    case Permission::ViewReports: return L"view the reports";
    case Permission::ManageUsers: return L"manage user accounts";
    case Permission::ManageData: return L"reset or replace the data";
    default: return L"do this";
    }
}

User::User(std::wstring username, Role role, std::wstring salt, std::wstring passwordHash)
    : m_username(std::move(username)), m_role(role), m_salt(std::move(salt)), m_passwordHash(std::move(passwordHash))
{
}

User User::create(const std::wstring& username, const std::wstring& password, Role role)
{
    std::wstring name = Validation::normalizeUsername(username);
    Validation::validatePassword(password);
    std::wstring salt = generateSalt();
    std::wstring hash = hashPassword(salt, password);
    return User(std::move(name), role, std::move(salt), std::move(hash));
}

User User::fromStored(const std::wstring& username, Role role, const std::wstring& salt, const std::wstring& hash)
{
    std::wstring name = Validation::normalizeUsername(username);
    const std::wstring trimmedSalt = TextUtils::trim(salt);
    const std::wstring trimmedHash = TextUtils::trim(hash);
    if (!isLowerHex(trimmedSalt, 16) || !isLowerHex(trimmedHash, 64))
    {
        throw ValidationException(InputField::None, L"the password data of user \"" + name + L"\" is damaged.");
    }
    return User(std::move(name), role, trimmedSalt, trimmedHash);
}

const std::wstring& User::getUsername() const noexcept { return m_username; }
Role User::getRole() const noexcept { return m_role; }
const std::wstring& User::getSalt() const noexcept { return m_salt; }
const std::wstring& User::getPasswordHash() const noexcept { return m_passwordHash; }

bool User::checkPassword(const std::wstring& password) const
{
    return sameHash(hashPassword(m_salt, password), m_passwordHash);
}

void User::setPassword(const std::wstring& newPassword)
{
    Validation::validatePassword(newPassword);
    std::wstring salt = generateSalt();
    m_passwordHash = hashPassword(salt, newPassword);
    m_salt = std::move(salt);
}

bool User::can(Permission permission) const noexcept
{
    return roleAllows(m_role, permission);
}

bool User::roleAllows(Role role, Permission permission) noexcept
{
    if (role == Role::Manager)
    {
        return true; // managers may do everything
    }
    return permission == Permission::RentAndReturn; // clerks: renting and returning only
}

std::wstring User::hashPassword(const std::wstring& salt, const std::wstring& password)
{
    const std::string saltBytes = TextUtils::toUtf8(salt) + ":";
    std::string hash = Sha256::hashHex(saltBytes + TextUtils::toUtf8(password));
    for (int round = 1; round < HashIterations; ++round)
    {
        hash = Sha256::hashHex(saltBytes + hash);
    }
    return TextUtils::fromUtf8Lenient(hash);
}

std::wstring User::generateSalt()
{
    static const wchar_t* const hexDigits = L"0123456789abcdef";
    std::random_device device;
    std::wstring salt;
    for (int index = 0; index < 16; ++index)
    {
        salt += hexDigits[device() % 16u];
    }
    return salt;
}
