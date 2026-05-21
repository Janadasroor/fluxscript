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

// ============================================================================
// FluxScript Example Gallery
// Catalog, categorize, and browse 100+ .flux example files
// ============================================================================

#ifndef FLUX_EXAMPLE_GALLERY_H
#define FLUX_EXAMPLE_GALLERY_H

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Flux {

// Example entry
struct ExampleEntry
{
    std::string name;              // Filename without extension
    std::string path;              // Full path to .flux file
    std::string category;          // e.g., "math", "control_flow", "spice"
    std::string description;       // Short description
    std::string difficulty;        // "beginner", "intermediate", "advanced"
    std::vector<std::string> tags; // Keywords
    std::string source_preview;    // First 20 lines of the file
    size_t line_count;
    bool runnable; // Whether it passes JIT compilation
};

// Category summary
struct CategorySummary
{
    std::string name;
    std::string description;
    std::string icon; // Emoji or icon
    int example_count;
    std::vector<std::string> tags; // Common tags in this category
};

// Gallery search/filter options
struct GalleryFilter
{
    std::string category;
    std::string difficulty;
    std::string tag;
    std::string search_query;
    bool runnable_only = false;
};

// Example Gallery
class ExampleGallery
{
public:
    static ExampleGallery& instance();

    // Initialize by scanning examples directory
    void initialize(const std::string& examples_dir = "examples/");
    void finalize();
    bool isInitialized() const { return m_initialized; }

    // Get all examples
    std::vector<ExampleEntry> getAllExamples() const;

    // Get examples by category
    std::vector<ExampleEntry> getExamplesByCategory(const std::string& category) const;
    std::vector<std::string> getCategories() const;
    std::map<std::string, CategorySummary> getCategorySummaries() const;

    // Search and filter
    std::vector<ExampleEntry> search(const std::string& query) const;
    std::vector<ExampleEntry> filter(const GalleryFilter& filter) const;

    // Get single example
    const ExampleEntry* getExample(const std::string& name) const;

    // Run example
    bool runExample(const std::string& name, std::string* output = nullptr, std::string* error = nullptr);

    // Export
    std::string generateCatalogMarkdown() const;
    std::string generateCatalogHTML() const;
    std::string generateCatalogJSON() const;

    // Statistics
    int totalExamples() const;
    int runnableExamples() const;
    int categories() const;
    std::map<std::string, int> examplesByCategory() const;
    std::map<std::string, int> examplesByDifficulty() const;

private:
    ExampleGallery() = default;
    ~ExampleGallery() = default;

    void scanDirectory(const std::string& dir);
    void categorizeExample(ExampleEntry& entry);
    std::string readFilePreview(const std::string& path, int max_lines = 20);
    std::string inferCategoryFromName(const std::string& filename);
    std::string inferDifficulty(const std::string& filename);
    bool testRunnable(const std::string& path);

    std::map<std::string, ExampleEntry> m_examples;
    std::map<std::string, CategorySummary> m_categories;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
};

} // namespace Flux

#endif // FLUX_EXAMPLE_GALLERY_H
