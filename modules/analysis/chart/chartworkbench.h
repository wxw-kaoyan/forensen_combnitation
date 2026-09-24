#ifndef ANALYSIS_CHART_CHARTWORKBENCH_H
#define ANALYSIS_CHART_CHARTWORKBENCH_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QFutureWatcher>

#include "modules/parse/columnextract.h"   // TextFileTable / loadTextFileTable
#include "chartwindow.h"                    // ChartWindow::Curve

class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QListWidget;
class QFutureWatcherBase;

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  通用绘图工作台（独立非模态窗口）
//  选一个 CSV 文件 → 自动识别表头 → 选时间列 + 勾选若干数据列 →
//  点「生成窗口」弹出一个 ChartWindow；可反复生成多个窗口对比。
//  数据加载与列提取均在后台线程，避免大文件阻塞 UI。
// ════════════════════════════════════════════════════════════════
class ChartWorkbench : public QWidget
{
    Q_OBJECT
public:
    explicit ChartWorkbench(QWidget *parent = nullptr);

private slots:
    void onBrowse();
    void onLoadFinished();
    void onSelectAll();
    void onDeselectAll();
    void onGenerate();
    void onGenerateFinished();

private:
    // 后台提取结果（标题 / X 轴标签 / X 数据 / 各条曲线）
    struct ExtractResult {
        QString                    title;
        QString                    xLabel;
        QVector<double>            x;
        QVector<ChartWindow::Curve> curves;
    };

    void startLoad(const QString &path);
    void startExtract();
    static QVector<double> extractColumn(const QList<QStringList> &rows, int col);

    QLineEdit   *m_pathEdit    = nullptr;
    QPushButton *m_browseBtn   = nullptr;
    QComboBox   *m_timeCombo   = nullptr;
    QListWidget *m_colList     = nullptr;
    QPushButton *m_selAllBtn   = nullptr;
    QPushButton *m_deselAllBtn = nullptr;
    QPushButton *m_generateBtn = nullptr;
    QLabel      *m_statusLabel = nullptr;

    // 全量缓存数据（后台加载；关闭工作台时释放）
    TextFileTable m_table;

    bool m_loading    = false;
    bool m_extracting = false;
    QFutureWatcher<TextFileTable>  *m_loadWatcher    = nullptr;
    QFutureWatcher<ExtractResult>  *m_extractWatcher = nullptr;

    void setBusy(bool busy, const QString &msg = QString());
};

} // namespace Analysis

#endif // ANALYSIS_CHART_CHARTWORKBENCH_H
