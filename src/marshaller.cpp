#include "flux/marshaller.h"

namespace py = pybind11;

namespace Flux {

py::dict Marshaller::mapToDict(const std::map<std::string, double>& data) {
    py::dict d;
    for (const auto& [name, val] : data) {
        d[py::cast(name)] = val;
    }
    return d;
}

std::map<std::string, double> Marshaller::dictToMap(const py::object& result) {
    std::map<std::string, double> outputs;

    if (py::isinstance<py::float_>(result) || py::isinstance<py::int_>(result)) {
        outputs["out"] = result.cast<double>();
    } else if (py::isinstance<py::dict>(result)) {
        py::dict d = result.cast<py::dict>();
        for (auto item : d) {
            outputs[item.first.cast<std::string>()] = item.second.cast<double>();
        }
    }

    return outputs;
}

} // namespace Flux
