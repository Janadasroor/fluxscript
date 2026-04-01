#ifndef FLUX_MARSHALLER_H
#define FLUX_MARSHALLER_H

#include <map>
#include <string>

#ifdef slots
#undef slots
#endif
#include <pybind11/pybind11.h>
#define slots Q_SLOTS

namespace Flux {

class Marshaller {
public:
    static pybind11::dict mapToDict(const std::map<std::string, double>& data);
    static std::map<std::string, double> dictToMap(const pybind11::object& result);
};

} // namespace Flux

#endif // FLUX_MARSHALLER_H
