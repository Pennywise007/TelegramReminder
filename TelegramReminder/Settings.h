#pragma once

#include <ext/core/singleton.h>
#include <ext/std/filesystem.h>
#include <ext/serialization/iserializable.h>

#include "TelegramThread.h"

class Settings : private ext::serializable::SerializableObject<Settings>
{
    friend ext::Singleton<Settings>;
    Settings()
    {
        using namespace ext::serializer;
        try
        {
            Executor::DeserializeObject(Factory::XMLDeserializer(get_settings_path()), this);
        }
        catch (...)
        {
        }
    }
    ~Settings()
    {
        Store();
    }

private:
    [[nodiscard]] static std::filesystem::path get_settings_path()
    {
        const static auto result = []()
        {
            std::wstring exeName = std::filesystem::get_binary_name().c_str();
            exeName = exeName.substr(0, exeName.rfind('.')) + L".xml";
            return std::filesystem::get_exe_directory() / exeName;
        }();
        return result;
    }

public:
    void Store()
    {
        using namespace ext::serializer;
        try
        {
            Executor::SerializeObject(Factory::XMLSerializer(get_settings_path()), this);
        }
        catch (...)
        {
        }
    }

    struct User : private ext::serializable::SerializableObject<User>
    {
        User() noexcept = default;
        User(const TgBot::User::Ptr& user) noexcept
        {
            id = user->id;
            firstName = getUNICODEString(user->firstName);
            userName = getUNICODEString(user->username);
        }
        DECLARE_SERIALIZABLE_FIELD(std::int64_t, id);
        DECLARE_SERIALIZABLE_FIELD(std::wstring, firstName);
        DECLARE_SERIALIZABLE_FIELD(std::wstring, userName);
    };

    DECLARE_SERIALIZABLE_FIELD(std::wstring, token);
    DECLARE_SERIALIZABLE_FIELD(std::wstring, password);
    DECLARE_SERIALIZABLE_FIELD(std::list<User>, registeredUsers);
};