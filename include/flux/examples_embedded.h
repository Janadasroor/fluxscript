// Embedded examples for FluxScript CLI.
#ifndef FLUX_EXAMPLES_EMBEDDED_H
#define FLUX_EXAMPLES_EMBEDDED_H

#include <string>

namespace Flux {
namespace Examples {
const char* getExamplesText();
std::string searchExamples(const std::string& keyword);
} // namespace Examples
} // namespace Flux

#endif // FLUX_EXAMPLES_EMBEDDED_H
