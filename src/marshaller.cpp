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
