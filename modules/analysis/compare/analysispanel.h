#ifndef ANALYSIS_ANALYSISPANEL_H
#define ANALYSIS_ANALYSISPANEL_H

#include <QWidget>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

#include <QtCharts/QChartView>

#include "../raw/truthreader.h"   // TruthSummary / TruthReader

class QTableWidget;
class QListWidget;
class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QDoubleSpinBox;
class QFutureWatcherBase;

namespace Analysis {

// 时间标准：GPS 周内秒 / UTC 周内秒（两者相差闰秒，对齐前统一换算到 GPS）
enum class TimeStandard { Gps = 0, Utc = 1 };

// 真值物理量（对比项的可选字段，来自真值 GINS 输出）
enum class TruthField {
    Lat = 0, Lon, Height,
    Roll, Pitch, Heading,
    VelN, VelE, VelU
};

// ════════════════════════════════════════════════════════════════
//  对比分析面板
//  以「真值文件（GINS）」为基准，导入对比文件（CSV/TXT），手动选择
//  时间标准与列映射，时间戳精确对齐（完全相等才配对），按场景分别
//  计算均值偏置 / RMS / 2σ，并输出误差曲线。
// ════════════════════════════════════════════════════════════════
class AnalysisPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AnalysisPanel(QWidget *parent = nullptr);
    ~AnalysisPanel() override;

private slots:
    void onBrowseTruth();
    void onTruthStandardChanged(int idx);
    void onAddCmpFile();
    void onRemoveCmpFile();
    void onCmpFileSelected(int row);
    void onCmpTimeColChanged(int idx);
    void onCmpDayColChanged(int idx);
    void onCmpStandardChanged(int idx);
    void onAddMetric();
    void onRemoveMetric();
    void onAddScene();
    void onRemoveScene();
    void onStartCompare();
    void onCompareFinished();

private:
    // 对比文件源：只缓存表头 + 时间列，数据列在分析时按需流式读取（避免全量缓存卡机）
    struct CmpSource {
        QString         path;
        QStringList     header;
        int             dataStart  = 0;              // 数据起始行（0-based）
        int             timeColumn = -1;             // 秒/毫秒列
        int             timeDayColumn = -1;          // 天数列（天内秒模式；-1 = 周内秒模式）
        bool            milliseconds = false;        // 时间列单位：true=毫秒，false=秒
        int             speedColumn = -1;            // 前向速度列（派生 VelN/VelE 用）
        int             dirColumn   = -1;            // 速度方向列（deg，派生 VelN/VelE 用）
        TimeStandard    timeStandard = TimeStandard::Gps;
        QVector<double> timeRaw;                     // 秒/毫秒列原始值（未换算）
        QVector<double> timeDayRaw;                  // 天数列原始值（天内秒模式）
        QVector<double> timeGps;                     // 换算到 GPS 周内秒的时间列
    };

    // 真值源（复用原始分析 TruthReader 自动解析）
    struct TruthSource {
        QString       path;
        TruthSummary  summary;
        TimeStandard  timeStandard = TimeStandard::Gps;
        double        heightAnomaly = 0.0;           // 高程异常（默认 0，可设置）
        QVector<double> timeGps;                     // 换算到 GPS 周内秒
    };

    // 对比项：真值字段 ↔ 每个对比文件对应的列
    struct MetricMapping {
        QString     name;
        TruthField  field = TruthField::Lat;
        bool        subtractMean = false;
        QVector<int> cmpCols;                        // 与 m_cmpFiles 一一对应，-1 未映射
    };

    struct SceneSegment {
        QString name;
        double  startSec = 0;
        double  endSec   = 0;
    };

    // 后台计算结果（表格行 + 误差曲线数据）
    struct CompareRow {
        QString scene, metric, file;
        int     n = 0;
        double  mean = 0, rms = 0, twoSigma = 0;
    };
    struct ErrorSeries {
        QString         label;
        QVector<double> time;
        QVector<double> error;
    };
    struct CompareOutcome {
        bool ok = false;
        QString err;
        QVector<CompareRow>   rows;
        QVector<ErrorSeries>  series;
    };

    // 数据
    TruthSource          m_truth;
    QList<CmpSource>     m_cmpFiles;
    QList<MetricMapping> m_mappings;
    QList<SceneSegment>  m_scenes;
    bool                 m_loading = false;
    QString              m_lastDir;
    QString              m_pendingTruthPath;         // 真值加载中暂存路径，完成后写回 m_truth.path
    double               m_leapSeconds = 18.0;       // 闰秒（可配置，默认 18）

    // 异步
    QFutureWatcherBase *m_watcher = nullptr;
    QList<QWidget*>     m_busyWidgets;

    // 控件
    QLineEdit      *m_truthPathEdit    = nullptr;
    QComboBox      *m_truthStandardCombo = nullptr;
    QDoubleSpinBox *m_heightAnomalySpin = nullptr;
    QLabel         *m_truthInfoLabel   = nullptr;

    QListWidget    *m_cmpFileList      = nullptr;
    QComboBox      *m_cmpTimeCombo     = nullptr;
    QComboBox      *m_cmpDayCombo      = nullptr;
    QComboBox      *m_cmpStandardCombo = nullptr;
    QLabel         *m_alignInfoLabel   = nullptr;

    QTableWidget   *m_mapTable         = nullptr;
    QTableWidget   *m_resultTable      = nullptr;

    QLineEdit      *m_sceneNameEdit    = nullptr;
    QLineEdit      *m_sceneStartEdit   = nullptr;
    QLineEdit      *m_sceneEndEdit     = nullptr;
    QListWidget    *m_sceneList        = nullptr;
    QPushButton    *m_startBtn         = nullptr;
    QProgressBar   *m_progressBar      = nullptr;
    QDoubleSpinBox *m_leapSpin         = nullptr;

    // 误差曲线
    QChartView *m_errorChartView = nullptr;

    void buildUi();
    void setBusy(bool busy);

    void startLoadTruth(const QString &path);
    void onTruthLoadFinished();
    void startLoadCmpFiles(const QStringList &paths);
    void onCmpLoadFinished();

    void refreshTruthInfo();
    void recomputeTruthTimeGps();
    void refreshCmpFileList();
    void refreshCmpTimeCombo();
    void refreshCmpStandardCombo();
    void recomputeCmpTimeGps(int fileIdx);
    void updateAlignStatus();
    void rebuildMappingTable();
    void rebuildResultTable();
    void refreshSceneList();
    void clearErrorChart();
    void showErrorChart(const QVector<ErrorSeries> &series);

    // 辅助
    static QString truthFieldName(TruthField f);
    static QString truthFieldUnit(TruthField f);
    static QVector<double> truthFieldValues(const TruthSummary &s, TruthField f);
    static double toGpsTime(double raw, TimeStandard std, double leap);
    static double daySodToGpsWeekSec(double day, double sod, TimeStandard std, double leap);
    static bool readCmpHeader(const QString &path, QStringList *header, int *dataStart, QString *err);
    static bool readCmpColumn(const QString &path, int dataStart, int col, QVector<double> *out, QString *err);
};

} // namespace Analysis

#endif // ANALYSIS_ANALYSISPANEL_H
