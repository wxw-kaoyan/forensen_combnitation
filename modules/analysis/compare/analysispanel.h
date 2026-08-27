#ifndef ANALYSIS_ANALYSISPANEL_H
#define ANALYSIS_ANALYSISPANEL_H

#include <QWidget>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

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

// 时间格式：用户手动指定（自己看数据、自己选表头），统一换算到 UTC 绝对秒（Unix 纪元）
enum class TimeFormat {
    GpsSow     = 0,  // GPS 周内秒（单列，秒）
    GpsSowMs   = 1,  // GPS 周内毫秒（单列，毫秒）
    GpsWeekSow = 2,  // GPS 周 + 周内秒（两列：周数、周内秒）
    GpsDaySod  = 3,  // GPS 天内秒（两列：周内天 0~6、天内秒）
    UtcDaySod  = 4,  // UTC 天数 + 天内秒（两列：绝对天数、天内秒）
    GpsAbsSec  = 5,  // GPS 绝对秒（单列，自 1980-01-06 起连续秒）
    UtcAbsSec  = 6,  // UTC 绝对秒（单列，Unix 纪元 1970-01-01 起连续秒）
    UtcSow     = 7,  // UTC 周内秒（单列，秒）
    UtcWeekSow = 8,  // UTC 周 + 周内秒（两列：周数、周内秒）
    GpsWeekSowMs = 9,  // GPS 周 + 周内毫秒（两列：周数、周内毫秒）
};

// 场景起止时间标准：UTC 天内秒 / GPS 天内秒（两者相差闰秒，基准不同）
enum class SceneTimeStd { UtcDay = 0, GpsDay = 1 };

// 对比指标：逐项计算「对比值 - 真值」的误差（误差曲线/差值图）
enum class MetricType {
    HorizPos = 0,  // 水平位置误差 (m)：经纬度投影到局部切平面后做差，对比文件需 纬度列 + 经度列
    Height,        // 高程误差 (m)：单列
    HorizVel,      // 水平速度误差 (m/s)：水平速度 sqrt(VN²+VE²) 后做差，对比文件需 北向列 + 东向列
    VertVel,       // 天向速度误差 (m/s)：单列
    Roll,          // 横滚 (deg)：单列
    Pitch,         // 俯仰 (deg)：单列
    Heading,       // 航向 (deg)：单列
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
    void onTruthFormatChanged(int idx);
    void onTruthWeekChanged(double v);
    void onAddCmpFile();
    void onRemoveCmpFile();
    void onCmpFileSelected(int row);
    void onCmpTimeColChanged(int idx);
    void onCmpDayColChanged(int idx);
    void onCmpFormatChanged(int idx);
    void onCmpWeekChanged(double v);
    void onAddMetric();
    void onRemoveMetric();
    void onAddScene();
    void onRemoveScene();
    void onStartCompare();
    void onCompareFinished();
    void onResultRowDoubleClicked(int row, int column);

private:
    // 对比文件源：只缓存表头 + 时间列，数据列在分析时按需流式读取（避免全量缓存卡机）
    struct CmpSource {
        QString         path;
        QStringList     header;
        int             dataStart  = 0;              // 数据起始行（0-based）
        int             timeColumn = -1;             // 主时间列（秒/毫秒列）
        int             timeDayColumn = -1;          // 第二时间列（周数列/天数列；-1 = 单列格式）
        bool            milliseconds = false;        // 时间列单位：true=毫秒，false=秒
        int             speedColumn = -1;            // 前向速度列（派生 VelN/VelE 用）
        int             dirColumn   = -1;            // 速度方向列（deg，派生 VelN/VelE 用）
        TimeFormat      timeFormat = TimeFormat::GpsSow;  // 用户指定的时间格式
        double          weekNum = 0;                 // GPS 周数（周内秒/毫秒/天内秒格式需要）
        QVector<double> timeRaw;                     // 主时间列原始值（秒/毫秒，未换算）
        QVector<double> timeDayRaw;                  // 第二时间列原始值（周数/天数）
        QVector<double> timeUtc;                     // 统一换算后的 UTC 绝对秒（Unix 纪元）
    };

    // 真值源（复用原始分析 TruthReader 自动解析）
    struct TruthSource {
        QString       path;
        TruthSummary  summary;
        TimeFormat    timeFormat = TimeFormat::GpsSow;  // 真值时间格式
        double        weekNum = 0;                   // GPS 周数（周内秒格式需要）
        double        heightAnomaly = 0.0;           // 高程异常（默认 0，可设置）
        QVector<double> timeUtc;                     // 统一换算后的 UTC 绝对秒
    };

    // 对比项：指标类型 ↔ 每个对比文件对应的列（复合指标用两列）
    struct MetricMapping {
        QString     name;
        MetricType  type = MetricType::HorizPos;
        bool        subtractMean = false;
        struct FileCols { int primary = -1; int secondary = -1; };  // 复合指标两列，单列指标只用 primary
        QVector<FileCols> cmpCols;                   // 与 m_cmpFiles 一一对应
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
    struct CurveSeries {
        QString         metric;    // 指标名
        QString         file;      // 对比文件名
        QString         unit;      // 单位（deg/m/m/s）
        QVector<double> time;      // 时间轴（UTC 绝对秒）
        QVector<double> error;     // 误差（对比值 - 真值）
    };
    struct CompareOutcome {
        bool ok = false;
        QString err;
        QVector<CompareRow>   rows;
        QVector<CurveSeries>  series;
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
    SceneTimeStd         m_sceneStd = SceneTimeStd::UtcDay;  // 场景起止时间标准
    QVector<CurveSeries> m_curSeries;                // 最近一次分析的曲线数据（点击结果行时按需弹出）

    // 异步
    QFutureWatcherBase *m_watcher = nullptr;
    QList<QWidget*>     m_busyWidgets;

    // 控件
    QLineEdit      *m_truthPathEdit    = nullptr;
    QComboBox      *m_truthFormatCombo = nullptr;
    QDoubleSpinBox *m_truthWeekSpin    = nullptr;
    QDoubleSpinBox *m_heightAnomalySpin = nullptr;
    QLabel         *m_truthInfoLabel   = nullptr;

    QListWidget    *m_cmpFileList      = nullptr;
    QComboBox      *m_cmpTimeCombo     = nullptr;
    QComboBox      *m_cmpDayCombo      = nullptr;
    QComboBox      *m_cmpFormatCombo   = nullptr;
    QDoubleSpinBox *m_cmpWeekSpin      = nullptr;
    QLabel         *m_alignInfoLabel   = nullptr;

    QTableWidget   *m_mapTable         = nullptr;
    QTableWidget   *m_resultTable      = nullptr;

    QLineEdit      *m_sceneNameEdit    = nullptr;
    QLineEdit      *m_sceneStartEdit   = nullptr;
    QLineEdit      *m_sceneEndEdit     = nullptr;
    QComboBox      *m_sceneStdCombo    = nullptr;
    QListWidget    *m_sceneList        = nullptr;
    QPushButton    *m_startBtn         = nullptr;
    QProgressBar   *m_progressBar      = nullptr;
    QDoubleSpinBox *m_leapSpin         = nullptr;

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
    void refreshCmpFormatCombo();
    void recomputeCmpTimeGps(int fileIdx);
    void updateAlignStatus();
    void rebuildMappingTable();
    void rebuildResultTable();
    void refreshSceneList();
    double sceneDayBase() const;
    void showCurveWindow(const CurveSeries &s);

    // 辅助
    static QString metricName(MetricType t);
    static QString metricUnit(MetricType t);
    static bool metricNeedsTwoCols(MetricType t);
    static double toUtcAbsSec(TimeFormat fmt, double colA, double colB, double week, double leap);
    static bool readCmpHeader(const QString &path, QStringList *header, int *dataStart, QString *err);
    static bool readCmpColumn(const QString &path, int dataStart, int col, QVector<double> *out, QString *err);
};

} // namespace Analysis

#endif // ANALYSIS_ANALYSISPANEL_H
