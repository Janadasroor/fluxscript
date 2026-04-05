/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0
 http://www.apache.org/licenses/LICENSE-2.0
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
     http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License. */

// TOML Parser Implementation
#include "flux/package/toml_parser.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>

namespace Flux {
namespace TOML {

Document::Document() = default;
Document::~Document() = default;

Document Document::parse(const std::string& toml) {
    Document doc;
    doc.parseContent(toml);
    return doc;
}

Document Document::parseFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        Document doc;
        doc.m_error = "Cannot open file: " + filename;
        return doc;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str());
}

void Document::parseContent(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    std::string currentTable;
    
    while (std::getline(stream, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Table header [table.name]
        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                currentTable = trim(line.substr(1, end - 1));
            }
            continue;
        }
        
        // Key = Value
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = trim(line.substr(0, eqPos));
            std::string value = trim(line.substr(eqPos + 1));
            
            Value parsedValue = parseValue(value);
            
            if (!currentTable.empty()) {
                key = currentTable + "." + key;
            }
            
            // Set nested value
            m_data[key] = parsedValue;
        }
    }
}

Value Document::parseValue(const std::string& value) {
    std::string trimmed = trim(value);
    
    // Boolean
    if (trimmed == "true") return true;
    if (trimmed == "false") return false;
    
    // String (quoted)
    if ((trimmed[0] == '"' && trimmed.back() == '"') ||
        (trimmed[0] == '\'' && trimmed.back() == '\'')) {
        return trimmed.substr(1, trimmed.length() - 2);
    }
    
    // Array
    if (trimmed[0] == '[') {
        std::vector<std::string> arr;
        std::string inner = trimmed.substr(1, trimmed.length() - 2);
        std::istringstream stream(inner);
        std::string item;
        while (std::getline(stream, item, ',')) {
            arr.push_back(trim(item));
        }
        return arr;
    }
    
    // Integer
    try {
        int64_t intVal = std::stoll(trimmed);
        return intVal;
    } catch (...) {}
    
    // Double
    try {
        double doubleVal = std::stod(trimmed);
        return doubleVal;
    } catch (...) {}
    
    // Default to string
    return trimmed;
}

Value Document::get(const std::string& key) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it->second;
    }
    return Value();
}

std::optional<std::string> Document::getString(const std::string& key) const {
    Value val = get(key);
    if (auto* s = std::get_if<std::string>(&val)) {
        return *s;
    }
    return std::nullopt;
}

std::optional<int64_t> Document::getInt(const std::string& key) const {
    Value val = get(key);
    if (auto* i = std::get_if<int64_t>(&val)) {
        return *i;
    }
    return std::nullopt;
}

std::optional<double> Document::getDouble(const std::string& key) const {
    Value val = get(key);
    if (auto* d = std::get_if<double>(&val)) {
        return *d;
    }
    return std::nullopt;
}

std::optional<bool> Document::getBool(const std::string& key) const {
    Value val = get(key);
    if (auto* b = std::get_if<bool>(&val)) {
        return *b;
    }
    return std::nullopt;
}

std::optional<std::vector<std::string>> Document::getArray(const std::string& key) const {
    Value val = get(key);
    if (auto* arr = std::get_if<std::vector<std::string>>(&val)) {
        return *arr;
    }
    return std::nullopt;
}

std::optional<Document> Document::getTable(const std::string& key) const {
    // Simplified - would need proper nested table support
    return std::nullopt;
}

bool Document::has(const std::string& key) const {
    return m_data.find(key) != m_data.end();
}

std::vector<std::string> Document::keys() const {
    std::vector<std::string> keys;
    for (const auto& pair : m_data) {
        keys.push_back(pair.first);
    }
    return keys;
}

std::string Document::toString() const {
    std::ostringstream oss;
    for (const auto& pair : m_data) {
        oss << pair.first << " = ";
        if (auto* s = std::get_if<std::string>(&pair.second)) {
            oss << "\"" << *s << "\"";
        } else if (auto* i = std::get_if<int64_t>(&pair.second)) {
            oss << *i;
        } else if (auto* b = std::get_if<bool>(&pair.second)) {
            oss << (*b ? "true" : "false");
        }
        oss << "\n";
    }
    return oss.str();
}

std::string Document::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Version parsing
Version Version::parse(const std::string& str) {
    Version ver;
    
    // Handle prerelease
    size_t dashPos = str.find('-');
    std::string versionPart = str;
    if (dashPos != std::string::npos) {
        ver.prerelease = str.substr(dashPos + 1);
        versionPart = str.substr(0, dashPos);
    }
    
    // Parse major.minor.patch
    int major = 0, minor = 0, patch = 0;
    int matched = sscanf(versionPart.c_str(), "%d.%d.%d", &major, &minor, &patch);
    
    ver.major = major;
    if (matched >= 2) ver.minor = minor;
    if (matched >= 3) ver.patch = patch;
    
    return ver;
}

std::string Version::toString() const {
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    if (!prerelease.empty()) {
        oss << "-" << prerelease;
    }
    return oss.str();
}

bool Version::operator<(const Version& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;
    // Prerelease versions are less than release versions
    if (!prerelease.empty() && other.prerelease.empty()) return true;
    if (prerelease.empty() && !other.prerelease.empty()) return false;
    return prerelease < other.prerelease;
}

bool Version::operator>=(const Version& other) const {
    return !(*this < other);
}

bool Version::operator==(const Version& other) const {
    return major == other.major && minor == other.minor && 
           patch == other.patch && prerelease == other.prerelease;
}

// Version constraint
bool VersionConstraint::satisfies(const Version& version, const std::string& constraint) {
    std::string trimmed = constraint;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    
    if (trimmed.empty()) return true;
    
    // Handle >= constraint
    if (trimmed.substr(0, 2) == ">=") {
        Version minVer = Version::parse(trimmed.substr(2));
        return version >= minVer;
    }
    
    // Handle < constraint
    if (trimmed[0] == '<') {
        Version maxVer = Version::parse(trimmed.substr(1));
        return version < maxVer;
    }
    
    // Handle exact match
    Version constraintVer = Version::parse(trimmed);
    return version == constraintVer;
}

Version VersionConstraint::parseVersion(const std::string& str) {
    return Version::parse(str);
}

} // namespace TOML
} // namespace Flux
