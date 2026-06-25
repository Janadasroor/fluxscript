// Embedded documentation for FluxScript CLI.
#ifndef FLUX_DOCS_EMBEDDED_H
#define FLUX_DOCS_EMBEDDED_H

#include <string>

namespace Flux {
namespace Docs {
const char* getDocsText();
std::string searchDocs(const std::string& keyword);
} // namespace Docs
} // namespace Flux

#endif // FLUX_DOCS_EMBEDDED_H
