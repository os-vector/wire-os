#include "unencrypted_properties.h"

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <dirent.h>

namespace properties {
    const char* key = "key";
    const char* ref = "ref";
    const char* props = "props";
    const char* is_default = "is_default";
}

namespace
{
    const char* unencrypted_folder = "unencrypted";
}

std::string UnencryptedProperties::GetPath(const char* device)
{
    return std::string() + device + "/" + unencrypted_folder;
}

UnencryptedProperties::UnencryptedProperties(const char* device)
  : folder_(GetPath(device))
{
    DIR* dir = opendir(folder_.c_str());
    if (dir) {
        closedir(dir);
    } else {
        folder_.clear();
    }
}

UnencryptedProperties::UnencryptedProperties()
{
}

template<> std::string UnencryptedProperties::Get(const char* name,
                                      std::string default_value) const
{
    if (!OK()) return default_value;
    std::ifstream i(folder_ + "/" + name, std::ios::binary);
    if (!i) {
        return default_value;
    }

    i.seekg(0, std::ios::end);
    int length = i.tellg();
    i.seekg(0, std::ios::beg);
    if (length == -1) {
        return default_value;
    }

    std::string s(length, 0);
    i.read(&s[0], length);
    if (!i) {
        return default_value;
    }

    return s;
}

template<> bool UnencryptedProperties::Set(const char* name, std::string const& value)
{
    if (!OK()) return false;
    std::ofstream o(folder_ + "/" + name, std::ios::binary);
    o << value;
    return !o.fail();
}

UnencryptedProperties UnencryptedProperties::GetChild(const char* name) const
{
    UnencryptedProperties up;
    if (!OK()) return up;

    std::string directory(folder_ + "/" + name);
    if (mkdir(directory.c_str(), 700) == -1 && errno != EEXIST) {
        return up;
    }

    up.folder_ = directory;
    return up;
}

bool UnencryptedProperties::Remove(const char* name)
{
    if (!OK()) return false;
    if (remove((folder_ + "/" + name).c_str())
        && errno != ENOENT) {
        return false;
    }

    return true;
}

bool UnencryptedProperties::OK() const
{
    return !folder_.empty();
}
