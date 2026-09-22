#ifndef ANALYSIS_RAW_RAWANALYSISPANEL_H
#define ANALYSIS_RAW_RAWANALYSISPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPointF>
#include <QColor>
#include <QStringList>
#include <QFutureWatcher>
#include <functional>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

#include "rinexreader.h"
#include "imureader.h"
#include "truthreader.h"

class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QSplitter;
class QScrollBar;
class QTimer;
class QScatterSeries;

namespace Analysis {

// 数据类型
enum class DataType {
    Gnss  = 0,   // GNSS 原始观测值（RINEX）
    Imu   = 1,   // IMU 原始观测值（CSV）
    Truth = 2    // 真值（GINS 后处理输出）
};

// 分析结果封装（用于后台线程返回）
struct AnalyzeOutcome {
    bool ok = false;
    DataType dataType = DataType::Gnss;
    RinexSummary summary;   // GNSS 结果
    ImuSummary imu;         // IMU 结果
    TruthSummary truth;     // 真值结果
    QString err;
};

// ════════════════════════════════════════════════════════════════
//  可缩放的图表视图（滚轮仅缩放 X 轴时间轴，平移由外部横向滚动条完成）
// ════════════════════════════════════════════════════════════════
class ZoomableChartView : public QChartView
{
    Q_OBJECT
public:
    explicit ZoomableChartView(QChart *chart, QValueAxis *xAxis, QValueAxis *yAxis = nullptr, QWidget *parent = nullptr)
        : QChartView(chart, parent), m_xAxis(xAxis), m_yAxis(yAxis) { /* 关闭抗锯齿，降低大数据量绘制开销 */ }

    // 地图模式：X/Y 同步等比缩放，保持轨迹几何比例（默认关闭，仅轨迹图开启）
    void setLinkedZoom(bool on) { m_linkedZoom = on; }

    // 左键点击回调：回传点击位置对应的图表坐标值（仅轨迹图使用）
    void setClickHandler(std::function<void(const QPointF &)> fn) { m_clickHandler = std::move(fn); }

protected:
    void mousePressEvent(QMouseEvent *ev) override {
        if (m_clickHandler && ev->button() == Qt::LeftButton)
            m_clickHandler(chart()->mapToValue(ev->pos()));
        QChartView::mousePressEvent(ev);
    }

    void wheelEvent(QWheelEvent *ev) override {
        if (ev->angleDelta().y() == 0) return;
        const double factor = ev->angleDelta().y() > 0 ? 0.85 : 1.15;

        if (m_linkedZoom && m_xAxis && m_yAxis) {
            const double xc = (m_xAxis->min() + m_xAxis->max()) / 2.0;
            const double xh = (m_xAxis->max() - m_xAxis->min()) * factor / 2.0;
            const double yc = (m_yAxis->min() + m_yAxis->max()) / 2.0;
            const double yh = (m_yAxis->max() - m_yAxis->min()) * factor / 2.0;
            m_xAxis->setRange(xc - xh, xc + xh);
            m_yAxis->setRange(yc - yh, yc + yh);
            ev->accept();
            return;
        }

        // 默认滚轮缩放 X 轴（时间轴）；按住 Ctrl 滚轮缩放 Y 轴
        QValueAxis *axis = (ev->modifiers() & Qt::ControlModifier) ? m_yAxis : m_xAxis;
        if (!axis) axis = m_xAxis;
        if (!axis) return;
        const double lo = axis->min();
        const double hi = axis->max();
        const double center = (lo + hi) / 2.0;
        const double half = (hi - lo) * factor / 2.0;
        axis->setRange(center - half, center + half);
        ev->accept();
    }

private:
    QValueAxis *m_xAxis = nullptr;
    QValueAxis *m_yAxis = nullptr;
    bool m_linkedZoom = false;
    std::function<void(const QPointF &)> m_clickHandler;
};

// ════════════════════════════════════════════════════════════════
//  原始数据分析面板（重构版）
//  左侧：数据类型选择 ComboBox + 可展开树形导航
//  右侧：检测报告 / 卫星数曲线 / SNR曲线（QStackedWidget 切换）
// ════════════════════════════════════════════════════════════════
class RawAnalysisPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RawAnalysisPanel(QWidget *parent = nullptr);

private slots:
    void onBrowse();
    void onAnalyze();
    void onAnalyzeFinished();
    void onTypeChanged(int index);
    void onTreeNodeClicked(QTreeWidgetItem *item, int /*column*/);
    void onTrajPlayClicked();
    void onTrajPlayTick();

private:
    // ── 左侧导航 ──
    QComboBox   *m_typeCombo = nullptr;
    QTreeWidget *m_tree      = nullptr;

    // ── 右侧内容 ──
    QLineEdit     *m_pathEdit      = nullptr;
    QPushButton   *m_browseBtn     = nullptr;
    QPushButton   *m_analyzeBtn    = nullptr;
    QStackedWidget *m_contentStack = nullptr;

    // 检测报告页
    QLabel *m_reportLabel = nullptr;

    // 卫星数曲线页（图表 + 占位，用垂直 splitter 可拖拽调整图表高度）
    QWidget   *m_satPage    = nullptr;
    QSplitter *m_satSplitter = nullptr;

    // SNR 曲线页（图表 + 占位，用垂直 splitter 可拖拽调整图表高度）
    QWidget   *m_snrPage    = nullptr;
    QSplitter *m_snrSplitter = nullptr;

    // IMU 加计曲线页 / 陀螺曲线页 / 模值曲线页
    QWidget   *m_imuAccPage     = nullptr;
    QSplitter *m_imuAccSplitter = nullptr;
    QWidget   *m_imuGyrPage     = nullptr;
    QSplitter *m_imuGyrSplitter = nullptr;
    QWidget   *m_imuMagPage     = nullptr;
    QSplitter *m_imuMagSplitter = nullptr;

    // 真值曲线页：轨迹 / 速度 / 位置高程 / 姿态
    QWidget   *m_truthTrajPage     = nullptr;
    QSplitter *m_truthTrajSplitter = nullptr;
    QWidget   *m_truthVelPage      = nullptr;
    QSplitter *m_truthVelSplitter  = nullptr;
    QWidget   *m_truthPosPage      = nullptr;
    QSplitter *m_truthPosSplitter  = nullptr;
    QWidget   *m_truthAttPage      = nullptr;
    QSplitter *m_truthAttSplitter  = nullptr;
    QWidget   *m_truthSatPage      = nullptr;
    QSplitter *m_truthSatSplitter  = nullptr;

    // ── 后台分析 ──
    DataType m_currentType = DataType::Gnss;
    QFutureWatcher<AnalyzeOutcome> *m_watcher = nullptr;
    AnalyzeOutcome m_outcome;

    // ── 轨迹播放 ──
    QTimer          *m_trajPlayTimer = nullptr;   // 播放定时器
    QScatterSeries  *m_trajPlayDot   = nullptr;   // 绿色起点/播放点（沿轨迹移动）
    QVector<QPointF> m_trajPath;                  // 投影后的轨迹点（东/北，米）
    QVector<double>  m_trajTimes;                 // 轨迹点对应的 UTC 周内秒（与 m_trajPath 一一对应）
    double           m_trajPlayPos   = 0.0;       // 当前播放位置（浮点索引）
    double           m_trajDuration  = 0.0;       // 数据总时长（秒）

    void buildUi();
    void rebuildTree();
    void resetResults();
    void showReport(const RinexSummary &s);
    void showSatelliteChart(const RinexSummary &s);
    void showSnrChart(const RinexSummary &s);
    void buildSatChart(const RinexSummary &s);
    void buildSnrChart(const RinexSummary &s);
    void showImuReport(const ImuSummary &s);
    void buildImuAccChart(const ImuSummary &s);
    void buildImuGyrChart(const ImuSummary &s);
    void buildImuMagChart(const ImuSummary &s);
    void buildImuTriChart(QSplitter *splitter, const ImuSummary &s,
                          const QString &title, const QString &yTitle, bool isAccel);
    void showTruthReport(const TruthSummary &s);
    void buildTruthTrajChart(const TruthSummary &s);
    void stopTrajPlay();
    void buildTruthVelChart(const TruthSummary &s);
    void buildTruthPosChart(const TruthSummary &s);
    void buildTruthAttChart(const TruthSummary &s);
    void buildTruthSatChart(const TruthSummary &s);
    void buildTruthTimeChart(QSplitter *splitter, const QString &title,
                             const QString &yTitle, const QStringList &labels,
                             const QVector<QColor> &colors,
                             const QVector<QVector<QPointF>> &seriesData);
    void buildTruthStackedCharts(QSplitter *splitter, const QStringList &titles,
                                 const QStringList &yTitles, const QStringList &names,
                                 const QVector<QColor> &colors,
                                 const QVector<QVector<QPointF>> &data,
                                 const QVector<int> &heights = {});
    QVector<TruthSample> downSample(const QVector<TruthSample> &samples, double targetHz) const;
    void attachTimeScrollBar(QValueAxis *axisX, QScrollBar *bar);
    void attachYScrollBar(QValueAxis *axisY, QScrollBar *bar);
    void clearChartPage(QSplitter *splitter);
};

} // namespace Analysis

#endif // ANALYSIS_RAW_RAWANALYSISPANEL_H
