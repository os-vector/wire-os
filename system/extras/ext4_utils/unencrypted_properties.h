#include <string>
#include <fstream>

// key names for properties we use
namespace properties {
    extern const char* key;
    extern const char* ref;
    extern const char* props;
    extern const char* is_default;
}

/**
 * Class to store data on the unencrypted folder of a device.
 * Note that the folder must exist before this class is constructed.
 * All names must be valid single level (no '/') file or directory names
 * Data is organized hierarchically so we can get a child folder
 */
class UnencryptedProperties
{
public:
    // Get path of folder. Must create before using any properties
    // This is to allow proper setting of SELinux policy
    static std::string GetPath(const char* device);

    // Opens properties folder on named device.
    // If folder does not exist, OK will return false, all
    // getters will return default properties and setters will fail.
    UnencryptedProperties(const char* device);

    // Get named object. Return default if object does not exist or error.
    template<typename t> t Get(const char* name, t default_value = t()) const;

    // Set named object. Return true if success, false otherwise
    template<typename t> bool Set(const char* name, t const& value);

    // Get child properties
    UnencryptedProperties GetChild(const char* name) const;

    // Remove named object
    bool Remove(const char* name);

    // Does folder exist?
    bool OK() const;

private:
    UnencryptedProperties();
    std::string folder_;
};


template<typename t> t UnencryptedProperties::Get(const char* name,
                                                  t default_value) const
{
    if (!OK()) return default_value;
    t value = default_value;
    std::ifstream(folder_ + "/" + name) >> value;
    return value;
}

template<typename t> bool UnencryptedProperties::Set(const char* name,
                                                     t const& value)
{
    if (!OK()) return false;
    std::ofstream o(folder_ + "/" + name);
    o << value;
    return !o.fail();
}

// Specialized getters/setters for strings
template<> std::string UnencryptedProperties::Get(const char* name,
                                      std::string default_value) const;

template<> bool UnencryptedProperties::Set(const char* name,
                                           std::string const& value);
