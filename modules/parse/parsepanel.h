#ifndef PARSEPANEL_H
#define PARSEPANEL_H

#include <QWidget>
#include <QPushButton>
#include <QByteArray>
#include <QList>
#include <QString>
#include "modules/parse/parser/parserbase.h"

class ParserBase;
class QFutureWatcherBase;

namespace Ui { class ParsePanel; }

class ParsePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ParsePanel(QWidget *parent = nullptr);
    ~ParsePanel() override;

signals:

private slots:
    void onLoadFileClicked();
    void onProtocolComboActivated(int index);
    void onExportClicked();
    void onClearClicked();
    void onNewCodeClicked();
    void onDownsampleClicked();

private:
    void setupProtocolCombo();
    void updateFileInfo();
    void setComboEnabled(bool enabled);
    static ParseResult doParse(int protocolIndex, bool skipCrc,
                               const QByteArray &rawData,
                               const QString &filePath);

    Ui::ParsePanel *ui;
    QByteArray       m_rawData;
    QString          m_filePath;
    QString          m_cachedFullText;
    QString          m_cachedCsvPath;    // 凯芯等外部生成的 CSV 路径
    ProtocolID       m_activeProtocolId = ProtocolID::Unknown;
    QList<ParsedFrame> m_cachedFrames;
    QList<int>       m_comboToParser;  // 下拉项索引 → 工厂解析器索引（分类头/占位项为 -1）
    bool m_parsing = false;
};

#endif // PARSEPANEL_H
