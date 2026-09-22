#include "analysispanel.h"
#include "comparechartwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QProgressBar>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QGuiApplication>
#include <QApplication>
#include <QTableWidgetItem>
#include <QDoubleSpinBox>
#include <QSplitter>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#include <cmath>
#include <algorithm>
#include <limits>
#include <QtMath>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  通用工具（分隔符 / 切分 / 时间列识别 / 数值格式化 / 精确时间匹配）
// ════════════════════════════════════════════════════════════════
static QChar detectDelimiter(const QString &line)
{
    int comma = line.count(',');
    int tab   = line.count('\t');
    int semi  = line.count(';');
    if (tab > 0 && tab >= comma && tab >= semi) return '\t';
    if (comma > 0 && comma >= semi) return ',';
    if (semi > 0) return ';';
    return ' ';  // 默认按空白分割
}

static QStringList splitFields(const QString &line, QChar delim)
{
    if (delim == ' ' || delim == '\t') {
        static const QRegularExpression ws(QStringLiteral("\\s+"));
        return line.trimmed().split(ws, Qt::SkipEmptyParts);
    }
    return line.split(delim);
}

// 手动校验字节序列是否为合法 UTF-8（GBK 等本地编码的高位字节会命中非法序列，从而正确判定为非 UTF-8）
static bool isUtf8(const QByteArray &b)
{
    int i = 0;
    const int n = b.size();
    while (i < n) {
        const uchar c = static_cast<uchar>(b[i]);
        int len;
        if (c < 0x80) { ++i; continue; }
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        else return false;                        // 非法 UTF-8 起始字节
        if (i + len > n) return true;             // 末尾截断，视作合法（可能切在多字节字符中间）
        for (int k = 1; k < len; ++k)
            if ((static_cast<uchar>(b[i + k]) & 0xC0) != 0x80)
                return false;                     // 非法继续字节
        i += len;
    }
    return true;
}

// 检测文本文件编码：无 BOM 时按 UTF-8 校验，失败（GBK 等本地编码）回退系统本地编码。
// 对比文件（如 agdebug CSV）常由中文 Windows 工具导出为 GBK，QTextStream 默认 UTF-8 会把中文表头读成乱码，
// 导致「秒/毫秒」等时间列名匹配失败，时间列被误判为第 0 列（GPS周），进而时间对齐对不上。
static QStringConverter::Encoding detectCsvEncoding(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QStringConverter::Utf8;
    const QByteArray head = f.read(8192);
    f.close();

    // UTF-8 / UTF-16 BOM 直接判定
    if (head.startsWith("\xEF\xBB\xBF"))
        return QStringConverter::Utf8;
    if (head.startsWith("\xFF\xFE") || head.startsWith("\xFE\xFF"))
        return QStringConverter::Utf16;

    // 无 BOM：合法 UTF-8 用 UTF-8，否则回退系统本地编码（中文 Windows = GBK/CP936）
    if (isUtf8(head))
        return QStringConverter::Utf8;
    return QStringConverter::System;
}

// ── 时间列识别结果 ──
struct TimeDetect {
    int  timeColumn    = -1;   // 秒/毫秒列
    int  timeDayColumn = -1;   // 天数列（天内秒模式；-1 = 周内秒模式）
    bool milliseconds  = false;// true = 毫秒，false = 秒
};

// 通用时间列识别：支持 周内秒 / 周内毫秒 / 天数+天内秒 等格式
static TimeDetect detectTime(const QStringList &header)
{
    TimeDetect r;

    // 1) 毫秒列（优先）
    for (int i = 0; i < header.size(); ++i) {
        const QString h = header[i].toLower();
        if (h.contains(QStringLiteral("毫秒")) || h.contains(QStringLiteral("ms"))) {
            r.timeColumn = i;
            r.milliseconds = true;
            break;
        }
    }
    // 2) 秒列（周内秒 / 天内秒 / itow / time / sod）
    if (r.timeColumn < 0) {
        for (int i = 0; i < header.size(); ++i) {
            const QString h = header[i].toLower();
            if (h.contains(QStringLiteral("秒")) || h.contains(QStringLiteral("itow")) ||
                h.contains(QStringLiteral("time")) || h.contains(QStringLiteral("sod"))) {
                r.timeColumn = i;
                break;
            }
        }
    }
    // 3) 默认第 0 列（未识别到任何时间列时）
    if (r.timeColumn < 0)
        r.timeColumn = 0;

    // 4) 天内秒 / GPS 周模式：根据时间列名识别第二列
    if (r.timeColumn < header.size()) {
        const QString th = header[r.timeColumn].toLower();
        if (th.contains(QStringLiteral("天内")) || th.contains(QStringLiteral("sod"))) {
            for (int i = 0; i < header.size(); ++i) {
                if (i == r.timeColumn) continue;
                const QString h = header[i].toLower();
                if ((h.contains(QStringLiteral("天")) || h.contains(QStringLiteral("day"))) &&
                    !h.contains(QStringLiteral("秒")) && !h.contains(QStringLiteral("毫秒"))) {
                    r.timeDayColumn = i;
                    break;
                }
            }
        } else if (th.contains(QStringLiteral("周内"))) {
            // GPS 周 + 周内秒/毫秒模式：找 GPS 周数列
            for (int i = 0; i < header.size(); ++i) {
                if (i == r.timeColumn) continue;
                const QString h = header[i].toLower();
                if ((h.contains(QStringLiteral("周")) || h.contains(QStringLiteral("week"))) &&
                    !h.contains(QStringLiteral("周内"))) {
                    r.timeDayColumn = i;
                    break;
                }
            }
        }
    }
    return r;
}

// 识别前向速度列与速度方向列（用于派生 VelN/VelE）。无则对应返回 -1。
static void detectSpeedColumns(const QStringList &header, int *speedCol, int *dirCol)
{
    *speedCol = -1;
    *dirCol   = -1;
    for (int i = 0; i < header.size(); ++i) {
        const QString h = header[i].toLower();
        if (*speedCol < 0 && h.contains(QStringLiteral("速度")) &&
            (h.contains(QStringLiteral("前向")) || h.contains(QStringLiteral("m/s")) ||
             h.contains(QStringLiteral("speed"))))
            *speedCol = i;
        if (*dirCol < 0 && h.contains(QStringLiteral("方向")) &&
            (h.contains(QStringLiteral("速度")) || h.contains(QStringLiteral("dir"))))
            *dirCol = i;
    }
}

static QString fmtNum(double v)
{
    if (!std::isfinite(v)) return QStringLiteral("-");
    return QString::number(v, 'g', 8);
}

// 时间戳精确对齐：两个升序数组完全相等才配对（带 1e-6 秒容差抗浮点噪声）
struct MatchedPair { int aIdx; int bIdx; };

static QVector<MatchedPair> exactMatchTimes(const QVector<double> &a,
                                            const QVector<double> &b)
{
    constexpr double kEps = 1e-6;
    QVector<MatchedPair> res;
    int i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        const double d = a[i] - b[j];
        if (std::abs(d) <= kEps) {
            res.append({i, j});
            ++i; ++j;
        } else if (d < 0) {
            ++i;
        } else {
            ++j;
        }
    }
    return res;
}

// ════════════════════════════════════════════════════════════════
//  静态辅助：真值字段名/单位/取值，时间换算，文件读取
// ════════════════════════════════════════════════════════════════
QString AnalysisPanel::metricName(MetricType t)
{
    switch (t) {
        case MetricType::HorizPos: return QStringLiteral("水平位置误差");
        case MetricType::Height:   return QStringLiteral("高程误差");
        case MetricType::HorizVel: return QStringLiteral("水平速度误差");
        case MetricType::VertVel:  return QStringLiteral("天向速度误差");
        case MetricType::Roll:     return QStringLiteral("横滚(Roll)");
        case MetricType::Pitch:    return QStringLiteral("俯仰(Pitch)");
        case MetricType::Heading:  return QStringLiteral("航向(Heading)");
    }
    return QString();
}

QString AnalysisPanel::metricUnit(MetricType t)
{
    switch (t) {
        case MetricType::HorizPos:
        case MetricType::Height:   return QStringLiteral("m");
        case MetricType::HorizVel:
        case MetricType::VertVel:  return QStringLiteral("m/s");
        case MetricType::Roll:
        case MetricType::Pitch:
        case MetricType::Heading:  return QStringLiteral("deg");
    }
    return QString();
}

bool AnalysisPanel::metricNeedsTwoCols(MetricType t)
{
    return t == MetricType::HorizPos || t == MetricType::HorizVel;
}

// 统一换算：把各种时间格式换算到 UTC 绝对秒（Unix 纪元 1970-01-01 起，跨周连续）。
// colA = 主时间列（秒/毫秒），colB = 第二时间列（周数/天数，单列格式忽略），
// week = GPS 周数（周内秒/毫秒/天内秒格式需要），leap = 闰秒（GPS 领先 UTC 的秒数）。
double AnalysisPanel::toUtcAbsSec(TimeFormat fmt, double colA, double colB, double week, double leap)
{
    constexpr double GPS_EPOCH_UNIX = 315964800.0;   // 1980-01-06 00:00:00 UTC 的 Unix 秒
    switch (fmt) {
    case TimeFormat::GpsSow:      // colA=周内秒
        return week * 604800.0 + colA + GPS_EPOCH_UNIX - leap;
    case TimeFormat::GpsSowMs:    // colA=周内毫秒
        return week * 604800.0 + colA / 1000.0 + GPS_EPOCH_UNIX - leap;
    case TimeFormat::GpsWeekSow:  // colA=周内秒, colB=周数
        return colB * 604800.0 + colA + GPS_EPOCH_UNIX - leap;
    case TimeFormat::GpsDaySod:   // colA=天内秒, colB=周内天(0~6)
        return week * 604800.0 + colB * 86400.0 + colA + GPS_EPOCH_UNIX - leap;
    case TimeFormat::UtcDaySod:   // colA=天内秒, colB=绝对天数(Unix 纪元)
        return colB * 86400.0 + colA;
    case TimeFormat::GpsAbsSec:   // colA=GPS 绝对秒（1980-01-06 起连续秒）
        return colA + GPS_EPOCH_UNIX - leap;
    case TimeFormat::UtcAbsSec:   // colA=UTC 绝对秒（Unix 纪元）
        return colA;
    case TimeFormat::UtcSow:      // colA=UTC 周内秒（已是 UTC，无需减闰秒）
        return week * 604800.0 + colA + GPS_EPOCH_UNIX;
    case TimeFormat::UtcWeekSow:  // colA=UTC 周内秒, colB=UTC 周数
        return colB * 604800.0 + colA + GPS_EPOCH_UNIX;
    case TimeFormat::GpsWeekSowMs:  // colA=周内毫秒, colB=GPS 周数
        return colB * 604800.0 + colA / 1000.0 + GPS_EPOCH_UNIX - leap;
    }
    return 0.0;
}

// 该格式是否缺少周数，需要用户额外填 GPS 周数
static bool fmtNeedsWeek(TimeFormat fmt)
{
    return fmt == TimeFormat::GpsSow || fmt == TimeFormat::GpsSowMs ||
           fmt == TimeFormat::GpsDaySod || fmt == TimeFormat::UtcSow;
}

bool AnalysisPanel::readCmpHeader(const QString &path, QStringList *header, int *dataStart, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法打开文件");
        return false;
    }
    QTextStream ts(&f);
    ts.setEncoding(detectCsvEncoding(path));

    // 流式定位表头行（不 readAll，避免 100MB+ 大文件整体读入内存卡机）
    // 对比文件为 CSV/TXT：首个非空行即表头
    QString headerLine;
    int headerIdx = -1;          // 表头行号（0-based）
    int lineNo = 0;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (!line.trimmed().isEmpty()) {
            headerLine = line;
            headerIdx = lineNo;
            break;
        }
        ++lineNo;
    }

    if (headerIdx < 0) { if (err) *err = QStringLiteral("空文件"); return false; }

    const QChar delim = detectDelimiter(headerLine);
    const QStringList hdr = splitFields(headerLine, delim);
    if (hdr.isEmpty()) { if (err) *err = QStringLiteral("无法识别表头"); return false; }

    // 数据起始行：表头下一行；若是单位行（首字段以 '(' 开头）再跳一行
    int ds = headerIdx + 1;
    if (!ts.atEnd()) {
        const QString nextLine = ts.readLine();
        const QStringList unitFields = splitFields(nextLine, delim);
        if (!unitFields.isEmpty() && unitFields.first().startsWith('('))
            ++ds;
    }
    f.close();

    if (header) *header = hdr;
    if (dataStart) *dataStart = ds;
    if (err) err->clear();
    return true;
}

bool AnalysisPanel::readCmpColumn(const QString &path, int dataStart, int col,
                                  QVector<double> *out, QString *err)
{
    if (!out || col < 0) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法打开文件");
        return false;
    }
    QTextStream ts(&f);
    ts.setEncoding(detectCsvEncoding(path));
    out->clear();

    QChar delim('\0');
    int lineNo = 0;
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (lineNo < dataStart) { ++lineNo; continue; }
        ++lineNo;
        if (line.isEmpty()) continue;
        if (delim == '\0') delim = detectDelimiter(line);   // 用首个数据行推断分隔符
        const QStringList cols = splitFields(line, delim);
        if (col < cols.size()) {
            bool ok = false;
            const double v = cols[col].toDouble(&ok);
            // 无效值（N/A、空、非数字）记 NaN，统计时跳过，避免被当成 0 拉大误差
            out->append(ok ? v : std::numeric_limits<double>::quiet_NaN());
        } else {
            out->append(std::numeric_limits<double>::quiet_NaN());
        }
    }
    f.close();
    if (err) err->clear();
    return true;
}

// ════════════════════════════════════════════════════════════════
//  面板
// ════════════════════════════════════════════════════════════════
AnalysisPanel::AnalysisPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

AnalysisPanel::~AnalysisPanel() = default;

void AnalysisPanel::buildUi()
{
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // 用 QSplitter 分隔左右，默认各占一半，用户可拖动调整
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // ── 左侧：真值文件 + 对比文件 + 映射 + 闰秒 ──
    auto *left = new QFrame(splitter);
    left->setFrameShape(QFrame::StyledPanel);
    auto *ll = new QVBoxLayout(left);
    ll->setSpacing(6);

    auto addSectionLabel = [&](const QString &s) {
        auto *lbl = new QLabel(s, left);
        lbl->setStyleSheet(QStringLiteral("font-size:13px;font-weight:700;color:#12263f;"));
        ll->addWidget(lbl);
    };

    // 真值文件
    addSectionLabel(QStringLiteral("真值文件（GINS 基准）"));
    auto *truthRow = new QHBoxLayout;
    m_truthPathEdit = new QLineEdit(left);
    m_truthPathEdit->setPlaceholderText(QStringLiteral("选择真值文件（自动识别列）"));
    auto *browseBtn = new QPushButton(QStringLiteral("浏览"), left);
    browseBtn->setFixedWidth(72);
    truthRow->addWidget(m_truthPathEdit, 1);
    truthRow->addWidget(browseBtn);
    ll->addLayout(truthRow);

    auto *truthOptForm = new QFormLayout;
    m_truthFormatCombo = new QComboBox(left);
    m_truthFormatCombo->addItem(QStringLiteral("GPS 周内秒"));
    m_truthFormatCombo->addItem(QStringLiteral("GPS 周内毫秒"));
    m_truthFormatCombo->addItem(QStringLiteral("GPS 周 + 周内秒"));
    m_truthFormatCombo->addItem(QStringLiteral("GPS 天内秒"));
    m_truthFormatCombo->addItem(QStringLiteral("UTC 天数 + 天内秒"));
    m_truthFormatCombo->addItem(QStringLiteral("GPS 绝对秒"));
    m_truthFormatCombo->addItem(QStringLiteral("UTC 绝对秒"));
    m_truthFormatCombo->addItem(QStringLiteral("UTC 周内秒"));
    m_truthFormatCombo->addItem(QStringLiteral("UTC 周 + 周内秒"));
    m_truthFormatCombo->addItem(QStringLiteral("GPS 周 + 周内毫秒"));
    truthOptForm->addRow(QStringLiteral("时间格式"), m_truthFormatCombo);

    m_truthWeekSpin = new QDoubleSpinBox(left);
    m_truthWeekSpin->setRange(0.0, 100000.0);
    m_truthWeekSpin->setDecimals(0);
    m_truthWeekSpin->setValue(0.0);
    truthOptForm->addRow(QStringLiteral("GPS 周数"), m_truthWeekSpin);

    m_heightAnomalySpin = new QDoubleSpinBox(left);
    m_heightAnomalySpin->setRange(-100000.0, 100000.0);
    m_heightAnomalySpin->setDecimals(3);
    m_heightAnomalySpin->setValue(0.0);
    m_heightAnomalySpin->setSuffix(QStringLiteral(" m"));
    truthOptForm->addRow(QStringLiteral("高程异常"), m_heightAnomalySpin);
    ll->addLayout(truthOptForm);

    m_truthInfoLabel = new QLabel(QStringLiteral("未加载真值"), left);
    m_truthInfoLabel->setWordWrap(true);
    m_truthInfoLabel->setStyleSheet(QStringLiteral("color:#617487;font-size:12px;"));
    ll->addWidget(m_truthInfoLabel);

    auto *sep1 = new QFrame(left);
    sep1->setFrameShape(QFrame::HLine);
    ll->addWidget(sep1);

    // 对比文件
    addSectionLabel(QStringLiteral("对比文件"));
    m_cmpFileList = new QListWidget(left);
    m_cmpFileList->setMinimumHeight(70);
    ll->addWidget(m_cmpFileList, 1);

    auto *cmpBtnRow = new QHBoxLayout;
    auto *addCmpBtn = new QPushButton(QStringLiteral("添加"), left);
    auto *rmCmpBtn  = new QPushButton(QStringLiteral("移除"), left);
    cmpBtnRow->addWidget(addCmpBtn);
    cmpBtnRow->addWidget(rmCmpBtn);
    cmpBtnRow->addStretch();
    ll->addLayout(cmpBtnRow);

    auto *cmpOptForm = new QFormLayout;
    m_cmpFormatCombo = new QComboBox(left);
    m_cmpFormatCombo->addItem(QStringLiteral("GPS 周内秒"));
    m_cmpFormatCombo->addItem(QStringLiteral("GPS 周内毫秒"));
    m_cmpFormatCombo->addItem(QStringLiteral("GPS 周 + 周内秒"));
    m_cmpFormatCombo->addItem(QStringLiteral("GPS 天内秒"));
    m_cmpFormatCombo->addItem(QStringLiteral("UTC 天数 + 天内秒"));
    m_cmpFormatCombo->addItem(QStringLiteral("GPS 绝对秒"));
    m_cmpFormatCombo->addItem(QStringLiteral("UTC 绝对秒"));
    m_cmpFormatCombo->addItem(QStringLiteral("UTC 周内秒"));
    m_cmpFormatCombo->addItem(QStringLiteral("UTC 周 + 周内秒"));
    m_cmpFormatCombo->addItem(QStringLiteral("GPS 周 + 周内毫秒"));
    m_cmpFormatCombo->setEnabled(false);
    cmpOptForm->addRow(QStringLiteral("时间格式"), m_cmpFormatCombo);

    m_cmpTimeCombo = new QComboBox(left);
    m_cmpTimeCombo->setEnabled(false);
    cmpOptForm->addRow(QStringLiteral("时间列(秒/毫秒)"), m_cmpTimeCombo);

    m_cmpDayCombo = new QComboBox(left);
    m_cmpDayCombo->setEnabled(false);
    cmpOptForm->addRow(QStringLiteral("第二列(周/天)"), m_cmpDayCombo);

    m_cmpWeekSpin = new QDoubleSpinBox(left);
    m_cmpWeekSpin->setRange(0.0, 100000.0);
    m_cmpWeekSpin->setDecimals(0);
    m_cmpWeekSpin->setValue(0.0);
    m_cmpWeekSpin->setEnabled(false);
    cmpOptForm->addRow(QStringLiteral("GPS 周数"), m_cmpWeekSpin);
    ll->addLayout(cmpOptForm);

    m_alignInfoLabel = new QLabel(left);
    m_alignInfoLabel->setWordWrap(true);
    m_alignInfoLabel->setStyleSheet(QStringLiteral("color:#617487;font-size:12px;"));
    ll->addWidget(m_alignInfoLabel);

    auto *sep2 = new QFrame(left);
    sep2->setFrameShape(QFrame::HLine);
    ll->addWidget(sep2);

    // 对比项映射
    addSectionLabel(QStringLiteral("对比项映射"));
    m_mapTable = new QTableWidget(left);
    m_mapTable->setMinimumHeight(120);
    ll->addWidget(m_mapTable, 2);

    auto *mapBtnRow = new QHBoxLayout;
    auto *addMetricBtn = new QPushButton(QStringLiteral("添加对比项"), left);
    auto *rmMetricBtn  = new QPushButton(QStringLiteral("删除对比项"), left);
    mapBtnRow->addWidget(addMetricBtn);
    mapBtnRow->addWidget(rmMetricBtn);
    mapBtnRow->addStretch();
    ll->addLayout(mapBtnRow);

    // 闰秒（可配置，默认 18）
    auto *leapForm = new QFormLayout;
    m_leapSpin = new QDoubleSpinBox(left);
    m_leapSpin->setRange(0.0, 60.0);
    m_leapSpin->setDecimals(0);
    m_leapSpin->setValue(m_leapSeconds);
    m_leapSpin->setSuffix(QStringLiteral(" s"));
    leapForm->addRow(QStringLiteral("闰秒(GPS-UTC)"), m_leapSpin);
    ll->addLayout(leapForm);

    // ── 右侧：对比结果表 + 场景分析设置 ──
    auto *right = new QFrame(splitter);
    right->setFrameShape(QFrame::StyledPanel);
    auto *rl = new QVBoxLayout(right);
    rl->setSpacing(6);

    auto addRightSectionLabel = [&](const QString &s) {
        auto *lbl = new QLabel(s, right);
        lbl->setStyleSheet(QStringLiteral("font-size:13px;font-weight:700;color:#12263f;"));
        rl->addWidget(lbl);
    };

    // 对比结果表（上）
    addRightSectionLabel(QStringLiteral("对比结果（RMS / 2σ）"));
    m_resultTable = new QTableWidget(right);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setMinimumHeight(160);
    rl->addWidget(m_resultTable, 1);

    auto *sepR = new QFrame(right);
    sepR->setFrameShape(QFrame::HLine);
    rl->addWidget(sepR);

    // 场景分析设置（下）
    addRightSectionLabel(QStringLiteral("场景分析"));
    auto *sceneForm = new QFormLayout;
    m_sceneNameEdit = new QLineEdit(right);
    m_sceneNameEdit->setPlaceholderText(QStringLiteral("可选"));
    m_sceneStartEdit = new QLineEdit(right);
    m_sceneStartEdit->setPlaceholderText(QStringLiteral("起始(天内秒)"));
    m_sceneEndEdit = new QLineEdit(right);
    m_sceneEndEdit->setPlaceholderText(QStringLiteral("结束(天内秒)"));
    sceneForm->addRow(QStringLiteral("场景名"), m_sceneNameEdit);
    sceneForm->addRow(QStringLiteral("起始(UTC天内秒)"), m_sceneStartEdit);
    sceneForm->addRow(QStringLiteral("结束(UTC天内秒)"), m_sceneEndEdit);
    rl->addLayout(sceneForm);

    auto *sceneBtnRow = new QHBoxLayout;
    auto *addSceneBtn = new QPushButton(QStringLiteral("添加场景"), right);
    auto *rmSceneBtn  = new QPushButton(QStringLiteral("删除场景"), right);
    sceneBtnRow->addWidget(addSceneBtn);
    sceneBtnRow->addWidget(rmSceneBtn);
    sceneBtnRow->addStretch();
    rl->addLayout(sceneBtnRow);

    m_sceneList = new QListWidget(right);
    m_sceneList->setMinimumHeight(56);
    rl->addWidget(m_sceneList, 1);

    m_startBtn = new QPushButton(QStringLiteral("开始对比"), right);
    m_startBtn->setStyleSheet(QStringLiteral("font-weight:600;color:#fff;background-color:#2c7be5;padding:8px;"));
    rl->addWidget(m_startBtn);

    m_progressBar = new QProgressBar(right);
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    rl->addWidget(m_progressBar);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ 500, 500 });   // 默认左右各占一半，可拖动调整

    mainLayout->addWidget(splitter);

    // ── 信号 ──
    connect(browseBtn, &QPushButton::clicked, this, &AnalysisPanel::onBrowseTruth);
    connect(m_truthFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onTruthFormatChanged);
    connect(m_truthWeekSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalysisPanel::onTruthWeekChanged);
    connect(m_heightAnomalySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v) { m_truth.heightAnomaly = v; });
    connect(addCmpBtn, &QPushButton::clicked, this, &AnalysisPanel::onAddCmpFile);
    connect(rmCmpBtn,  &QPushButton::clicked, this, &AnalysisPanel::onRemoveCmpFile);
    connect(m_cmpFileList, &QListWidget::currentRowChanged,
            this, &AnalysisPanel::onCmpFileSelected);
    connect(m_cmpTimeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onCmpTimeColChanged);
    connect(m_cmpDayCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onCmpDayColChanged);
    connect(m_cmpFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onCmpFormatChanged);
    connect(m_cmpWeekSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalysisPanel::onCmpWeekChanged);
    connect(addMetricBtn, &QPushButton::clicked, this, &AnalysisPanel::onAddMetric);
    connect(rmMetricBtn,  &QPushButton::clicked, this, &AnalysisPanel::onRemoveMetric);
    connect(addSceneBtn, &QPushButton::clicked, this, &AnalysisPanel::onAddScene);
    connect(rmSceneBtn,  &QPushButton::clicked, this, &AnalysisPanel::onRemoveScene);
    connect(m_startBtn, &QPushButton::clicked, this, &AnalysisPanel::onStartCompare);
    connect(m_leapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v) {
                m_leapSeconds = v;
                recomputeTruthTimeGps();
                for (int i = 0; i < m_cmpFiles.size(); ++i)
                    recomputeCmpTimeGps(i);
                refreshTruthInfo();
                updateAlignStatus();
            });

    // 忙碌时禁用
    m_busyWidgets = {
        browseBtn, m_truthFormatCombo, m_truthWeekSpin, m_heightAnomalySpin, m_leapSpin,
        addCmpBtn, rmCmpBtn, m_cmpFileList, m_cmpTimeCombo, m_cmpDayCombo,
        m_cmpFormatCombo, m_cmpWeekSpin,
        addMetricBtn, rmMetricBtn, m_mapTable,
        m_sceneNameEdit, m_sceneStartEdit, m_sceneEndEdit,
        addSceneBtn, rmSceneBtn, m_sceneList, m_startBtn
    };

    // 指标名编辑回写
    connect(m_mapTable, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem *it) {
                if (!it || it->column() != 0) return;
                const int r = it->row();
                if (r < 0 || r >= m_mappings.size()) return;
                m_mappings[r].name = it->text();
            });

    // 双击结果表某行：弹出对应对比项曲线窗口（非模态、可缩放）
    connect(m_resultTable, &QTableWidget::cellDoubleClicked,
            this, &AnalysisPanel::onResultRowDoubleClicked);

    rebuildMappingTable();
    rebuildResultTable();
}

// ════════════════════════════════════════════════════════════════
//  真值加载（复用 TruthReader 自动解析）
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onBrowseTruth()
{
    if (m_loading) return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择真值文件"), m_lastDir,
        QStringLiteral("文本文件 (*.csv *.txt *.out);;所有文件 (*)"));
    if (path.isEmpty()) return;
    m_lastDir = QFileInfo(path).absolutePath();
    startLoadTruth(path);
}

void AnalysisPanel::startLoadTruth(const QString &path)
{
    m_pendingTruthPath = path;
    setBusy(true);
    auto *watcher = new QFutureWatcher<TruthSummary>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<TruthSummary>::finished,
            this, &AnalysisPanel::onTruthLoadFinished);
    watcher->setFuture(QtConcurrent::run([path]() -> TruthSummary {
        TruthSummary s;
        QString err;
        if (!TruthReader::analyze(path, &s, &err))
            s.totalSamples = -1;   // 用负数标记失败
        return s;
    }));
}

void AnalysisPanel::onTruthLoadFinished()
{
    auto *watcher = static_cast<QFutureWatcher<TruthSummary>*>(m_watcher);
    TruthSummary s = watcher->result();
    watcher->deleteLater();
    m_watcher = nullptr;
    setBusy(false);

    if (s.totalSamples < 0 || s.samples.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("加载失败"),
                             QStringLiteral("无法解析真值文件（未识别到经纬度等列）。"));
        return;
    }

    m_truth = TruthSource();
    m_truth.path = m_pendingTruthPath;
    m_truth.summary = s;
    m_truth.timeFormat = static_cast<TimeFormat>(m_truthFormatCombo->currentIndex());
    m_truth.weekNum = m_truthWeekSpin->value();
    m_truth.heightAnomaly = m_heightAnomalySpin->value();

    // 真值文件若含 Week 列，自动带入 GPS 周数（避免用户手填），并同步到输入框
    if (s.hasWeek) {
        m_truth.weekNum = s.week;
        m_truthWeekSpin->setValue(s.week);
    }
    recomputeTruthTimeGps();

    m_truthPathEdit->setText(m_truth.path);
    refreshTruthInfo();
    updateAlignStatus();
    rebuildMappingTable();
}

void AnalysisPanel::recomputeTruthTimeGps()
{
    if (m_truth.summary.samples.isEmpty()) {
        m_truth.timeUtc.clear();
        return;
    }
    const TruthSummary &s = m_truth.summary;
    m_truth.timeUtc.resize(s.samples.size());
    for (int i = 0; i < s.samples.size(); ++i)
        m_truth.timeUtc[i] = toUtcAbsSec(m_truth.timeFormat, s.firstTime + s.samples[i].t, 0.0,
                                         m_truth.weekNum, m_leapSeconds);
}

void AnalysisPanel::refreshTruthInfo()
{
    if (m_truth.summary.samples.isEmpty()) {
        m_truthInfoLabel->setText(QStringLiteral("未加载真值"));
        return;
    }
    const TruthSummary &s = m_truth.summary;
    m_truthInfoLabel->setText(
        QStringLiteral("样本 %1，时长 %2s，采样率 %3Hz；"
                       "时间范围 %4 ~ %5（UTC 绝对秒）")
            .arg(s.totalSamples)
            .arg(s.durationSec, 0, 'f', 1)
            .arg(s.sampleRate, 0, 'f', 2)
            .arg(m_truth.timeUtc.isEmpty() ? 0.0 : m_truth.timeUtc.first(), 0, 'f', 3)
            .arg(m_truth.timeUtc.isEmpty() ? 0.0 : m_truth.timeUtc.last(), 0, 'f', 3));
}

void AnalysisPanel::onTruthFormatChanged(int idx)
{
    if (m_truth.summary.samples.isEmpty()) return;
    m_truth.timeFormat = static_cast<TimeFormat>(idx);
    recomputeTruthTimeGps();
    refreshTruthInfo();
    updateAlignStatus();
}

void AnalysisPanel::onTruthWeekChanged(double v)
{
    m_truth.weekNum = v;
    recomputeTruthTimeGps();
    refreshTruthInfo();
    updateAlignStatus();
}

void AnalysisPanel::setBusy(bool busy)
{
    m_loading = busy;
    for (QWidget *w : m_busyWidgets)
        if (w) w->setEnabled(!busy);
    m_progressBar->setVisible(busy);
}

// ════════════════════════════════════════════════════════════════
//  对比文件加载（只读表头 + 时间列，不缓存全量）
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onAddCmpFile()
{
    if (m_loading) return;
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择对比文件"), m_lastDir,
        QStringLiteral("文本文件 (*.csv *.txt);;所有文件 (*)"));
    if (paths.isEmpty()) return;
    m_lastDir = QFileInfo(paths.first()).absolutePath();
    startLoadCmpFiles(paths);
}

void AnalysisPanel::startLoadCmpFiles(const QStringList &paths)
{
    setBusy(true);

    auto *watcher = new QFutureWatcher<QList<CmpSource>>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<QList<CmpSource>>::finished,
            this, &AnalysisPanel::onCmpLoadFinished);

    watcher->setFuture(QtConcurrent::run([](const QStringList &ps) -> QList<CmpSource> {
        QList<CmpSource> result;
        for (const QString &p : ps) {
            CmpSource c;
            c.path = p;
            QString err;
            if (!AnalysisPanel::readCmpHeader(p, &c.header, &c.dataStart, &err)) {
                c.header.clear();
                c.dataStart = -1;   // 标记失败
                continue;
            }
            const TimeDetect td = detectTime(c.header);
            c.timeColumn = td.timeColumn;
            c.timeDayColumn = td.timeDayColumn;
            c.milliseconds = td.milliseconds;
            detectSpeedColumns(c.header, &c.speedColumn, &c.dirColumn);

            // 根据表头推断初始时间格式（仅预填，用户可手动改）
            c.timeFormat = TimeFormat::GpsSow;
            if (c.milliseconds && c.timeDayColumn >= 0) {
                c.timeFormat = TimeFormat::GpsWeekSowMs;   // GPS 周 + 周内毫秒
            } else if (c.milliseconds) {
                c.timeFormat = TimeFormat::GpsSowMs;
            } else if (c.timeDayColumn >= 0 && c.timeDayColumn < c.header.size()) {
                const QString dh = c.header[c.timeDayColumn].toLower();
                if (dh.contains(QStringLiteral("utc")) || dh.contains(QStringLiteral("day")))
                    c.timeFormat = TimeFormat::UtcDaySod;   // 绝对天数 + 天内秒
                else if (dh.contains(QStringLiteral("周")) || dh.contains(QStringLiteral("week")))
                    c.timeFormat = TimeFormat::GpsWeekSow;  // 周数 + 周内秒
                else
                    c.timeFormat = TimeFormat::GpsDaySod;   // 周内天 + 天内秒
            }

            // 读取秒/毫秒列（仅时间列，保持轻量）；存原始值，换算交给 recomputeCmpTimeGps
            QVector<double> raw;
            if (AnalysisPanel::readCmpColumn(p, c.dataStart, c.timeColumn, &raw, &err))
                c.timeRaw = raw;
            else
                c.dataStart = -1;

            // 第二时间列（天数列 / GPS 周数列）
            if (c.timeDayColumn >= 0) {
                QVector<double> dayRaw;
                if (AnalysisPanel::readCmpColumn(p, c.dataStart, c.timeDayColumn, &dayRaw, &err))
                    c.timeDayRaw = dayRaw;
                else
                    c.dataStart = -1;
            }
            result.append(c);
        }
        return result;
    }, paths));
}

void AnalysisPanel::onCmpLoadFinished()
{
    auto *watcher = static_cast<QFutureWatcher<QList<CmpSource>>*>(m_watcher);
    QList<CmpSource> loaded = watcher->result();
    watcher->deleteLater();
    m_watcher = nullptr;
    setBusy(false);

    int okCount = 0;
    for (CmpSource &c : loaded) {
        if (c.dataStart < 0 || c.header.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("加载失败"),
                                 QStringLiteral("无法读取表头：%1").arg(QFileInfo(c.path).fileName()));
            continue;
        }
        m_cmpFiles.append(c);
        for (MetricMapping &m : m_mappings)
            m.cmpCols.append({-1, -1});
        ++okCount;
    }

    if (okCount == 0) {
        rebuildMappingTable();
        return;
    }

    // 统一换算时间列到 GPS 周内秒
    for (int i = 0; i < m_cmpFiles.size(); ++i)
        recomputeCmpTimeGps(i);

    refreshCmpFileList();
    rebuildMappingTable();
    updateAlignStatus();
}

void AnalysisPanel::onRemoveCmpFile()
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) return;
    m_cmpFiles.removeAt(row);
    for (MetricMapping &m : m_mappings)
        if (row < m.cmpCols.size()) m.cmpCols.removeAt(row);
    refreshCmpFileList();
    rebuildMappingTable();
    updateAlignStatus();
}

void AnalysisPanel::recomputeCmpTimeGps(int fileIdx)
{
    if (fileIdx < 0 || fileIdx >= m_cmpFiles.size()) return;
    CmpSource &c = m_cmpFiles[fileIdx];
    // 从原始时间列换算到 UTC 绝对秒（幂等：每次都以 timeRaw/timeDayRaw 为基准）
    c.timeUtc.resize(c.timeRaw.size());
    for (int i = 0; i < c.timeRaw.size(); ++i) {
        const double colA = c.timeRaw[i];          // 主时间列（秒/毫秒）
        const double colB = (c.timeDayColumn >= 0 && c.timeDayRaw.size() == c.timeRaw.size())
                            ? c.timeDayRaw[i] : 0.0;   // 第二时间列（周数/天数）
        c.timeUtc[i] = toUtcAbsSec(c.timeFormat, colA, colB, c.weekNum, m_leapSeconds);
    }
}

void AnalysisPanel::refreshCmpFileList()
{
    m_cmpFileList->clear();
    for (const auto &c : m_cmpFiles) {
        m_cmpFileList->addItem(QStringLiteral("%1  (%2 列)")
                                   .arg(QFileInfo(c.path).fileName())
                                   .arg(c.header.size()));
    }
    if (!m_cmpFiles.isEmpty() && m_cmpFileList->currentRow() < 0)
        m_cmpFileList->setCurrentRow(0);
}

void AnalysisPanel::onCmpFileSelected(int /*row*/)
{
    refreshCmpTimeCombo();
    refreshCmpFormatCombo();
}

void AnalysisPanel::refreshCmpTimeCombo()
{
    m_cmpTimeCombo->clear();
    m_cmpDayCombo->clear();
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) {
        m_cmpTimeCombo->setEnabled(false);
        m_cmpDayCombo->setEnabled(false);
        return;
    }
    m_cmpTimeCombo->addItems(m_cmpFiles[row].header);
    m_cmpTimeCombo->setEnabled(true);
    const int tc = m_cmpFiles[row].timeColumn;
    if (tc >= 0 && tc < m_cmpFiles[row].header.size())
        m_cmpTimeCombo->setCurrentIndex(tc);

    // 天数列下拉：第 0 项 = 无（单列时间），其余对应各列
    m_cmpDayCombo->addItem(QStringLiteral("（无 / 单列时间）"));
    m_cmpDayCombo->addItems(m_cmpFiles[row].header);
    m_cmpDayCombo->setEnabled(true);
    const int dc = m_cmpFiles[row].timeDayColumn;
    m_cmpDayCombo->setCurrentIndex((dc >= 0 && dc < m_cmpFiles[row].header.size()) ? dc + 1 : 0);
}

void AnalysisPanel::refreshCmpFormatCombo()
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) {
        m_cmpFormatCombo->setEnabled(false);
        m_cmpWeekSpin->setEnabled(false);
        return;
    }
    m_cmpFormatCombo->setEnabled(true);
    m_cmpFormatCombo->setCurrentIndex(static_cast<int>(m_cmpFiles[row].timeFormat));
    m_cmpWeekSpin->setValue(m_cmpFiles[row].weekNum);
    m_cmpWeekSpin->setEnabled(fmtNeedsWeek(m_cmpFiles[row].timeFormat));
}

void AnalysisPanel::onCmpTimeColChanged(int idx)
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) return;
    if (idx < 0 || idx >= m_cmpFiles[row].header.size()) return;
    m_cmpFiles[row].timeColumn = idx;
    // 根据列名判断时间单位（毫秒 / 秒）
    const QString h = m_cmpFiles[row].header[idx].toLower();
    m_cmpFiles[row].milliseconds = h.contains(QStringLiteral("毫秒"));
    // 重新读取该文件的时间列（存原始值，再按当前标准换算）
    QVector<double> raw;
    QString err;
    if (AnalysisPanel::readCmpColumn(m_cmpFiles[row].path, m_cmpFiles[row].dataStart, idx, &raw, &err)) {
        m_cmpFiles[row].timeRaw = raw;
        recomputeCmpTimeGps(row);
    }
    updateAlignStatus();
}

void AnalysisPanel::onCmpDayColChanged(int idx)
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) return;
    // idx 0 = 无（单列时间），idx>=1 对应 header[idx-1]
    m_cmpFiles[row].timeDayColumn =
        (idx >= 1 && idx - 1 < m_cmpFiles[row].header.size()) ? (idx - 1) : -1;

    if (m_cmpFiles[row].timeDayColumn >= 0) {
        QVector<double> dayRaw;
        QString err;
        if (AnalysisPanel::readCmpColumn(m_cmpFiles[row].path, m_cmpFiles[row].dataStart,
                                         m_cmpFiles[row].timeDayColumn, &dayRaw, &err))
            m_cmpFiles[row].timeDayRaw = dayRaw;
        else
            m_cmpFiles[row].timeDayRaw.clear();
    } else {
        m_cmpFiles[row].timeDayRaw.clear();
    }
    recomputeCmpTimeGps(row);
    updateAlignStatus();
}

void AnalysisPanel::onCmpFormatChanged(int idx)
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) return;
    m_cmpFiles[row].timeFormat = static_cast<TimeFormat>(idx);
    m_cmpWeekSpin->setEnabled(fmtNeedsWeek(m_cmpFiles[row].timeFormat));
    recomputeCmpTimeGps(row);
    updateAlignStatus();
}

void AnalysisPanel::onCmpWeekChanged(double v)
{
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size()) return;
    m_cmpFiles[row].weekNum = v;
    recomputeCmpTimeGps(row);
    updateAlignStatus();
}

// 显示当前真值 vs 选中对比文件的时间对齐情况
void AnalysisPanel::updateAlignStatus()
{
    if (m_truth.timeUtc.isEmpty()) {
        m_alignInfoLabel->setText(QString());
        return;
    }
    const int row = m_cmpFileList->currentRow();
    if (row < 0 || row >= m_cmpFiles.size() || m_cmpFiles[row].timeUtc.isEmpty()) {
        m_alignInfoLabel->setText(QString());
        return;
    }
    const QVector<MatchedPair> pairs = exactMatchTimes(m_truth.timeUtc, m_cmpFiles[row].timeUtc);
    const int n = pairs.size();

    QString text = QStringLiteral("时间对齐：匹配 %1 个时间戳（共 %2 / %3）")
                       .arg(n)
                       .arg(m_truth.timeUtc.size())
                       .arg(m_cmpFiles[row].timeUtc.size());

    if (n > 0) {
        // 重叠段首尾（UTC 绝对秒）
        const double t0 = m_truth.timeUtc[pairs.first().aIdx];
        const double t1 = m_truth.timeUtc[pairs.last().aIdx];
        // 基准与场景输入一致：首样本所在 UTC 天 0 点；GPS 天 0 点 = UTC 天 0 点 - 闰秒
        const double dayBaseUtc = std::floor(m_truth.timeUtc.first() / 86400.0) * 86400.0;
        const double dayBaseGps = dayBaseUtc - m_leapSeconds;
        text += QStringLiteral("\n重叠段 UTC 天内秒：%1 ~ %2")
                    .arg(t0 - dayBaseUtc, 0, 'f', 3)
                    .arg(t1 - dayBaseUtc, 0, 'f', 3);
        text += QStringLiteral("\n重叠段 GPS 天内秒：%1 ~ %2")
                    .arg(t0 - dayBaseGps, 0, 'f', 3)
                    .arg(t1 - dayBaseGps, 0, 'f', 3);
    }
    m_alignInfoLabel->setText(text);
}

// ════════════════════════════════════════════════════════════════
//  对比项映射
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onAddMetric()
{
    MetricMapping m;
    m.type = MetricType::HorizPos;
    m.name = metricName(m.type);
    m.cmpCols = QVector<MetricMapping::FileCols>(m_cmpFiles.size());
    m_mappings.append(m);
    rebuildMappingTable();
}

void AnalysisPanel::onRemoveMetric()
{
    const int row = m_mapTable->currentRow();
    if (row < 0 || row >= m_mappings.size()) return;
    m_mappings.removeAt(row);
    rebuildMappingTable();
}

static QComboBox *putCombo(QTableWidget *table, int row, int col,
                           const QStringList &items, int current)
{
    auto *combo = new QComboBox(table);
    combo->addItems(items);
    if (current >= 0 && current < items.size())
        combo->setCurrentIndex(current);
    table->setCellWidget(row, col, combo);
    return combo;
}

void AnalysisPanel::rebuildMappingTable()
{
    m_mapTable->blockSignals(true);

    // 表头：指标名 | 指标类型 | 扣均值 | 每个对比文件两列（列1/列2，复合指标两列都用，单列只用列1）
    QStringList headers{ QStringLiteral("指标名"), QStringLiteral("指标类型"), QStringLiteral("扣均值") };
    for (const auto &c : m_cmpFiles) {
        headers << QFileInfo(c.path).fileName() + QStringLiteral(" 列1");
        headers << QFileInfo(c.path).fileName() + QStringLiteral(" 列2");
    }

    m_mapTable->clear();
    m_mapTable->setColumnCount(headers.size());
    m_mapTable->setHorizontalHeaderLabels(headers);
    m_mapTable->setRowCount(m_mappings.size());
    m_mapTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_mapTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    // 指标类型下拉列表（顺序必须与 MetricType 枚举一致）
    const QStringList metricLabels{
        QStringLiteral("水平位置误差（列1=纬度，列2=经度）"),
        QStringLiteral("高程误差"),
        QStringLiteral("水平速度误差（列1=北向，列2=东向）"),
        QStringLiteral("天向速度误差"),
        QStringLiteral("横滚(Roll)"),
        QStringLiteral("俯仰(Pitch)"),
        QStringLiteral("航向(Heading)"),
    };

    for (int r = 0; r < m_mappings.size(); ++r) {
        MetricMapping &m = m_mappings[r];

        auto *nameItem = new QTableWidgetItem(m.name);
        m_mapTable->setItem(r, 0, nameItem);

        putCombo(m_mapTable, r, 1, metricLabels, static_cast<int>(m.type));

        auto *ck = new QCheckBox(m_mapTable);
        ck->setChecked(m.subtractMean);
        auto *ckWrap = new QWidget(m_mapTable);
        auto *ckLay = new QHBoxLayout(ckWrap);
        ckLay->addWidget(ck);
        ckLay->setAlignment(Qt::AlignCenter);
        ckLay->setContentsMargins(0, 0, 0, 0);
        m_mapTable->setCellWidget(r, 2, ckWrap);

        const bool twoCols = metricNeedsTwoCols(m.type);
        for (int f = 0; f < m_cmpFiles.size(); ++f) {
            QStringList items = m_cmpFiles[f].header;
            // 若存在前向速度 + 速度方向列，追加两个派生项（索引 = header.size() / +1）
            if (m_cmpFiles[f].speedColumn >= 0 && m_cmpFiles[f].dirColumn >= 0) {
                items << QStringLiteral("北向速度(前向速度×cos方向)");
                items << QStringLiteral("东向速度(前向速度×sin方向)");
            }
            const MetricMapping::FileCols fc = m.cmpCols.value(f);
            putCombo(m_mapTable, r, 3 + f * 2, items, fc.primary);

            // 列2：复合指标启用；单列指标禁用并置空
            auto *c2 = new QComboBox(m_mapTable);
            c2->addItems(items);
            if (twoCols && fc.secondary >= 0 && fc.secondary < items.size())
                c2->setCurrentIndex(fc.secondary);
            c2->setEnabled(twoCols);
            m_mapTable->setCellWidget(r, 3 + f * 2 + 1, c2);
        }
    }

    // 从单元格控件回写 m_mappings
    const int cmpCount = m_cmpFiles.size();
    for (int r = 0; r < m_mappings.size(); ++r) {
        const int rr = r;

        if (auto *fc = qobject_cast<QComboBox*>(m_mapTable->cellWidget(r, 1))) {
            connect(fc, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this, rr, cmpCount](int idx) {
                        if (rr >= 0 && rr < m_mappings.size()) {
                            const MetricType nt = static_cast<MetricType>(idx);
                            if (m_mappings[rr].name.isEmpty() ||
                                m_mappings[rr].name == metricName(m_mappings[rr].type))
                                m_mappings[rr].name = metricName(nt);
                            m_mappings[rr].type = nt;
                            // 类型变化可能改变「是否两列」，同步该行列2控件的启用状态
                            const bool twoCols = metricNeedsTwoCols(nt);
                            for (int f = 0; f < cmpCount; ++f)
                                if (auto *c2 = qobject_cast<QComboBox*>(m_mapTable->cellWidget(rr, 3 + f * 2 + 1)))
                                    c2->setEnabled(twoCols);
                        }
                    });
        }

        if (auto *wrap = qobject_cast<QWidget*>(m_mapTable->cellWidget(r, 2))) {
            if (auto *ck = wrap->findChild<QCheckBox*>()) {
                connect(ck, &QCheckBox::toggled, this, [this, rr](bool on) {
                    if (rr >= 0 && rr < m_mappings.size())
                        m_mappings[rr].subtractMean = on;
                });
            }
        }

        for (int f = 0; f < cmpCount; ++f) {
            const int ff = f;
            if (auto *cc = qobject_cast<QComboBox*>(m_mapTable->cellWidget(r, 3 + f * 2))) {
                connect(cc, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                        [this, rr, ff](int idx) {
                            if (rr >= 0 && rr < m_mappings.size()) {
                                if (m_mappings[rr].cmpCols.size() <= ff)
                                    m_mappings[rr].cmpCols.resize(ff + 1);
                                m_mappings[rr].cmpCols[ff].primary = idx;
                            }
                        });
            }
            if (auto *cc = qobject_cast<QComboBox*>(m_mapTable->cellWidget(r, 3 + f * 2 + 1))) {
                connect(cc, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                        [this, rr, ff](int idx) {
                            if (rr >= 0 && rr < m_mappings.size()) {
                                if (m_mappings[rr].cmpCols.size() <= ff)
                                    m_mappings[rr].cmpCols.resize(ff + 1);
                                m_mappings[rr].cmpCols[ff].secondary = idx;
                            }
                        });
            }
        }
    }

    m_mapTable->blockSignals(false);
}

// ════════════════════════════════════════════════════════════════
//  场景切段
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onAddScene()
{
    bool ok1 = false, ok2 = false;
    const double startSod = m_sceneStartEdit->text().trimmed().toDouble(&ok1);
    const double endSod   = m_sceneEndEdit->text().trimmed().toDouble(&ok2);
    if (!ok1 || !ok2) {
        QMessageBox::warning(this, QStringLiteral("输入错误"),
                             QStringLiteral("请填写有效的起始/结束时间（天内秒）。"));
        return;
    }
    if (startSod < 0.0 || startSod > 86400.0 || endSod < 0.0 || endSod > 86400.0 || endSod <= startSod) {
        QMessageBox::warning(this, QStringLiteral("输入错误"),
                             QStringLiteral("天内秒需在 0~86400 之间，且结束必须大于起始。"));
        return;
    }

    // 场景起止以「UTC 天内秒」输入，内部换算成 UTC 绝对秒后再与时间列比较。
    // 基准由 sceneDayBase() 给出（真值首样本所在 UTC 天的 0 点）。
    if (m_truth.timeUtc.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先导入真值文件，再添加场景。"));
        return;
    }
    const double dayBase = sceneDayBase();

    SceneSegment seg;
    seg.name = m_sceneNameEdit->text().trimmed();
    if (seg.name.isEmpty())
        seg.name = QStringLiteral("场景%1").arg(m_scenes.size() + 1);
    seg.startSec = dayBase + startSod;
    seg.endSec   = dayBase + endSod;
    m_scenes.append(seg);
    refreshSceneList();
}

void AnalysisPanel::onRemoveScene()
{
    const int row = m_sceneList->currentRow();
    if (row < 0 || row >= m_scenes.size()) return;
    m_scenes.removeAt(row);
    refreshSceneList();
}

double AnalysisPanel::sceneDayBase() const
{
    if (m_truth.timeUtc.isEmpty())
        return 0.0;
    // 场景起止统一使用 UTC 天内秒：基准取真值首样本所在 UTC 天的 0 点绝对秒
    return std::floor(m_truth.timeUtc.first() / 86400.0) * 86400.0;
}

void AnalysisPanel::refreshSceneList()
{
    m_sceneList->clear();
    const double dayBase = sceneDayBase();
    for (const auto &s : m_scenes) {
        QString name = s.name.isEmpty() ? QStringLiteral("场景%1").arg(m_sceneList->count() + 1) : s.name;
        m_sceneList->addItem(QStringLiteral("%1  [%2s - %3s]（UTC 天内秒）")
                                 .arg(name)
                                 .arg(s.startSec - dayBase, 0, 'f', 3)
                                 .arg(s.endSec - dayBase, 0, 'f', 3));
    }
}

// ════════════════════════════════════════════════════════════════
//  结果计算（后台线程）
// ════════════════════════════════════════════════════════════════
void AnalysisPanel::onStartCompare()
{
    if (m_truth.summary.samples.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先导入真值文件。"));
        return;
    }
    if (m_cmpFiles.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先导入至少一个对比文件。"));
        return;
    }
    if (m_mappings.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先添加对比项并指定列映射。"));
        return;
    }

    setBusy(true);

    // 有效场景：分析当前选中的场景；未选中任何场景时用全路段
    QList<SceneSegment> effScenes;
    const int sceneRow = m_sceneList->currentRow();
    if (sceneRow >= 0 && sceneRow < m_scenes.size()) {
        effScenes.append(m_scenes[sceneRow]);
    } else {
        SceneSegment all;
        all.name = QStringLiteral("全路段");
        all.startSec = -std::numeric_limits<double>::max();
        all.endSec   =  std::numeric_limits<double>::max();
        effScenes.append(all);
    }

    // 捕获副本（QVector 隐式共享，拷贝代价低）
    const TruthSource truth = m_truth;
    const QList<CmpSource> cmp = m_cmpFiles;
    const QList<MetricMapping> mappings = m_mappings;
    const double leap = m_leapSeconds;

    auto *watcher = new QFutureWatcher<CompareOutcome>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<CompareOutcome>::finished,
            this, &AnalysisPanel::onCompareFinished);

    watcher->setFuture(QtConcurrent::run(
        [truth, cmp, mappings, effScenes, leap]() -> CompareOutcome {
            CompareOutcome out;

            constexpr double R = 6378137.0;          // WGS84 长半轴（米），ENU 投影用
            constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;

            // 每个对比文件先做一次时间精确对齐（复用索引配对）
            QVector<QVector<MatchedPair>> matchIdx(cmp.size());
            for (int f = 0; f < cmp.size(); ++f)
                matchIdx[f] = exactMatchTimes(truth.timeUtc, cmp[f].timeUtc);

            // 提取真值字段（which：0=主字段，1=次字段；单字段指标忽略 which）
            auto truthField = [&truth](MetricType t, int which) -> QVector<double> {
                QVector<double> v;
                v.reserve(truth.summary.samples.size());
                for (const TruthSample &sp : truth.summary.samples) {
                    switch (t) {
                        case MetricType::HorizPos: v.append(which == 0 ? sp.lat : sp.lon); break;
                        case MetricType::Height:   v.append(sp.h); break;
                        case MetricType::HorizVel: v.append(which == 0 ? sp.velN : sp.velE); break;
                        case MetricType::VertVel:  v.append(sp.velU); break;
                        case MetricType::Roll:     v.append(sp.roll); break;
                        case MetricType::Pitch:    v.append(sp.pitch); break;
                        case MetricType::Heading:  v.append(sp.heading); break;
                    }
                }
                return v;
            };

            // 读取对比文件某列（支持派生列：北向/东向速度 = 前向速度×cos/sin(方向)）
            auto readCol = [&cmp](int f, int col, QVector<double> *out) -> bool {
                if (col < 0) return false;
                const int hdrSize = cmp[f].header.size();
                QString err;
                if (col == hdrSize || col == hdrSize + 1) {
                    const bool velN = (col == hdrSize);
                    QVector<double> spd, dir;
                    if (!AnalysisPanel::readCmpColumn(cmp[f].path, cmp[f].dataStart, cmp[f].speedColumn, &spd, &err) ||
                        !AnalysisPanel::readCmpColumn(cmp[f].path, cmp[f].dataStart, cmp[f].dirColumn, &dir, &err))
                        return false;
                    const int nv = qMin(spd.size(), dir.size());
                    out->resize(nv);
                    for (int i = 0; i < nv; ++i) {
                        const double rad = qDegreesToRadians(dir[i]);
                        (*out)[i] = velN ? spd[i] * std::cos(rad) : spd[i] * std::sin(rad);
                    }
                    return true;
                }
                return AnalysisPanel::readCmpColumn(cmp[f].path, cmp[f].dataStart, col, out, &err);
            };

            for (const MetricMapping &m : mappings) {
                const QString metricLabel = m.name.isEmpty() ? metricName(m.type) : m.name;
                const bool twoCols = metricNeedsTwoCols(m.type);

                for (int f = 0; f < cmp.size(); ++f) {
                    const MetricMapping::FileCols fc = m.cmpCols.value(f);
                    const int hdrSize = cmp[f].header.size();
                    if (fc.primary < 0 || fc.primary >= hdrSize + 2) continue;
                    if (twoCols && (fc.secondary < 0 || fc.secondary >= hdrSize + 2)) continue;

                    // 读对比列（单/双列）
                    QVector<double> cA, cB;
                    if (!readCol(f, fc.primary, &cA)) continue;
                    if (twoCols && !readCol(f, fc.secondary, &cB)) continue;

                    // 真值字段
                    const QVector<double> tA = truthField(m.type, 0);
                    const QVector<double> tB = twoCols ? truthField(m.type, 1) : QVector<double>();

                    // 高程异常：仅高程误差把真值抬高（真值 = 原始 + 异常）
                    const double anomaly = (m.type == MetricType::Height) ? truth.heightAnomaly : 0.0;

                    // ENU 投影参考点（轨迹首点）
                    const double lat0 = truth.summary.samples.isEmpty() ? 0.0
                                       : truth.summary.samples.first().lat;
                    const double cosLat0 = std::cos(lat0 * DEG2RAD);

                    const QVector<MatchedPair> &pairs = matchIdx[f];

                    CurveSeries es;
                    es.metric = metricLabel;
                    es.file   = QFileInfo(cmp[f].path).fileName();
                    es.unit   = metricUnit(m.type);

                    for (const SceneSegment &sc : effScenes) {
                        double sum = 0.0, sumSq = 0.0;
                        int n = 0;
                        for (const MatchedPair &p : pairs) {
                            const double t = truth.timeUtc[p.aIdx];
                            if (t < sc.startSec || t > sc.endSec) continue;
                            if (p.bIdx >= cA.size()) continue;
                            if (twoCols && p.bIdx >= cB.size()) continue;
                            if (p.aIdx >= tA.size()) continue;

                            double e = 0.0;
                            switch (m.type) {
                            case MetricType::HorizPos: {
                                // 经纬度投影到局部切平面后求水平位置差（米）
                                const double dLat = (cA[p.bIdx] - tA[p.aIdx]) * DEG2RAD;
                                const double dLon = (cB[p.bIdx] - tB[p.aIdx]) * DEG2RAD;
                                e = R * std::sqrt(dLat * dLat + dLon * dLon * cosLat0 * cosLat0);
                                break;
                            }
                            case MetricType::Height:
                                e = cA[p.bIdx] - (tA[p.aIdx] + anomaly);
                                break;
                            case MetricType::HorizVel: {
                                // 合成水平速度 sqrt(VN²+VE²) 后做差
                                const double vt = std::sqrt(tA[p.aIdx] * tA[p.aIdx] + tB[p.aIdx] * tB[p.aIdx]);
                                const double vc = std::sqrt(cA[p.bIdx] * cA[p.bIdx] + cB[p.bIdx] * cB[p.bIdx]);
                                e = vc - vt;
                                break;
                            }
                            case MetricType::Heading: {
                                // 航向角度差归一化到 [-180, 180)，避免 359° vs 1° 得 358° 的错误
                                double de = cA[p.bIdx] - tA[p.aIdx];
                                de = std::fmod(de + 180.0, 360.0);
                                if (de < 0.0) de += 360.0;
                                e = de - 180.0;
                                break;
                            }
                            default:
                                e = cA[p.bIdx] - tA[p.aIdx];   // 天向速度 / Roll / Pitch 直接做差
                                break;
                            }

                            if (!std::isfinite(e)) continue;
                            sum += e;
                            sumSq += e * e;
                            ++n;
                            es.time.append(t);
                            es.error.append(e);
                        }

                        if (n == 0) continue;
                        const double mean  = sum / n;
                        const double rmsBias = std::sqrt(sumSq / n);
                        const double var = sumSq / n - mean * mean;
                        const double sigma = var > 0.0 ? std::sqrt(var) : 0.0;
                        const double rms = m.subtractMean ? sigma : rmsBias;

                        CompareRow row;
                        row.scene  = sc.name;
                        row.metric = metricLabel;
                        row.file   = QFileInfo(cmp[f].path).fileName();
                        row.n      = n;
                        row.mean   = mean;
                        row.rms    = rms;
                        row.twoSigma = 2.0 * sigma;
                        out.rows.append(row);
                    }

                    if (!es.time.isEmpty())
                        out.series.append(es);
                }
            }

            out.ok = true;
            if (out.rows.isEmpty())
                out.err = QStringLiteral("没有精确对齐的时间戳数据，请确认两边时间列单位一致且存在相同时间戳。");
            return out;
        }));
}

void AnalysisPanel::onCompareFinished()
{
    auto *watcher = static_cast<QFutureWatcher<CompareOutcome>*>(m_watcher);
    CompareOutcome out = watcher->result();
    watcher->deleteLater();
    m_watcher = nullptr;
    setBusy(false);

    if (!out.ok) {
        QMessageBox::warning(this, QStringLiteral("分析失败"), out.err);
        return;
    }
    if (out.rows.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), out.err);
        return;
    }

    // 结果表
    m_resultTable->clear();
    m_resultTable->setColumnCount(7);
    m_resultTable->setHorizontalHeaderLabels({
        QStringLiteral("场景"), QStringLiteral("指标"), QStringLiteral("对比文件"),
        QStringLiteral("样本数"), QStringLiteral("均值偏置"),
        QStringLiteral("RMS"), QStringLiteral("2σ") });
    m_resultTable->setRowCount(out.rows.size());
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    for (int r = 0; r < out.rows.size(); ++r) {
        const CompareRow &row = out.rows[r];
        m_resultTable->setItem(r, 0, new QTableWidgetItem(row.scene));
        m_resultTable->setItem(r, 1, new QTableWidgetItem(row.metric));
        m_resultTable->setItem(r, 2, new QTableWidgetItem(row.file));
        m_resultTable->setItem(r, 3, new QTableWidgetItem(QString::number(row.n)));
        m_resultTable->setItem(r, 4, new QTableWidgetItem(fmtNum(row.mean)));
        m_resultTable->setItem(r, 5, new QTableWidgetItem(fmtNum(row.rms)));
        m_resultTable->setItem(r, 6, new QTableWidgetItem(fmtNum(row.twoSigma)));
    }

    // 曲线不自动弹出，缓存起来，双击结果表某一行时再弹出对应曲线窗口
    m_curSeries = out.series;
}

void AnalysisPanel::rebuildResultTable()
{
    m_resultTable->clear();
    m_resultTable->setColumnCount(7);
    m_resultTable->setHorizontalHeaderLabels({
        QStringLiteral("场景"), QStringLiteral("指标"), QStringLiteral("对比文件"),
        QStringLiteral("样本数"), QStringLiteral("均值偏置"),
        QStringLiteral("RMS"), QStringLiteral("2σ") });
    m_resultTable->setRowCount(0);
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void AnalysisPanel::showCurveWindow(const CurveSeries &s)
{
    // 单个对比项差值图窗口：非模态、可缩放，展示误差曲线（对比值 - 真值）
    const QString title = QStringLiteral("%1 - %2").arg(s.metric, s.file);
    auto *win = new CompareChartWindow(title, s.unit, s.time, s.error, this);
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->show();
}

void AnalysisPanel::onResultRowDoubleClicked(int row, int /*column*/)
{
    if (row < 0) return;
    const QTableWidgetItem *mi = m_resultTable->item(row, 1);   // 指标
    const QTableWidgetItem *fi = m_resultTable->item(row, 2);   // 对比文件
    if (!mi || !fi) return;

    const QString metric = mi->text();
    const QString file   = fi->text();
    for (const CurveSeries &s : m_curSeries) {
        if (s.metric == metric && s.file == file) {
            showCurveWindow(s);
            return;
        }
    }
}

} // namespace Analysis
