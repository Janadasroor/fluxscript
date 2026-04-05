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

#ifndef FLUX_GUTTER_WIDGETS_H
#define FLUX_GUTTER_WIDGETS_H

#include <QWidget>
#include <QColor>
#include <QPainter>
#include <QMap>

namespace Flux {

class FluxEditor;

/**
 * @brief Gutter marker types for visual indicators in the line number area
 */
enum class GutterMarkerType {
    Breakpoint,
    Error,
    Warning,
    Info,
    Bookmark,
    ExecutionPoint,
    FoldMarker
};

/**
 * @brief Visual marker displayed in the gutter (line number area)
 * Note: This is a simple data class, not a QWidget
 */
class GutterMarker {
public:
    explicit GutterMarker(GutterMarkerType type) : m_type(type) {}
    virtual ~GutterMarker() = default;

    GutterMarkerType type() const { return m_type; }
    virtual QColor color() const;
    virtual void paint(QPainter* painter, const QRect& rect);

private:
    GutterMarkerType m_type;
};

} // namespace Flux

#endif // FLUX_GUTTER_WIDGETS_H
