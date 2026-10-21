#include "columnextract.h"

#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QStringConverter>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QApplication>

#include <algorithm>
#include <cmath>

#include <QCloseEvent>

namespace {
struct LoadResult   { QList<TextFileTable> tables; };
struct ExportResult { bool ok; QString error; qint64 size = 0; };
}

// ── 识别分隔符 ──
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

// ── 按分隔符切分；空白分隔时合并连续空白并去首尾 ──
static QStringList splitFields(const QString &line, QChar delim)
{
    if (delim == ' ' || delim == '\t') {
        static const QRegularExpression ws(QStringLiteral("\\s+"));
        return line.trimmed().split(ws, Qt::SkipEmptyParts);
    }
    return line.split(delim);
}

// 手动校验字节序列是否为合法 UTF-8（GBK 等本地编码的高位字节会命中非法序列，从而正确判定为非 UTF-8）
static bool isUtf8Bytes(const QByteArray &b)
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

// ── 解码文本字节：BOM 优先；无 BOM 时合法 UTF-8 用 UTF-8，否则回退系统本地编码 ──
// 中文 Windows 下的 Excel「CSV（逗号分隔）」导出的是 GBK，按 UTF-8 硬解会把中文表头读成乱码，
// 进而导致「GPS周内秒」等时间列与经/纬度列的表头关键词匹配失败。
static QString decodeTextBytes(const QByteArray &raw)
{
    QStringDecoder::Encoding enc = QStringDecoder::Utf8;
    int skip = 0;

    if (raw.startsWith("\xEF\xBB\xBF")) {
        enc = QStringDecoder::Utf8;
        skip = 3;
    } else if (raw.startsWith("\xFF\xFE")) {
        enc = QStringDecoder::Utf16LE;
        skip = 2;
    } else if (raw.startsWith("\xFE\xFF")) {
        enc = QStringDecoder::Utf16BE;
        skip = 2;
    } else {
        enc = isUtf8Bytes(raw.left(8192)) ? QStringDecoder::Utf8 : QStringDecoder::System;
    }

    QStringDecoder dec(enc);
    QString text = dec(QByteArrayView(raw).sliced(skip));

    // 统一换行：后续按 '\n' 切行且表头行不做 trim，必须清掉残留的 '\r'
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

// ── 自动识别时间列（默认第 0 列） ──
static int autoDetectTimeColumn(const QStringList &header)
{
    static const QString keys[] = {
        QStringLiteral("时间"), QStringLiteral("周内秒"), QStringLiteral("秒"),
        QStringLiteral("itow"), QStringLiteral("time"), QStringLiteral("gps"),
        QStringLiteral("ms")
    };
    for (int i = 0; i < header.size(); ++i) {
        const QString h = header[i].toLower();
        for (const QString &k : keys) {
            if (h.contains(k.toLower()))
                return i;
        }
    }
    return 0;
}

TextFileTable loadTextFileTable(const QString &path, int timeColumn)
{
    TextFileTable t;
    t.path = path;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        t.error = QStringLiteral("无法打开文件");
        return t;
    }
    const QByteArray raw = f.readAll();
    f.close();

    // 按内容自适应编码（UTF-8 / UTF-8 BOM / UTF-16 / GBK 等系统本地编码）
    const QString text = decodeTextBytes(raw);

    const QStringList lines = text.split('\n');

    // 定位表头行：若存在 "END OF HEADER" 标记（真值文件头部元信息结束标记），
    // 则从其后的第一个非空行取表头；否则取第一个非空行（普通 CSV/TXT）。
    int headerIdx = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].contains(QStringLiteral("END OF HEADER"), Qt::CaseInsensitive)) {
            for (int j = i + 1; j < lines.size(); ++j) {
                if (!lines[j].trimmed().isEmpty()) { headerIdx = j; break; }
            }
            break;
        }
    }
    if (headerIdx < 0) {
        for (int i = 0; i < lines.size(); ++i) {
            if (!lines[i].trimmed().isEmpty()) { headerIdx = i; break; }
        }
    }
    if (headerIdx < 0) { t.error = QStringLiteral("空文件"); return t; }

    t.delimiter = detectDelimiter(lines[headerIdx]);
    t.header    = splitFields(lines[headerIdx], t.delimiter);
    if (t.header.isEmpty()) { t.error = QStringLiteral("无法识别表头"); return t; }

    // 数据起始行：若表头下一行是单位行（首字段以 '(' 开头，如 "(sec)"），则跳过
    int dataStart = headerIdx + 1;
    if (dataStart < lines.size()) {
        const QStringList unitFields = splitFields(lines[dataStart], t.delimiter);
        if (!unitFields.isEmpty() && unitFields.first().startsWith('('))
            ++dataStart;
    }

    for (int i = dataStart; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        QStringList cols = splitFields(line, t.delimiter);
        if (cols.isEmpty()) continue;
        while (cols.size() < t.header.size()) cols.append(QString());
        if (cols.size() > t.header.size()) cols = cols.mid(0, t.header.size());
        t.rows.append(cols);
    }

    if (timeColumn >= 0 && timeColumn < t.header.size())
        t.timeColumn = timeColumn;
    else
        t.timeColumn = autoDetectTimeColumn(t.header);

    t.times.resize(t.rows.size());
    for (int i = 0; i < t.rows.size(); ++i) {
        bool ok = false;
        const double v = t.rows[i].value(t.timeColumn).trimmed().toDouble(&ok);
        t.times[i] = ok ? v : 0.0;
    }

    return t;
}

// ════════════════════════════════════════════════════════════════
//  ColumnExtractDialog
// ════════════════════════════════════════════════════════════════

ColumnExtractDialog::ColumnExtractDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("New Code — 文本列提取与时间对齐"));
    resize(760, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(6);

    // ── 顶部操作行 ──
    auto *top = new QHBoxLayout;
    m_addBtn    = new QPushButton(QStringLiteral("添加文件..."), this);
    m_removeBtn = new QPushButton(QStringLiteral("移除选中"), this);
    m_removeBtn->setEnabled(false);
    top->addWidget(m_addBtn);
    top->addWidget(m_removeBtn);
    top->addStretch();
    root->addLayout(top);

    // ── 提示文字 ──
    auto *hint = new QLabel(
        QStringLiteral("首行为表头，支持空格/逗号/Tab 分隔；以低Hz文件为主轴对齐，各文件时间列单位需一致"),
        this);
    hint->setStyleSheet(QStringLiteral("color:#617487;"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    // ── 主体 ──
    auto *body = new QHBoxLayout;
    body->setSpacing(8);

    // 左：文件列表 + 主文件
    auto *left = new QVBoxLayout;
    left->setSpacing(4);
    left->addWidget(new QLabel(QStringLiteral("文件列表"), this));
    m_fileList = new QListWidget(this);
    left->addWidget(m_fileList, 1);
    left->addWidget(new QLabel(QStringLiteral("主文件(时间轴基准，默认低Hz)"), this));
    m_masterCombo = new QComboBox(this);
    left->addWidget(m_masterCombo);
    body->addLayout(left, 1);

    // 右：时间列 + 列勾选 + 全选/全不选
    auto *right = new QVBoxLayout;
    right->setSpacing(4);
    right->addWidget(new QLabel(QStringLiteral("当前文件的时间列"), this));
    m_timeCombo = new QComboBox(this);
    right->addWidget(m_timeCombo);

    auto *colHeader = new QHBoxLayout;
    colHeader->addWidget(new QLabel(QStringLiteral("勾选要输出的列"), this));
    colHeader->addStretch();
    m_selAllBtn   = new QPushButton(QStringLiteral("全选"), this);
    m_deselAllBtn = new QPushButton(QStringLiteral("全不选"), this);
    m_selAllBtn->setFixedWidth(60);
    m_deselAllBtn->setFixedWidth(60);
    colHeader->addWidget(m_selAllBtn);
    colHeader->addWidget(m_deselAllBtn);
    right->addLayout(colHeader);

    m_columnList = new QListWidget(this);
    right->addWidget(m_columnList, 1);
    body->addLayout(right, 1);

    root->addLayout(body, 1);

    // ── 进度条（默认隐藏） ──
    auto *progRow = new QHBoxLayout;
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);  // 不确定进度
    m_progressBar->setVisible(false);
    m_progressLbl = new QLabel(this);
    m_progressLbl->setVisible(false);
    progRow->addWidget(m_progressBar, 1);
    progRow->addWidget(m_progressLbl);
    root->addLayout(progRow);

    // ── 状态栏 ──
    m_hintLabel = new QLabel(this);
    m_hintLabel->setStyleSheet(QStringLiteral("color:#617487;"));
    root->addWidget(m_hintLabel);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = m_buttonBox->button(QDialogButtonBox::Ok);
    m_okBtn->setText(QStringLiteral("开始导出"));
    root->addWidget(m_buttonBox);

    // 注意：不用 QDialogButtonBox 的自动 accepted/rejected 信号，
    // 因为那会触发 QDialog::accept()/reject() 自动关闭对话框，
    // 而异步导出期间不能关闭。改为手动连接按钮 clicked。
    connect(m_okBtn, &QPushButton::clicked, this, &ColumnExtractDialog::onOkClicked);
    connect(m_buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
            this, &ColumnExtractDialog::onCancelClicked);

    connect(m_addBtn,    &QPushButton::clicked, this, &ColumnExtractDialog::addFiles);
    connect(m_removeBtn, &QPushButton::clicked, this, &ColumnExtractDialog::removeCurrentFile);
    connect(m_selAllBtn,   &QPushButton::clicked, this, [this](){ selectAllColumns(true);  });
    connect(m_deselAllBtn, &QPushButton::clicked, this, [this](){ selectAllColumns(false); });
    connect(m_fileList, &QListWidget::currentRowChanged,
            this, &ColumnExtractDialog::onCurrentFileChanged);
    connect(m_columnList, &QListWidget::itemChanged,
            this, &ColumnExtractDialog::onColumnItemChanged);
    connect(m_timeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        const int fi = currentFileIndex();
        if (fi >= 0 && fi < m_files.size())
            m_files[fi].timeColumn = idx;
    });
}

ColumnExtractDialog::~ColumnExtractDialog() = default;

// ── 防止导出/加载期间被关闭 ──
void ColumnExtractDialog::closeEvent(QCloseEvent *e)
{
    if (m_exporting || m_loading) {
        e->ignore();
        return;
    }
    scheduleDataCleanup();   // 关闭前先把大数据移走，避免析构时卡顿
    QDialog::closeEvent(e);
}

void ColumnExtractDialog::onCancelClicked()
{
    if (m_exporting || m_loading) return;  // 工作中不允许取消关闭
    scheduleDataCleanup();
    reject();
}

// ── 将 m_files 中的大数据移到后台线程释放 ──
// 核心问题：m_files 里可能有上千万个 QString（几十万行 × 几十列），
// 如果在主线程 delete 会造成数秒卡顿。swap 到堆上，交给后台线程慢慢 free。
void ColumnExtractDialog::scheduleDataCleanup()
{
    if (m_files.isEmpty()) return;
    auto *bigData = new QList<FileUi>();
    bigData->swap(m_files);
    auto *cleaner = new QFutureWatcher<void>();   // 无 parent，自我销毁
    connect(cleaner, &QFutureWatcher<void>::finished, cleaner, &QObject::deleteLater);
    cleaner->setFuture(QtConcurrent::run([bigData]() {
        delete bigData;
    }));
}

void ColumnExtractDialog::addFiles()
{
    if (m_loading) return;
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择文本数据文件"), QString(),
        QStringLiteral("文本文件 (*.csv *.txt);;所有文件 (*)"));
    if (paths.isEmpty()) return;
    startAsyncLoad(paths);
}

void ColumnExtractDialog::startAsyncLoad(const QStringList &paths)
{
    setBusy(true);
    m_progressLbl->setText(QStringLiteral("正在加载 %1 个文件...").arg(paths.size()));

    auto *watcher = new QFutureWatcher<LoadResult>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<LoadResult>::finished,
            this, &ColumnExtractDialog::onLoadFinished);

    watcher->setFuture(QtConcurrent::run([](const QStringList &ps) -> LoadResult {
        LoadResult r;
        for (const QString &p : ps) {
            r.tables.append(loadTextFileTable(p));
        }
        return r;
    }, paths));
}

void ColumnExtractDialog::onLoadFinished()
{
    auto *watcher = static_cast<QFutureWatcher<LoadResult>*>(m_watcher);
    LoadResult res = watcher->result();
    QList<TextFileTable> tables = res.tables;
    watcher->deleteLater();
    m_watcher = nullptr;

    int okCount = 0, failCount = 0;
    QStringList failMsgs;
    for (int i = 0; i < tables.size(); ++i) {
        const TextFileTable &t = tables[i];
        if (!t.error.isEmpty()) {
            ++failCount;
            failMsgs << QStringLiteral("  %1: %2")
                            .arg(QFileInfo(t.path).fileName(), t.error);
            continue;
        }
        FileUi fu;
        fu.table      = t;
        fu.timeColumn = t.timeColumn;
        fu.checked    = QList<bool>(t.header.size(), true);
        m_files.append(fu);
        ++okCount;
    }

    setBusy(false);

    if (failCount > 0) {
        QMessageBox::warning(this, QStringLiteral("部分文件加载失败"),
                             QStringLiteral("成功 %1 个，失败 %2 个:\n%3")
                                 .arg(okCount).arg(failCount).arg(failMsgs.join('\n')));
    }

    refreshFileList();
    if (!m_files.isEmpty()) {
        if (m_fileList->currentRow() < 0)
            m_fileList->setCurrentRow(0);
        m_masterCombo->setCurrentIndex(autoMasterIndex());
    }
}

void ColumnExtractDialog::setBusy(bool busy, const QString &msg)
{
    m_loading = busy;
    m_addBtn->setEnabled(!busy);
    m_removeBtn->setEnabled(!busy && !m_files.isEmpty());
    m_selAllBtn->setEnabled(!busy);
    m_deselAllBtn->setEnabled(!busy);
    m_fileList->setEnabled(!busy);
    m_columnList->setEnabled(!busy);
    m_timeCombo->setEnabled(!busy);
    m_masterCombo->setEnabled(!busy);
    if (m_okBtn) m_okBtn->setEnabled(!busy);
    if (m_buttonBox) {
        auto *cancelBtn = m_buttonBox->button(QDialogButtonBox::Cancel);
        if (cancelBtn) cancelBtn->setEnabled(!busy);
    }
    m_progressBar->setVisible(busy);
    m_progressLbl->setVisible(busy);
    if (!msg.isEmpty()) m_progressLbl->setText(msg);
    if (busy) m_progressBar->setRange(0, 0);
    else { m_progressBar->setRange(0, 100); m_progressBar->setValue(0); }
    QApplication::processEvents();
}

void ColumnExtractDialog::removeCurrentFile()
{
    const int row = m_fileList->currentRow();
    if (row < 0 || row >= m_files.size()) return;
    m_files.removeAt(row);
    refreshFileList();
    if (!m_files.isEmpty())
        m_fileList->setCurrentRow(qMin(row, int(m_files.size()) - 1));
}

void ColumnExtractDialog::refreshFileList()
{
    m_fileList->clear();
    m_masterCombo->clear();
    for (const auto &fu : m_files) {
        const QString name = QFileInfo(fu.table.path).fileName();
        m_fileList->addItem(QStringLiteral("%1  (%2行 × %3列)")
                                .arg(name)
                                .arg(fu.table.rows.size())
                                .arg(fu.table.header.size()));
        m_masterCombo->addItem(name);
    }
    m_removeBtn->setEnabled(!m_files.isEmpty());
    if (!m_files.isEmpty())
        m_masterCombo->setCurrentIndex(autoMasterIndex());
}

int ColumnExtractDialog::currentFileIndex() const
{
    return m_fileList->currentRow();
}

void ColumnExtractDialog::refreshColumnUI()
{
    const int fi = currentFileIndex();
    const bool has = (fi >= 0 && fi < m_files.size());

    m_timeCombo->clear();
    m_columnList->clear();
    if (!has) {
        m_timeCombo->setEnabled(false);
        m_columnList->setEnabled(false);
        return;
    }

    m_timeCombo->setEnabled(true);
    m_columnList->setEnabled(true);

    const auto &fu = m_files[fi];
    m_timeCombo->addItems(fu.table.header);
    m_timeCombo->setCurrentIndex(fu.timeColumn);

    m_columnList->blockSignals(true);
    for (int c = 0; c < fu.table.header.size(); ++c) {
        auto *item = new QListWidgetItem(fu.table.header[c], m_columnList);
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setCheckState(fu.checked.value(c) ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, c);
    }
    m_columnList->blockSignals(false);

    m_hintLabel->setText(QStringLiteral("已加载 %1 个文件").arg(m_files.size()));
}

void ColumnExtractDialog::onCurrentFileChanged(int /*row*/)
{
    refreshColumnUI();
}

void ColumnExtractDialog::onColumnItemChanged()
{
    const int fi = currentFileIndex();
    if (fi < 0 || fi >= m_files.size()) return;
    for (int i = 0; i < m_columnList->count(); ++i) {
        QListWidgetItem *item = m_columnList->item(i);
        const int c = item->data(Qt::UserRole).toInt();
        if (c >= 0 && c < m_files[fi].checked.size())
            m_files[fi].checked[c] = (item->checkState() == Qt::Checked);
    }
}

void ColumnExtractDialog::selectAllColumns(bool checked)
{
    const int fi = currentFileIndex();
    if (fi < 0 || fi >= m_files.size()) return;
    FileUi &fu = m_files[fi];
    std::fill(fu.checked.begin(), fu.checked.end(), checked);

    m_columnList->blockSignals(true);
    for (int i = 0; i < m_columnList->count(); ++i) {
        QListWidgetItem *item = m_columnList->item(i);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
    m_columnList->blockSignals(false);
}

int ColumnExtractDialog::autoMasterIndex() const
{
    int best = 0;
    for (int i = 1; i < m_files.size(); ++i) {
        if (m_files[i].table.rows.size() < m_files[best].table.rows.size())
            best = i;
    }
    return best;
}

// ── 最近邻索引：在有序时间数组里找离 t 最近的下标 ──
static int nearestIndex(const QVector<double> &times, double t)
{
    if (times.isEmpty()) return -1;
    auto it = std::lower_bound(times.begin(), times.end(), t);
    if (it == times.begin()) return 0;
    if (it == times.end()) return int(times.size()) - 1;
    const int i = int(it - times.begin());
    return (std::fabs(times[i] - t) < std::fabs(times[i - 1] - t)) ? i : (i - 1);
}

QString ColumnExtractDialog::buildMergedCsv(const QList<FileUi> &files, int masterIndex)
{
    if (files.isEmpty() || masterIndex < 0 || masterIndex >= files.size())
        return QString();

    const FileUi &master = files[masterIndex];

    struct OutCol { int fileIdx; int colIdx; QString name; };
    QList<OutCol> outCols;
    for (int fi = 0; fi < files.size(); ++fi) {
        const FileUi &fu = files[fi];
        const QString prefix = QFileInfo(fu.table.path).completeBaseName();
        for (int c = 0; c < fu.checked.size(); ++c) {
            if (!fu.checked.value(c)) continue;
            if (c == fu.timeColumn) continue;  // 时间列统一由主轴提供
            OutCol oc;
            oc.fileIdx = fi;
            oc.colIdx   = c;
            oc.name     = prefix + QLatin1Char('_') + fu.table.header.value(c);
            outCols.append(oc);
        }
    }

    QString timeName = master.table.header.value(master.timeColumn);
    if (timeName.isEmpty()) timeName = QStringLiteral("时间");

    QString csv;
    QTextStream s(&csv);

    s << timeName;
    for (const auto &oc : outCols)
        s << ',' << oc.name;
    s << '\n';

    for (int r = 0; r < master.table.rows.size(); ++r) {
        const double t = master.table.times.value(r);
        s << master.table.rows[r].value(master.timeColumn);
        for (const auto &oc : outCols) {
            const FileUi &fu = files[oc.fileIdx];
            int srcRow = r;
            if (oc.fileIdx != masterIndex)
                srcRow = nearestIndex(fu.table.times, t);
            QString val;
            if (srcRow >= 0 && srcRow < fu.table.rows.size())
                val = fu.table.rows[srcRow].value(oc.colIdx);
            s << ',' << val;
        }
        s << '\n';
    }

    return csv;
}

// ── 直接流式写入文件，避免大字符串占满内存；后台线程调用 ──
bool ColumnExtractDialog::writeMergedCsv(const QList<FileUi> &files, int masterIndex,
                                         const QString &outPath, QString *errorOut, qint64 *sizeOut)
{
    if (files.isEmpty() || masterIndex < 0 || masterIndex >= files.size()) {
        if (errorOut) *errorOut = QStringLiteral("参数错误");
        return false;
    }

    const FileUi &master = files[masterIndex];

    struct OutCol { int fileIdx; int colIdx; };
    QList<OutCol> outCols;
    QStringList header;
    for (int fi = 0; fi < files.size(); ++fi) {
        const FileUi &fu = files[fi];
        const QString prefix = QFileInfo(fu.table.path).completeBaseName();
        for (int c = 0; c < fu.checked.size(); ++c) {
            if (!fu.checked.value(c)) continue;
            if (c == fu.timeColumn) continue;
            OutCol oc;
            oc.fileIdx = fi;
            oc.colIdx  = c;
            outCols.append(oc);
            header.append(prefix + QLatin1Char('_') + fu.table.header.value(c));
        }
    }

    QString timeName = master.table.header.value(master.timeColumn);
    if (timeName.isEmpty()) timeName = QStringLiteral("时间");

    QFile f(outPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) *errorOut = f.errorString();
        return false;
    }
    QTextStream s(&f);

    s << timeName;
    for (const QString &h : header) s << ',' << h;
    s << '\n';

    const int totalRows = master.table.rows.size();
    for (int r = 0; r < totalRows; ++r) {
        const double t = master.table.times.value(r);
        s << master.table.rows[r].value(master.timeColumn);
        for (const OutCol &oc : outCols) {
            const FileUi &fu = files[oc.fileIdx];
            int srcRow = r;
            if (oc.fileIdx != masterIndex)
                srcRow = nearestIndex(fu.table.times, t);
            QString val;
            if (srcRow >= 0 && srcRow < fu.table.rows.size())
                val = fu.table.rows[srcRow].value(oc.colIdx);
            s << ',' << val;
        }
        s << '\n';
    }
    s.flush();
    f.close();

    if (sizeOut) *sizeOut = f.size();
    if (errorOut) errorOut->clear();
    return true;
}

// ── 异步导出入口（由"开始导出"按钮触发，不直接关闭对话框） ──
void ColumnExtractDialog::onOkClicked()
{
    if (m_exporting || m_loading) return;

    if (m_files.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先添加文本文件。"));
        return;
    }

    // 至少勾选一列非时间列
    bool anyCol = false;
    for (const auto &fu : m_files) {
        for (int c = 0; c < fu.checked.size(); ++c) {
            if (fu.checked.value(c) && c != fu.timeColumn) { anyCol = true; break; }
        }
        if (anyCol) break;
    }
    if (!anyCol) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请至少勾选一个输出列。"));
        return;
    }

    int master = m_masterCombo->currentIndex();
    if (master < 0 || master >= m_files.size())
        master = autoMasterIndex();

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存合并结果"),
        QStringLiteral("merged_columns.csv"),
        QStringLiteral("CSV文件 (*.csv);;所有文件 (*)"));
    if (path.isEmpty()) return;

    startExport(path, master);
}

void ColumnExtractDialog::startExport(const QString &path, int masterIdx)
{
    m_exportPath   = path;
    m_exportMaster = masterIdx;
    m_exporting    = true;

    // 拷贝一份 files 到后台线程（避免并发访问 UI 线程的 m_files）
    QList<FileUi> filesCopy = m_files;

    // 导出期间用"导出中"状态禁用所有控件，OK 按钮文字变"导出中..."
    m_loading = true;
    m_addBtn->setEnabled(false);
    m_removeBtn->setEnabled(false);
    m_selAllBtn->setEnabled(false);
    m_deselAllBtn->setEnabled(false);
    m_fileList->setEnabled(false);
    m_columnList->setEnabled(false);
    m_timeCombo->setEnabled(false);
    m_masterCombo->setEnabled(false);
    if (m_okBtn) {
        m_okBtn->setEnabled(false);
        m_okBtn->setText(QStringLiteral("导出中..."));
    }
    if (m_buttonBox) {
        auto *cancelBtn = m_buttonBox->button(QDialogButtonBox::Cancel);
        if (cancelBtn) cancelBtn->setEnabled(false);
    }
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    m_progressLbl->setVisible(true);
    m_progressLbl->setText(QStringLiteral("正在合并并写入 %1 ...").arg(QFileInfo(path).fileName()));
    QApplication::processEvents();

    auto *watcher = new QFutureWatcher<ExportResult>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<ExportResult>::finished,
            this, &ColumnExtractDialog::onExportFinished);

    watcher->setFuture(QtConcurrent::run(
        [filesCopy, masterIdx, path]() -> ExportResult {
            ExportResult r;
            r.ok = ColumnExtractDialog::writeMergedCsv(filesCopy, masterIdx, path, &r.error, &r.size);
            return r;
        }));
}

void ColumnExtractDialog::onExportFinished()
{
    auto *watcher = static_cast<QFutureWatcher<ExportResult>*>(m_watcher);
    ExportResult res = watcher->result();
    watcher->deleteLater();
    m_watcher = nullptr;
    m_exporting = false;

    // 恢复控件状态
    setBusy(false);
    if (m_okBtn) m_okBtn->setText(QStringLiteral("开始导出"));

    if (!res.ok) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("写入失败: %1").arg(res.error));
        return;
    }

    scheduleDataCleanup();   // 弹提示前先把大数据移走，关闭时不卡顿

    QMessageBox::information(this, QStringLiteral("导出成功"),
                             QStringLiteral("已保存 %1 KB → %2")
                                 .arg(res.size / 1024.0, 0, 'f', 1)
                                 .arg(m_exportPath));

    accept();
}

