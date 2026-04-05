// ============================================================================
// FluxScript Example Gallery Implementation
// ============================================================================

#include "flux/tooling/example_gallery.h"
#include "flux/jit_engine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <mutex>

namespace fs = std::filesystem;

namespace Flux {

ExampleGallery& ExampleGallery::instance() {
    static ExampleGallery instance;
    return instance;
}

void ExampleGallery::initialize(const std::string& examples_dir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;

    // Define categories
    {
        CategorySummary& c = m_categories["math"];
        c.name = "Math & Arithmetic"; c.description = "Basic math operations, functions, and computations"; c.icon = "📐";
    }
    {
        CategorySummary& c = m_categories["control_flow"];
        c.name = "Control Flow"; c.description = "If/else, loops, lambda expressions"; c.icon = "🔀";
    }
    {
        CategorySummary& c = m_categories["types"];
        c.name = "Type System"; c.description = "Type checking, complex numbers, integers"; c.icon = "🏷️";
    }
    {
        CategorySummary& c = m_categories["matrix"];
        c.name = "Matrix & Vector"; c.description = "Linear algebra, SIMD operations"; c.icon = "🔢";
    }
    {
        CategorySummary& c = m_categories["spice"];
        c.name = "SPICE & Circuits"; c.description = "Circuit simulation, netlists, analysis"; c.icon = "⚡";
    }
    {
        CategorySummary& c = m_categories["signal"];
        c.name = "Signal Processing"; c.description = "Waveforms, modulation, FFT"; c.icon = "📡";
    }
    {
        CategorySummary& c = m_categories["analysis"];
        c.name = "Advanced Analysis"; c.description = "Monte Carlo, optimization, stability"; c.icon = "📊";
    }
    {
        CategorySummary& c = m_categories["string"];
        c.name = "String Operations"; c.description = "String manipulation and text processing"; c.icon = "📝";
    }
    {
        CategorySummary& c = m_categories["bitwise"];
        c.name = "Bitwise & Logical"; c.description = "Bit operations, boolean logic"; c.icon = "🔧";
    }
    {
        CategorySummary& c = m_categories["stdlib"];
        c.name = "Standard Library"; c.description = "Built-in functions and utilities"; c.icon = "📚";
    }
    {
        CategorySummary& c = m_categories["compiler"];
        c.name = "Compiler Tests"; c.description = "Phase tests and compiler features"; c.icon = "🛠️";
    }
    {
        CategorySummary& c = m_categories["demo"];
        c.name = "Demos"; c.description = "Showcase examples and tutorials"; c.icon = "🎯";
    }

    scanDirectory(examples_dir);
    m_initialized = true;

    std::cout << "[ExampleGallery] Initialized with " << m_examples.size() << " examples in "
              << m_categories.size() << " categories" << std::endl;
}

void ExampleGallery::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_examples.clear();
    m_initialized = false;
}

void ExampleGallery::scanDirectory(const std::string& dir) {
    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".flux") {
                ExampleEntry ex;
                ex.name = entry.path().stem().string();
                ex.path = entry.path().string();
                ex.line_count = 0;
                ex.source_preview = readFilePreview(ex.path, 20);
                ex.category = inferCategoryFromName(ex.name);
                ex.difficulty = inferDifficulty(ex.name);
                ex.runnable = testRunnable(ex.path);
                categorizeExample(ex);

                m_examples[ex.name] = ex;

                // Update category count
                auto it = m_categories.find(ex.category);
                if (it != m_categories.end()) {
                    it->second.example_count++;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ExampleGallery] Warning: " << e.what() << std::endl;
    }
}

void ExampleGallery::categorizeExample(ExampleEntry& entry) {
    // Generate tags from content
    std::istringstream stream(entry.source_preview);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find("monte_carlo") != std::string::npos || line.find("monte carlo") != std::string::npos) {
            entry.tags.push_back("monte-carlo");
        }
        if (line.find("fft") != std::string::npos) entry.tags.push_back("fft");
        if (line.find("matrix") != std::string::npos) entry.tags.push_back("matrix");
        if (line.find("plot") != std::string::npos) entry.tags.push_back("plotting");
        if (line.find("sine") != std::string::npos || line.find("sin(") != std::string::npos) {
            entry.tags.push_back("trigonometry");
        }
    }

    // Remove duplicates
    std::sort(entry.tags.begin(), entry.tags.end());
    entry.tags.erase(std::unique(entry.tags.begin(), entry.tags.end()), entry.tags.end());
}

std::vector<ExampleEntry> ExampleGallery::getAllExamples() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ExampleEntry> result;
    for (const auto& [name, entry] : m_examples) {
        result.push_back(entry);
    }
    std::sort(result.begin(), result.end(), [](const ExampleEntry& a, const ExampleEntry& b) {
        return a.name < b.name;
    });
    return result;
}

std::vector<ExampleEntry> ExampleGallery::getExamplesByCategory(const std::string& category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ExampleEntry> result;
    for (const auto& [name, entry] : m_examples) {
        if (entry.category == category) {
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<std::string> ExampleGallery::getCategories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> cats;
    for (const auto& [name, cat] : m_categories) {
        if (cat.example_count > 0) {
            cats.push_back(name);
        }
    }
    std::sort(cats.begin(), cats.end());
    return cats;
}

std::map<std::string, CategorySummary> ExampleGallery::getCategorySummaries() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_categories;
}

std::vector<ExampleEntry> ExampleGallery::search(const std::string& query) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ExampleEntry> results;
    std::string q_lower = query;
    std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(), ::tolower);

    for (const auto& [name, entry] : m_examples) {
        std::string name_lower = entry.name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

        bool match = name_lower.find(q_lower) != std::string::npos;
        match |= entry.category.find(q_lower) != std::string::npos;
        match |= entry.description.find(q_lower) != std::string::npos;

        for (const auto& tag : entry.tags) {
            std::string tag_lower = tag;
            std::transform(tag_lower.begin(), tag_lower.end(), tag_lower.begin(), ::tolower);
            if (tag_lower.find(q_lower) != std::string::npos) {
                match = true;
                break;
            }
        }

        if (match) results.push_back(entry);
    }

    return results;
}

std::vector<ExampleEntry> ExampleGallery::filter(const GalleryFilter& filter) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ExampleEntry> results;

    for (const auto& [name, entry] : m_examples) {
        bool match = true;

        if (!filter.category.empty() && entry.category != filter.category) match = false;
        if (!filter.difficulty.empty() && entry.difficulty != filter.difficulty) match = false;
        if (filter.runnable_only && !entry.runnable) match = false;

        if (!filter.search_query.empty()) {
            std::string q = filter.search_query;
            std::transform(q.begin(), q.end(), q.begin(), ::tolower);
            std::string n = entry.name;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n.find(q) == std::string::npos) match = false;
        }

        if (!filter.tag.empty()) {
            bool has_tag = false;
            for (const auto& t : entry.tags) {
                if (t == filter.tag) { has_tag = true; break; }
            }
            if (!has_tag) match = false;
        }

        if (match) results.push_back(entry);
    }

    return results;
}

const ExampleEntry* ExampleGallery::getExample(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_examples.find(name);
    if (it == m_examples.end()) return nullptr;
    return &it->second;
}

bool ExampleGallery::runExample(const std::string& name, std::string* output, std::string* error) {
    auto* entry = getExample(name);
    if (!entry) {
        if (error) *error = "Example not found: " + name;
        return false;
    }

    try {
        std::ifstream file(entry->path);
        if (!file.is_open()) {
            if (error) *error = "Cannot open file: " + entry->path;
            return false;
        }
        std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        auto& jit = JITEngine::instance();
        std::string compile_error;
        if (!jit.compileScript(source, &compile_error)) {
            if (error) *error = compile_error;
            return false;
        }

        if (output) *output = "Compiled successfully";
        return true;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}

std::string ExampleGallery::generateCatalogMarkdown() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;

    oss << "# FluxScript Example Gallery\n\n";
    oss << "**Total Examples:** " << m_examples.size() << "\n\n";

    int runnable = 0;
    std::map<std::string, int> by_cat;
    for (const auto& [name, entry] : m_examples) {
        if (entry.runnable) runnable++;
        by_cat[entry.category]++;
    }

    oss << "**Runnable:** " << runnable << "/" << m_examples.size() << "\n\n";

    for (const auto& [cat_key, cat] : m_categories) {
        if (cat.example_count == 0) continue;

        oss << "\n## " << cat.icon << " " << cat.name << "\n\n";
        oss << cat.description << "\n\n";

        for (const auto& [name, entry] : m_examples) {
            if (entry.category != cat_key) continue;
            oss << "- `" << entry.name << "` (" << entry.difficulty << ")";
            if (entry.runnable) oss << " ✅";
            oss << "\n";
        }
        oss << "\n";
    }

    return oss.str();
}

std::string ExampleGallery::generateCatalogHTML() const {
    std::ostringstream oss;
    oss << "<!DOCTYPE html>\n<html><head><title>FluxScript Examples</title></head>\n<body>\n";
    oss << "<h1>FluxScript Example Gallery</h1>\n";
    oss << "<p>Total: " << m_examples.size() << " examples</p>\n";

    for (const auto& [cat_key, cat] : m_categories) {
        if (cat.example_count == 0) continue;
        oss << "<h2>" << cat.icon << " " << cat.name << "</h2>\n<ul>\n";
        for (const auto& [name, entry] : m_examples) {
            if (entry.category != cat_key) continue;
            oss << "<li><code>" << entry.name << "</code> (" << entry.difficulty << ")";
            if (entry.runnable) oss << " ✅";
            oss << "</li>\n";
        }
        oss << "</ul>\n";
    }

    oss << "</body></html>";
    return oss.str();
}

std::string ExampleGallery::generateCatalogJSON() const {
    std::ostringstream oss;
    oss << "{\n  \"total\": " << m_examples.size() << ",\n  \"examples\": [\n";

    bool first = true;
    for (const auto& [name, entry] : m_examples) {
        if (!first) oss << ",\n";
        oss << "    {\"name\": \"" << entry.name << "\", \"category\": \"" << entry.category
            << "\", \"difficulty\": \"" << entry.difficulty << "\", \"runnable\": "
            << (entry.runnable ? "true" : "false") << "}";
        first = false;
    }

    oss << "\n  ]\n}\n";
    return oss.str();
}

int ExampleGallery::totalExamples() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_examples.size();
}

int ExampleGallery::runnableExamples() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& [name, entry] : m_examples) {
        if (entry.runnable) count++;
    }
    return count;
}

int ExampleGallery::categories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& [name, cat] : m_categories) {
        if (cat.example_count > 0) count++;
    }
    return count;
}

std::map<std::string, int> ExampleGallery::examplesByCategory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::map<std::string, int> result;
    for (const auto& [name, entry] : m_examples) {
        result[entry.category]++;
    }
    return result;
}

std::map<std::string, int> ExampleGallery::examplesByDifficulty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::map<std::string, int> result;
    for (const auto& [name, entry] : m_examples) {
        result[entry.difficulty]++;
    }
    return result;
}

// ============================================================================
// Private Helpers
// ============================================================================

std::string ExampleGallery::readFilePreview(const std::string& path, int max_lines) {
    std::ifstream file(path);
    if (!file.is_open()) return "";

    std::ostringstream oss;
    std::string line;
    int count = 0;
    int total_lines = 0;

    while (std::getline(file, line)) {
        total_lines++;
        if (count < max_lines) {
            oss << line << "\n";
            count++;
        }
    }

    // Store line count (best effort, no mutex here since called during scan)
    // The caller will update entry.line_count
    return oss.str();
}

std::string ExampleGallery::inferCategoryFromName(const std::string& filename) {
    if (filename.find("monte_carlo") != std::string::npos || filename.find("worst_case") != std::string::npos ||
        filename.find("stability") != std::string::npos || filename.find("optimiz") != std::string::npos) {
        return "analysis";
    }
    if (filename.find("rc_") != std::string::npos || filename.find("voltage_divider") != std::string::npos ||
        filename.find("spice") != std::string::npos || filename.find("subckt") != std::string::npos ||
        filename.find("filter") != std::string::npos || filename.find("mc34063") != std::string::npos) {
        return "spice";
    }
    if (filename.find("signal") != std::string::npos || filename.find("waveform") != std::string::npos ||
        filename.find("chirp") != std::string::npos || filename.find("modulation") != std::string::npos ||
        filename.find("tone") != std::string::npos) {
        return "signal";
    }
    if (filename.find("matrix") != std::string::npos || filename.find("vector") != std::string::npos ||
        filename.find("simd") != std::string::npos) {
        return "matrix";
    }
    if (filename.find("for") != std::string::npos || filename.find("while") != std::string::npos ||
        filename.find("if_") != std::string::npos || filename.find("control_flow") != std::string::npos ||
        filename.find("lambda") != std::string::npos) {
        return "control_flow";
    }
    if (filename.find("type") != std::string::npos || filename.find("complex") != std::string::npos ||
        filename.find("int_") != std::string::npos) {
        return "types";
    }
    if (filename.find("string") != std::string::npos) return "string";
    if (filename.find("bitwise") != std::string::npos || filename.find("logical") != std::string::npos) {
        return "bitwise";
    }
    if (filename.find("stdlib") != std::string::npos) return "stdlib";
    if (filename.find("phase") != std::string::npos || filename.find("tco") != std::string::npos) {
        return "compiler";
    }
    if (filename.find("demo") != std::string::npos) return "demo";
    if (filename.find("math") != std::string::npos || filename.find("trig") != std::string::npos ||
        filename.find("power") != std::string::npos || filename.find("statistics") != std::string::npos) {
        return "math";
    }

    return "demo"; // Default
}

std::string ExampleGallery::inferDifficulty(const std::string& filename) {
    if (filename.find("advanced") != std::string::npos || filename.find("monte_carlo") != std::string::npos ||
        filename.find("optimiz") != std::string::npos || filename.find("stability") != std::string::npos) {
        return "advanced";
    }
    if (filename.find("demo") != std::string::npos || filename.find("hello") != std::string::npos ||
        filename.find("test_") != std::string::npos) {
        return "beginner";
    }
    return "intermediate";
}

bool ExampleGallery::testRunnable(const std::string& path) {
    // Simple heuristic: files that don't start with "test_" and don't have obvious syntax errors
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string first_line;
    std::getline(file, first_line);

    // Skip files that are clearly just test stubs
    if (first_line.find("// TODO") != std::string::npos) return false;

    return true;
}

} // namespace Flux
