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

#ifndef FLUX_TOML_PARSER_H
#define FLUX_TOML_PARSER_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Flux {
namespace TOML {

// TOML Value types
using Value = std::variant<std::monostate, bool, int64_t, double, std::string, std::vector<std::string>>;

// TOML Document
class Document
{
public:
    Document();
    ~Document();

    // Parse TOML string
    static Document parse(const std::string& toml);

    // Parse TOML file
    static Document parseFile(const std::string& filename);

    // Get value
    Value get(const std::string& key) const;

    // Get typed value
    std::optional<std::string> getString(const std::string& key) const;
    std::optional<int64_t> getInt(const std::string& key) const;
    std::optional<double> getDouble(const std::string& key) const;
    std::optional<bool> getBool(const std::string& key) const;
    std::optional<std::vector<std::string>> getArray(const std::string& key) const;
    std::optional<Document> getTable(const std::string& key) const;

    // Check if key exists
    bool has(const std::string& key) const;

    // Get all keys
    std::vector<std::string> keys() const;

    // Serialize back to TOML
    std::string toString() const;

    // Error handling
    bool hasError() const { return !m_error.empty(); }
    std::string getError() const { return m_error; }

private:
    std::map<std::string, Value> m_data;
    std::string m_error;

    // Parser internals
    void parseContent(const std::string& content);
    void parseLine(const std::string& line);
    Value parseValue(const std::string& value);
    std::string trim(const std::string& str) const;
};

// Version parsing
struct Version
{
    int major;
    int minor;
    int patch;
    std::string prerelease;

    Version() : major(0), minor(0), patch(0) {}
    Version(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}

    static Version parse(const std::string& str);
    std::string toString() const;

    bool operator<(const Version& other) const;
    bool operator>=(const Version& other) const;
    bool operator==(const Version& other) const;
};

// Version constraint parsing
class VersionConstraint
{
public:
    static bool satisfies(const Version& version, const std::string& constraint);

private:
    static Version parseVersion(const std::string& str);
};

} // namespace TOML
} // namespace Flux

#endif // FLUX_TOML_PARSER_H
