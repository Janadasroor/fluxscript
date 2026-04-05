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
