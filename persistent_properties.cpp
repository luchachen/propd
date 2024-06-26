/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "persistent_properties.h"

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <sys/types.h>

#include <memory>

#include "util.h"
#include "log.h"


namespace propd {

std::string persistent_property_filename = "/data/property/persistent_properties";

namespace {

bool StartsWith(std::string_view s, std::string_view prefix) {
  return s.substr(0, prefix.size()) == prefix;
}

bool WriteStringToFd(std::string_view content, int fd) {
  const char* p = content.data();
  size_t left = content.size();
  while (left > 0) {
    ssize_t n = TEMP_FAILURE_RETRY(write(fd, p, left));
    if (n == -1) {
      return false;
    }
    p += n;
    left -= n;
  }
  return true;
}

std::vector<std::pair<std::string, std::string>> LoadPersistentPropertiesFromMemory() {
    std::vector<std::pair<std::string, std::string>> properties;
    __system_property_foreach(
        [](const prop_info* pi, void* cookie) {
            __system_property_read_callback(
                pi,
                [](void* cookie, const char* name, const char* value, unsigned serial) {
                    if (StartsWith(name, "persist.")) {
                        auto properties =
                            reinterpret_cast<std::vector<std::pair<std::string, std::string>>*>(
                                cookie);
                        properties->emplace_back(name, value);
                    }
                },
                cookie);
        },
        &properties);
    return properties;
}

class PersistentPropertyFileParser {
  public:
    PersistentPropertyFileParser(const std::string& contents) : contents_(contents), position_(0) {}
    Result<std::vector<std::pair<std::string, std::string>>> Parse();

  private:
    Result<std::string> ReadString();
    Result<uint32_t> ReadUint32();

    const std::string& contents_;
    size_t position_;
};

Result<std::vector<std::pair<std::string, std::string>>> PersistentPropertyFileParser::Parse() {
    std::vector<std::pair<std::string, std::string>> result;

    if (auto magic = ReadUint32(); magic) {
        if (*magic != kMagic) {
            return Error() << "Magic value '0x" << std::hex << *magic
                           << "' does not match expected value '0x" << kMagic << "'";
        }
    } else {
        return Error() << "Could not read magic value: " << magic.error();
    }

    if (auto version = ReadUint32(); version) {
        if (*version != 1) {
            return Error() << "Version '" << *version
                           << "' does not match any compatible version: (1)";
        }
    } else {
        return Error() << "Could not read version: " << version.error();
    }

    auto num_properties = ReadUint32();
    if (!num_properties) {
        return Error() << "Could not read num_properties: " << num_properties.error();
    }

    while (position_ < contents_.size()) {
        auto key = ReadString();
        if (!key) {
            return Error() << "Could not read key: " << key.error();
        }
        if (!StartsWith(*key, "persist.")) {
            return Error() << "Property '" << *key << "' does not starts with 'persist.'";
        }
        auto value = ReadString();
        if (!value) {
            return Error() << "Could not read value: " << value.error();
        }
        result.emplace_back(*key, *value);
    }

    if (result.size() != *num_properties) {
        return Error() << "Mismatch of number of persistent properties read, " << result.size()
                       << " and number of persistent properties expected, " << *num_properties;
    }

    return result;
}

Result<std::string> PersistentPropertyFileParser::ReadString() {
    auto string_length = ReadUint32();
    if (!string_length) {
        return Error() << "Could not read size for string";
    }

    if (position_ + *string_length > contents_.size()) {
        return Error() << "String size would cause it to overflow the input buffer";
    }
    auto result = std::string(contents_, position_, *string_length);
    position_ += *string_length;
    return result;
}

Result<uint32_t> PersistentPropertyFileParser::ReadUint32() {
    if (position_ + 3 > contents_.size()) {
        return Error() << "Input buffer not large enough to read uint32_t";
    }
    uint32_t result = *reinterpret_cast<const uint32_t*>(&contents_[position_]);
    position_ += sizeof(uint32_t);
    return result;
}

}  // namespace

Result<std::vector<std::pair<std::string, std::string>>> LoadPersistentPropertyFile() {
    const std::string temp_filename = persistent_property_filename + ".tmp";
    if (access(temp_filename.c_str(), F_OK) == 0) {
        LOG(INFO)
            << "Found temporary property file while attempting to persistent system properties"
               " a previous persistent property write may have failed";
        unlink(temp_filename.c_str());
    }
    auto file_contents = ReadFile(persistent_property_filename);
    if (!file_contents) {
        return Error() << "Unable to read persistent property file: " << file_contents.error();
    }
    auto parsed_contents = PersistentPropertyFileParser(*file_contents).Parse();
    if (!parsed_contents) {
        // If the file cannot be parsed, then we don't have any recovery mechanisms, so we delete
        // it to allow for future writes to take place successfully.
        unlink(persistent_property_filename.c_str());
        return Error() << "Unable to parse persistent property file: " << parsed_contents.error();
    }
    return parsed_contents;
}

std::string GenerateFileContents(
    const std::vector<std::pair<std::string, std::string>>& persistent_properties) {
    std::string result;

    uint32_t magic = kMagic;
    result.append(reinterpret_cast<char*>(&magic), sizeof(uint32_t));

    uint32_t version = 1;
    result.append(reinterpret_cast<char*>(&version), sizeof(uint32_t));

    uint32_t num_properties = persistent_properties.size();
    result.append(reinterpret_cast<char*>(&num_properties), sizeof(uint32_t));

    for (const auto& [key, value] : persistent_properties) {
        uint32_t key_length = key.length();
        result.append(reinterpret_cast<char*>(&key_length), sizeof(uint32_t));
        result.append(key);
        uint32_t value_length = value.length();
        result.append(reinterpret_cast<char*>(&value_length), sizeof(uint32_t));
        result.append(value);
    }
    return result;
}

Result<Success> WritePersistentPropertyFile(
    const std::vector<std::pair<std::string, std::string>>& persistent_properties) {
    auto file_contents = GenerateFileContents(persistent_properties);

    const std::string temp_filename = persistent_property_filename + ".tmp";
    int fd(TEMP_FAILURE_RETRY(
        open(temp_filename.c_str(), O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC | O_CLOEXEC, 0600)));
    if (fd == -1) {
        return ErrnoError() << "Could not open temporary properties file";
    }
    if (!WriteStringToFd(file_contents, fd)) {
        return ErrnoError() << "Unable to write file contents";
    }
    fsync(fd);
    close(fd);

    if (rename(temp_filename.c_str(), persistent_property_filename.c_str())) {
        int saved_errno = errno;
        unlink(temp_filename.c_str());
        return Error(saved_errno) << "Unable to rename persistent property file";
    }
    return Success();
}

// Persistent properties are not written often, so we rather not keep any data in memory and read
// then rewrite the persistent property file for each update.
void WritePersistentProperty(const std::string& name, const std::string& value) {
    auto persistent_properties = LoadPersistentPropertyFile();
    if (!persistent_properties) {
        LOG(ERROR) << "Recovering persistent properties from memory: "
                   << persistent_properties.error();
        persistent_properties = LoadPersistentPropertiesFromMemory();
    }
    auto it = std::find_if(persistent_properties->begin(), persistent_properties->end(),
                           [&name](const auto& entry) { return entry.first == name; });
    if (it != persistent_properties->end()) {
        *it = {name, value};
    } else {
        persistent_properties->emplace_back(name, value);
    }

    if (auto result = WritePersistentPropertyFile(*persistent_properties); !result) {
        LOG(ERROR) << "Could not store persistent property: " << result.error();
    }
}

std::vector<std::pair<std::string, std::string>> LoadPersistentProperties() {
    auto persistent_properties = LoadPersistentPropertyFile();

    if (!persistent_properties) {
        ERROR("Could not load single persistent property file");
    }

    return *persistent_properties;
}

}  // namespace propd
