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

#ifndef FLUX_EDITOR_H
#define FLUX_EDITOR_H

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QMap>

class QCompleter;

namespace Flux {

class Highlighter;
class LineNumberArea;

/**
 * @brief High-performance base code editor for FluxScript.
 * Supports syntax highlighting, line numbers, and bracket matching.
 */
class FluxEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit FluxEditor(QWidget* parent = nullptr);

    void setCompleter(QCompleter* completer);
    QCompleter* completer() const;
    
    virtual void updateCompletionKeywords(const QStringList& keywords);
    virtual void setErrorLines(const QMap<int, QString>& errors);
    virtual void setActiveDebugLine(int line);

    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int lineNumberAreaWidth();

public slots:
    virtual void onRunRequested();
    void highlightCurrentLine();

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* e) override;

    Highlighter* m_highlighter;
    QCompleter* m_completer = nullptr;
    QWidget* m_lineNumberArea;
    QMap<int, QString> m_errorLines;
    int m_activeDebugLine = -1;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);

private:
    void setupEditor();
    QString textUnderCursor() const;
    void insertCompletion(const QString& completion);
};

class LineNumberArea : public QWidget {
public:
    LineNumberArea(FluxEditor* editor) : QWidget(editor), m_codeEditor(editor) {}

    QSize sizeHint() const override {
        return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        m_codeEditor->lineNumberAreaPaintEvent(event);
    }

private:
    FluxEditor* m_codeEditor;
};

} // namespace Flux

#endif // FLUX_EDITOR_H
