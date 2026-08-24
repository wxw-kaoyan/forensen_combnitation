#include "downsampledialog.h"

#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QCloseEvent>
#include <QIntValidator>
#include <QApplication>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

// 类内已初始化，此处提供定义以满足 ODR-use（QtConcurrent::run 以引用绑定取址）
const int DownsampleDialog::PREVIEW_ROWS;

// ── 识别分隔符 ──
static QChar detectDelimiter(const QString &line)
{
    int comma = line.count(',');
    int tab   = line.count('\t');
    int space = line.count(' ');
    int multi = line.count(QStringLiteral("  "));
    int semi  = line.count(';');
    // 多空格当作空格分隔
    QChar best = ',';
    int   bc   = comma;
    if (tab   > bc) { best = '\t'; bc = tab; }
    if (semi  > bc) { best = ';';  bc = semi; }
    if (space > bc && multi > 0) { best = ' '; }
    return best;
}

// ── 按分隔符拆分一行 ──
static QStringList splitLine(const QString &line, QChar delim)
{
    if (delim == ' ') {
        // 空格分隔：用正则压缩连续空白
        return line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    }
    return line.split(delim);
}

// ════════════════════════════════════════════════════════════════

DownsampleDialog::DownsampleDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("数据降采样 (选择起始基准行)"));
    resize(900, 600);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // ── 1. 文件选择 ──
    auto *fileRow = new QHBoxLayout;
    fileRow->addWidget(new QLabel(QStringLiteral("源文件:"), this));
    m_fileEdit = new QLineEdit(this);
    m_fileEdit->setPlaceholderText(QStringLiteral("选择 CSV/TXT 文件（首行为表头）..."));
    m_browseBtn = new QPushButton(QStringLiteral("浏览..."), this);
    fileRow->addWidget(m_fileEdit, 1);
    fileRow->addWidget(m_browseBtn);
    root->addLayout(fileRow);

    // ── 2. 参数行（倍数 + 起始行） ──
    auto *paramRow = new QHBoxLayout;
    paramRow->addWidget(new QLabel(QStringLiteral("降采样倍数:"), this));
    m_factorCombo = new QComboBox(this);
    m_factorCombo->addItem(QStringLiteral("÷10 (默认, 200Hz→20Hz)"), 10);
    m_factorCombo->addItem(QStringLiteral("÷20 (200Hz→10Hz)"), 20);
    m_factorCombo->addItem(QStringLiteral("÷50 (200Hz→4Hz)"),  50);
    m_factorCombo->addItem(QStringLiteral("÷100 (200Hz→2Hz)"), 100);
    m_factorCombo->addItem(QStringLiteral("÷200 (200Hz→1Hz)"), 200);
    m_factorCombo->addItem(QStringLiteral("自定义..."),         0);
    paramRow->addWidget(m_factorCombo);
    m_customEdit = new QLineEdit(this);
    m_customEdit->setPlaceholderText(QStringLiteral("N"));
    m_customEdit->setFixedWidth(70);
    m_customEdit->setValidator(new QIntValidator(2, 1000000, this));
    m_customEdit->setVisible(false);
    paramRow->addWidget(m_customEdit);
    paramRow->addSpacing(16);

    paramRow->addWidget(new QLabel(QStringLiteral("起始行(基准):"), this));
    m_startSpin = new QSpinBox(this);
    m_startSpin->setRange(1, 1);
    m_startSpin->setValue(1);
    m_startSpin->setFixedWidth(80);
    paramRow->addWidget(m_startSpin);
    paramRow->addStretch();
    root->addLayout(paramRow);

    // ── 3. 提示 ──
    m_infoLabel = new QLabel(QStringLiteral("选择文件后将显示前 %1 行数据，点击表格某行或调整 SpinBox 选择起始基准行，之后每 N 行保留 1 行").arg(PREVIEW_ROWS), this);
    m_infoLabel->setStyleSheet(QStringLiteral("color:#617487;"));
    m_infoLabel->setWordWrap(true);
    root->addWidget(m_infoLabel);

    // ── 4. 预览表格 ──
    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setDefaultSectionSize(20);
    root->addWidget(m_table, 1);

    // ── 5. 进度条 ──
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    root->addWidget(m_progressBar);

    // ── 6. 按钮 ──
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_startBtn = new QPushButton(QStringLiteral("开始降采样"), this);
    m_startBtn->setEnabled(false);
    m_startBtn->setMinimumWidth(120);
    m_startBtn->setStyleSheet(
        QStringLiteral("QPushButton{background:#17324d;color:white;border-radius:4px;font-size:13px;font-weight:600;padding:6px 20px;}"
                       "QPushButton:hover{background:#1e426b;}"
                       "QPushButton:disabled{background:#aaa;}"));
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    cancelBtn->setMinimumWidth(80);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(cancelBtn);
    root->addLayout(btnRow);

    // ── 信号 ──
    connect(m_browseBtn, &QPushButton::clicked, this, &DownsampleDialog::browseFile);
    connect(m_fileEdit, &QLineEdit::editingFinished, this, &DownsampleDialog::onFileEntered);
    connect(m_factorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DownsampleDialog::onFactorChanged);
    connect(m_startSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DownsampleDialog::onStartRowChanged);
    connect(m_table, &QTableWidget::cellClicked, this, &DownsampleDialog::onCellClicked);
    connect(m_startBtn, &QPushButton::clicked, this, &DownsampleDialog::onStartClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &DownsampleDialog::onCancelClicked);
}

DownsampleDialog::~DownsampleDialog() = default;

void DownsampleDialog::browseFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择要降采样的文件"), QString(),
        QStringLiteral("数据文件 (*.csv *.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;
    m_fileEdit->setText(path);
    onFileEntered();
}

void DownsampleDialog::onFileEntered()
{
    const QString path = m_fileEdit->text().trimmed();
    if (path.isEmpty()) return;
    if (!QFile::exists(path)) {
        m_infoLabel->setText(QStringLiteral("<font color=red>文件不存在: %1</font>").arg(path));
        return;
    }
    loadPreview(path);
}

void DownsampleDialog::onFactorChanged(int idx)
{
    const int v = m_factorCombo->itemData(idx).toInt();
    m_customEdit->setVisible(v == 0);
    if (v > 0) m_factor = v;
    else {
        bool ok = false;
        int cv = m_customEdit->text().trimmed().toInt(&ok);
        if (ok && cv >= 2) m_factor = cv;
    }
}

void DownsampleDialog::onStartRowChanged(int row)
{
    m_startRow = row;
    // 同步高亮表格对应行
    if (m_table->rowCount() > 0) {
        int r = row - 1;  // 表格第0行 = 数据第1行
        if (r >= 0 && r < m_table->rowCount()) {
            m_table->blockSignals(true);
            m_table->selectRow(r);
            m_table->scrollToItem(m_table->item(r, 0));
            m_table->blockSignals(false);
        }
    }
}

void DownsampleDialog::onCellClicked(int row, int /*col*/)
{
    // 数据第 row+1 行
    m_startSpin->blockSignals(true);
    m_startSpin->setValue(row + 1);
    m_startSpin->blockSignals(false);
    m_startRow = row + 1;
}

void DownsampleDialog::setBusy(bool busy, const QString &msg)
{
    m_busy = busy;
    m_fileEdit->setEnabled(!busy);
    m_browseBtn->setEnabled(!busy);
    m_factorCombo->setEnabled(!busy);
    m_customEdit->setEnabled(!busy);
    m_startSpin->setEnabled(!busy);
    m_table->setEnabled(!busy);
    m_startBtn->setEnabled(!busy && !m_header.isEmpty());
    m_progressBar->setVisible(busy);
    if (!msg.isEmpty()) m_infoLabel->setText(msg);
    QApplication::processEvents();
}

void DownsampleDialog::closeEvent(QCloseEvent *e)
{
    if (m_busy) { e->ignore(); return; }
    QDialog::closeEvent(e);
}

void DownsampleDialog::onCancelClicked()
{
    if (m_busy) return;
    reject();
}

// ── 后台加载预览（只读前 N 行，不读全文件） ──
void DownsampleDialog::loadPreview(const QString &path)
{
    setBusy(true, QStringLiteral("正在读取文件前 %1 行...").arg(PREVIEW_ROWS));

    auto *w = new QFutureWatcher<PreviewData>(this);
    m_watcher = w;
    connect(w, &QFutureWatcher<PreviewData>::finished,
            this, &DownsampleDialog::onPreviewLoaded);
    w->setFuture(QtConcurrent::run(&DownsampleDialog::readPreview, path, PREVIEW_ROWS));
}

DownsampleDialog::PreviewData DownsampleDialog::readPreview(const QString &path, int nRows)
{
    PreviewData pd;
    pd.path = path;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        pd.error = QStringLiteral("无法打开文件: %1").arg(f.errorString());
        return pd;
    }
    QTextStream sin(&f);
    sin.setAutoDetectUnicode(true);

    QString headerLine;
    if (!sin.readLineInto(&headerLine)) {
        pd.error = QStringLiteral("文件为空");
        return pd;
    }
    pd.delim  = detectDelimiter(headerLine);
    pd.header = splitLine(headerLine, pd.delim);

    // 读 nRows 行预览
    for (int i = 0; i < nRows; ++i) {
        QString line;
        if (!sin.readLineInto(&line)) break;
        pd.rows.append(splitLine(line, pd.delim));
    }
    f.close();
    return pd;
}

void DownsampleDialog::onPreviewLoaded()
{
    auto *w = static_cast<QFutureWatcher<PreviewData>*>(m_watcher);
    PreviewData pd = w->result();
    w->deleteLater();
    m_watcher = nullptr;

    if (!pd.error.isEmpty()) {
        setBusy(false);
        m_infoLabel->setText(QStringLiteral("<font color=red>%1</font>").arg(pd.error));
        m_table->clear();
        m_table->setRowCount(0);
        m_startBtn->setEnabled(false);
        return;
    }

    m_srcPath = pd.path;
    m_delim   = pd.delim;
    m_header  = pd.header;
    populateTable(pd);

    // 设置起始行 SpinBox 上限（预览到的行数，如果文件比 PREVIEW_ROWS 长，允许用户输入更大值，这里先放宽）
    m_startSpin->setRange(1, 1000000000);
    m_startSpin->setValue(1);
    m_startRow = 1;

    setBusy(false);
    m_startBtn->setEnabled(true);
    m_infoLabel->setText(QStringLiteral(
        "共 %1 列，显示前 %2 行（第1行=数据起始）。点击表格任意行作为降采样基准起点，之后每 N 行保留 1 行。")
        .arg(pd.header.size()).arg(pd.rows.size()));
}

void DownsampleDialog::populateTable(const PreviewData &pd)
{
    m_table->blockSignals(true);
    m_table->clear();
    m_table->setColumnCount(pd.header.size());
    m_table->setHorizontalHeaderLabels(pd.header);
    m_table->setRowCount(pd.rows.size());

    for (int r = 0; r < pd.rows.size(); ++r) {
        const QStringList &cols = pd.rows[r];
        for (int c = 0; c < pd.header.size(); ++c) {
            QString val = (c < cols.size()) ? cols[c] : QString();
            auto *item = new QTableWidgetItem(val);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(r, c, item);
        }
        // 行表头显示行号（1-based）
        m_table->setVerticalHeaderItem(r, new QTableWidgetItem(QString::number(r + 1)));
    }

    // 列宽自适应（限制最大宽度避免过宽）
    m_table->resizeColumnsToContents();
    for (int c = 0; c < m_table->columnCount(); ++c) {
        if (m_table->columnWidth(c) > 200) m_table->setColumnWidth(c, 200);
    }
    m_table->blockSignals(false);
}

// ── 开始降采样 ──
void DownsampleDialog::onStartClicked()
{
    if (m_busy || m_header.isEmpty()) return;

    // 读取自定义倍数
    if (m_factorCombo->currentData().toInt() == 0) {
        bool ok = false;
        int cv = m_customEdit->text().trimmed().toInt(&ok);
        if (!ok || cv < 2) {
            QMessageBox::warning(this, QStringLiteral("参数错误"),
                                 QStringLiteral("请输入 ≥2 的自定义降采样倍数。"));
            return;
        }
        m_factor = cv;
    }

    if (m_startRow < 1) {
        QMessageBox::warning(this, QStringLiteral("参数错误"),
                             QStringLiteral("起始行必须 ≥1。"));
        return;
    }

    QFileInfo fi(m_srcPath);
    const QString defName = QStringLiteral("%1_ds%2_from%3.csv")
                                .arg(fi.completeBaseName()).arg(m_factor).arg(m_startRow);
    m_dstPath = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存降采样结果"),
        fi.absolutePath() + "/" + defName,
        QStringLiteral("CSV文件 (*.csv);;所有文件 (*)"));
    if (m_dstPath.isEmpty()) return;

    setBusy(true, QStringLiteral("正在降采样 (÷%1, 起始行 %2)...").arg(m_factor).arg(m_startRow));

    auto *w = new QFutureWatcher<ExportResult>(this);
    m_watcher = w;
    connect(w, &QFutureWatcher<ExportResult>::finished,
            this, &DownsampleDialog::onExportFinished);
    w->setFuture(QtConcurrent::run(&DownsampleDialog::doDownsample,
                                   m_srcPath, m_dstPath, m_delim,
                                   m_factor, m_startRow));
}

void DownsampleDialog::onExportFinished()
{
    auto *w = static_cast<QFutureWatcher<ExportResult>*>(m_watcher);
    ExportResult r = w->result();
    w->deleteLater();
    m_watcher = nullptr;
    setBusy(false);

    if (!r.ok) {
        QMessageBox::warning(this, QStringLiteral("降采样失败"), r.error);
        return;
    }

    m_infoLabel->setText(QStringLiteral(
        "完成: 输入 %1 行 → 输出 %2 行 (÷%3, 起始行 %4), 文件 %5 KB")
        .arg(r.inRows).arg(r.outRows).arg(m_factor).arg(m_startRow)
        .arg(r.size / 1024.0, 0, 'f', 1));

    QMessageBox::information(this, QStringLiteral("降采样完成"),
        QStringLiteral("输入: %1 行\n输出: %2 行 (÷%3)\n起始行: %4\n保存: %5\n大小: %6 KB")
            .arg(r.inRows).arg(r.outRows).arg(m_factor).arg(m_startRow)
            .arg(m_dstPath).arg(r.size / 1024.0, 0, 'f', 1));

    accept();
}

// ── 后台流式降采样 ──
// startRow: 1-based 数据行号（1 = 表头下第一行）
// 保留：第 startRow 行，第 startRow+factor 行，第 startRow+2*factor 行 ...
DownsampleDialog::ExportResult DownsampleDialog::doDownsample(const QString &src,
                                                              const QString &dst,
                                                              QChar delim,
                                                              int factor,
                                                              int startRow)
{
    ExportResult r;
    QFile fin(src);
    if (!fin.open(QIODevice::ReadOnly | QIODevice::Text)) {
        r.ok = false; r.error = QStringLiteral("无法打开源文件: %1").arg(fin.errorString());
        return r;
    }
    QFile fout(dst);
    if (!fout.open(QIODevice::WriteOnly | QIODevice::Text)) {
        r.ok = false; r.error = QStringLiteral("无法写入目标: %1").arg(fout.errorString());
        fin.close();
        return r;
    }

    QTextStream sin(&fin);
    QTextStream sout(&fout);
    sin.setAutoDetectUnicode(true);

    // 表头原样输出
    QString headerLine;
    if (!sin.readLineInto(&headerLine)) {
        r.ok = false; r.error = QStringLiteral("源文件为空");
        fin.close(); fout.close();
        return r;
    }
    sout << headerLine << '\n';

    qint64 dataRow = 0;   // 当前读取到第几条数据（1-based）
    qint64 outRows = 0;
    qint64 inRows  = 0;
    QString line;

    // 跳过 startRow 之前的数据行（不写入），从 startRow 开始写入，之后每隔 factor-1 行写入一行
    while (sin.readLineInto(&line)) {
        ++inRows;
        ++dataRow;
        if (dataRow >= startRow && ((dataRow - startRow) % factor == 0)) {
            sout << line << '\n';
            ++outRows;
        }
    }

    sout.flush();
    fout.close();
    fin.close();

    r.ok      = true;
    r.inRows  = inRows;
    r.outRows = outRows;
    r.size    = fout.size();
    return r;
}
