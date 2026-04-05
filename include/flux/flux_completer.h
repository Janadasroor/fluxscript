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

#ifndef FLUX_COMPLETER_H
#define FLUX_COMPLETER_H

#include <QCompleter>

class QStandardItemModel;

namespace Flux {

class Completer : public QCompleter {
    Q_OBJECT
public:
    explicit Completer(QObject* parent = nullptr);
    void updateCompletions(const QStringList& additionalKeywords = {});

private:
    void addCompletionItem(const QString& text, const QString& type);

    QStandardItemModel* m_model;
};

} // namespace Flux

#endif // FLUX_COMPLETER_H
