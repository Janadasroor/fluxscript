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
