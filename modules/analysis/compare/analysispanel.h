#ifndef ANALYSIS_ANALYSISPANEL_H
#define ANALYSIS_ANALYSISPANEL_H

#include <QWidget>
#include <QList>
#include <QString>
#include <QVector>

#include "../../parse/columnextract.h"   // TextFileTable / loadTextFileTable

class QTableWidget;
class QListWidget;
class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QFutureWatcherBase;

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  对比分析面板
//  以「真值文件」为基准，导入一个或多个对比文件，手动指定列映射，
//  时间戳精确对齐（完全相等才参与计算），按场景分别计算
//  均值偏置 / RMS / 2σ。
// ════════════════════════════════════════════════════════════════
class AnalysisPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AnalysisPanel(QWidget *parent = nullptr);
    ~AnalysisPanel() override;

private slots:
    void onBrowseTruth();
    void onTruthTimeColChanged(int idx);
    void onAddCmpFile();
    void onRemoveCmpFile();
    void onCmpFileSelected(int row);
    void onCmpTimeColChanged(int idx);
    void onAddMetric();
    void onRemoveMetric();
    void onAddScene();
    void onRemoveScene();
    void onStartCompare();

private:
    // 一个对比项：真值列 + 每个对比文件对应的列 + 是否扣均值
    struct MetricMapping {
        QString      name;
        int          truthCol     = -1;
        bool         subtractMean = false;
        QVector<int> cmpCols;               // 与 m_cmpFiles 一一对应，-1 表示未映射
    };

    // 场景：手动截断时间定义
    struct SceneSegment {
        QString name;
        double  startSec = 0;
        double  endSec   = 0;
    };

    // 数据
    TextFileTable         m_truth;
    QList<TextFileTable>  m_cmpFiles;
    QList<MetricMapping>  m_mappings;
    QList<SceneSegment>   m_scenes;
    bool                  m_loading = false;
    QString               m_lastDir;          // 记住上次打开目录

    // 异步加载
    QFutureWatcherBase   *m_watcher  = nullptr;
    QList<QWidget*>       m_busyWidgets;    // 忙碌时禁用的控件

    // 控件
    QLineEdit     *m_truthPathEdit = nullptr;
    QComboBox     *m_truthTimeCombo = nullptr;
    QListWidget   *m_cmpFileList   = nullptr;
    QComboBox     *m_cmpTimeCombo  = nullptr;
    QTableWidget  *m_mapTable      = nullptr;   // 映射表
    QTableWidget  *m_resultTable   = nullptr;   // 结果表
    QLineEdit     *m_sceneNameEdit = nullptr;   // 场景名
    QLineEdit     *m_sceneStartEdit = nullptr;  // 起始(秒)
    QLineEdit     *m_sceneEndEdit   = nullptr;  // 结束(秒)
    QListWidget   *m_sceneList     = nullptr;   // 场景列表
    QPushButton   *m_startBtn      = nullptr;
    QProgressBar  *m_progressBar   = nullptr;

    void buildUi();
    void setBusy(bool busy);
    void startLoadTruth(const QString &path);
    void onTruthLoadFinished();
    void startLoadCmpFiles(const QStringList &paths);
    void onCmpLoadFinished();
    void refreshTruthTimeCombo();
    void refreshCmpFileList();
    void refreshCmpTimeCombo();
    void rebuildMappingTable();
    void rebuildResultTable();
    void refreshSceneList();
};

} // namespace Analysis

#endif // ANALYSIS_ANALYSISPANEL_H
