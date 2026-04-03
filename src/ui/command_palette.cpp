#include "flux/ui/command_palette.h"
#include <QPainter>
#include <QApplication>
#include <QScreen>

namespace Flux {

CommandPalette::CommandPalette(QWidget* parent)
    : QDialog(parent)
    , m_searchEdit(nullptr)
    , m_commandList(nullptr)
    , m_layout(nullptr)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setModal(true);
    setMinimumSize(600, 400);
    setMaximumSize(600, 500);
    
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    
    // Search input
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Type to search commands...");
    m_searchEdit->setStyleSheet(
        "QLineEdit { "
        "    background-color: #252526; "
        "    color: #cccccc; "
        "    border: 1px solid #007acc; "
        "    border-bottom: none; "
        "    padding: 12px 16px; "
        "    font-size: 14px; "
        "}"
        "QLineEdit:focus { border: 1px solid #007acc; }"
    );
    m_layout->addWidget(m_searchEdit);
    
    // Command list
    m_commandList = new QListWidget();
    m_commandList->setStyleSheet(
        "QListWidget { "
        "    background-color: #252526; "
        "    color: #cccccc; "
        "    border: 1px solid #454545; "
        "    outline: none; "
        "}"
        "QListWidget::item { "
        "    padding: 8px 16px; "
        "    border-bottom: 1px solid #333333; "
        "}"
        "QListWidget::item:selected { "
        "    background-color: #094771; "
        "}"
        "QListWidget::item:hover { "
        "    background-color: #2a2d2e; "
        "}"
    );
    m_layout->addWidget(m_commandList, 1);
    
    connect(m_searchEdit, &QLineEdit::textChanged, this, &CommandPalette::onTextChanged);
    connect(m_commandList, &QListWidget::itemActivated, this, &CommandPalette::onItemActivated);
}

CommandPalette::~CommandPalette()
{
}

void CommandPalette::registerCommand(const CommandEntry& cmd)
{
    m_allCommands.append(cmd);
}

void CommandPalette::registerCommand(const QString& name, const QString& shortcut, std::function<void()> callback, int category)
{
    CommandEntry cmd;
    cmd.name = name;
    cmd.shortcut = shortcut;
    cmd.callback = std::move(callback);
    cmd.category = category;
    cmd.description = "";
    m_allCommands.append(cmd);
}

void CommandPalette::showPalette()
{
    m_searchEdit->clear();
    m_commandList->clear();
    m_filteredCommands = m_allCommands;
    
    // Populate list
    for (const auto& cmd : m_allCommands) {
        auto* item = new QListWidgetItem();
        item->setText(cmd.name);
        item->setData(Qt::UserRole, m_filteredCommands.indexOf(cmd));
        m_commandList->addItem(item);
    }
    
    m_commandList->setCurrentRow(0);
    
    // Position in center of screen
    if (QScreen* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        move(screenGeometry.center().x() - width() / 2, 
             screenGeometry.center().y() - height() / 2);
    }
    
    show();
    m_searchEdit->setFocus();
}

void CommandPalette::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Down:
        if (m_commandList->currentRow() < m_commandList->count() - 1) {
            m_commandList->setCurrentRow(m_commandList->currentRow() + 1);
        }
        event->accept();
        break;
        
    case Qt::Key_Up:
        if (m_commandList->currentRow() > 0) {
            m_commandList->setCurrentRow(m_commandList->currentRow() - 1);
        }
        event->accept();
        break;
        
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_commandList->currentRow() >= 0) {
            onItemActivated(m_commandList->currentItem());
        }
        event->accept();
        break;
        
    case Qt::Key_Escape:
        reject();
        event->accept();
        break;
        
    default:
        QDialog::keyPressEvent(event);
        break;
    }
}

void CommandPalette::onTextChanged(const QString& text)
{
    filterCommands(text);
}

void CommandPalette::onItemActivated(QListWidgetItem* item)
{
    if (!item) return;
    
    int index = item->data(Qt::UserRole).toInt();
    if (index >= 0 && index < m_filteredCommands.size()) {
        const auto& cmd = m_filteredCommands[index];
        if (cmd.callback) {
            cmd.callback();
        }
    }
    
    accept();
}

void CommandPalette::filterCommands(const QString& query)
{
    m_commandList->clear();
    m_filteredCommands.clear();
    
    if (query.isEmpty()) {
        m_filteredCommands = m_allCommands;
    } else {
        // Fuzzy match
        for (const auto& cmd : m_allCommands) {
            int score = fuzzyMatchScore(query.toLower(), cmd.name.toLower());
            if (score > 0) {
                m_filteredCommands.append(cmd);
            }
        }
    }
    
    // Populate list
    for (int i = 0; i < m_filteredCommands.size(); ++i) {
        const auto& cmd = m_filteredCommands[i];
        auto* item = new QListWidgetItem();
        item->setText(cmd.name);
        item->setData(Qt::UserRole, i);
        m_commandList->addItem(item);
    }
    
    if (m_commandList->count() > 0) {
        m_commandList->setCurrentRow(0);
    }
}

int CommandPalette::fuzzyMatchScore(const QString& pattern, const QString& text)
{
    if (pattern.isEmpty()) return 1;
    if (text.isEmpty()) return 0;
    
    int patternIdx = 0;
    int textIdx = 0;
    int score = 0;
    int consecutiveBonus = 0;
    
    while (patternIdx < pattern.length() && textIdx < text.length()) {
        if (pattern[patternIdx] == text[textIdx]) {
            score += 10;
            if (consecutiveBonus > 0) {
                score += consecutiveBonus * 5;
            }
            consecutiveBonus++;
            patternIdx++;
        } else {
            consecutiveBonus = 0;
        }
        textIdx++;
    }
    
    return (patternIdx == pattern.length()) ? score : 0;
}

} // namespace Flux
