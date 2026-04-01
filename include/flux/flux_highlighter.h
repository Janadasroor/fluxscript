#ifndef FLUX_HIGHLIGHTER_H
#define FLUX_HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

namespace Flux {

class Highlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit Highlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat componentFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat builtinFormat;
    QTextCharFormat multiLineStringFormat;
    
    QRegularExpression multiLineStringPattern1;
    QRegularExpression multiLineStringPattern2;
};

} // namespace Flux

#endif // FLUX_HIGHLIGHTER_H
