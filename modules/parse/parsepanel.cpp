#include "parsepanel.h"
#include "ui_parsepanel.h"
#include "modules/parse/parser/parserbase.h"
#include "modules/parse/parser/parserfactory.h"
#include "modules/parse/parser/debugstream/debugframeparser.h"
#include "modules/parse/parser/debugstream/eb614debugparser.h"
#include "modules/parse/parser/single/protocolregistry.h"
#include "columnextract.h"
#include "downsampledialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QTemporaryFile>

// ── 将 hex 文本转换为二进制；非 hex 文本原样返回 ──
static int hexDigitValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static QByteArray tryDecodeHexText(const QByteArray &raw)
{
    QByteArray hex;
    hex.reserve(raw.size());
    for (char c : raw) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == ',' || c == ';' || c == ':') {
            continue;  // 分隔符
        }
        if (hexDigitValue(c) < 0) {
            return raw;  // 含非 hex 字符 → 视为二进制，原样返回
        }
        hex.append(c);
    }
    if (hex.isEmpty() || (hex.size() % 2) != 0) {
        return raw;
    }
    QByteArray bin;
    bin.reserve(hex.size() / 2);
    for (int i = 0; i + 1 < hex.size(); i += 2) {
        int hi = hexDigitValue(hex.at(i));
        int lo = hexDigitValue(hex.at(i + 1));
        bin.append(static_cast<char>((hi << 4) | lo));
    }
    return bin;
}

// ── 通用：按帧 ID 列表拆分导出多个 CSV（AG调试流 / 614E-B 共用） ──
static int exportFrameCsvs(const QList<ParsedFrame> &frames, const QString &dir,
                           const QList<ProtocolID> &pids, const QList<QString> &names,
                           QString (*headerFn)(ProtocolID),
                           QString (*formatFn)(ProtocolID, const ParsedFrame &))
{
    int written = 0;
    for (int i = 0; i < pids.size(); ++i) {
        QString text = headerFn(pids[i]);
        for (const auto &f : frames) {
            if (f.protocol == pids[i])
                text += formatFn(pids[i], f);
        }
        QFile file(dir + QLatin1Char('/') + names[i]);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(text.toUtf8());
            file.close();
            ++written;
        }
    }
    return written;
}

// ═══════════════════════════════════════════════════════════════
ParsePanel::ParsePanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ParsePanel)
{
    ui->setupUi(this);

    connect(ui->loadFileButton, &QPushButton::clicked, this, &ParsePanel::onLoadFileClicked);
    connect(ui->exportButton,   &QPushButton::clicked, this, &ParsePanel::onExportClicked);
    connect(ui->clearButton,    &QPushButton::clicked, this, &ParsePanel::onClearClicked);
    connect(ui->newCodeButton,  &QPushButton::clicked, this, &ParsePanel::onNewCodeClicked);
    connect(ui->downsampleButton, &QPushButton::clicked, this, &ParsePanel::onDownsampleClicked);
    setupProtocolButtons();
}

ParsePanel::~ParsePanel()
{
    delete ui;
}

// ═══════════════════════════════════════════════════
//  动态生成协议按钮
// ═══════════════════════════════════════════════════

void ParsePanel::setupProtocolButtons()
{
    QList<ParserBase*> parsers = ParserFactory::createAll();
    QLayout *layout = ui->filterLayout;

    for (auto *parser : parsers) {
        auto *btn = new QPushButton(parser->buttonLabel(), this);
        btn->setMinimumSize(90, 32);
        btn->setStyleSheet(
            "QPushButton{background:#e8edf2;color:#17324d;border:1px solid #bcc7d4;"
            "border-radius:4px;font-size:12px;font-weight:600;padding:0 12px;}"
            "QPushButton:hover{background:#d0dae5;}"
            "QPushButton:checked{background:#17324d;color:white;border-color:#17324d;}");
        btn->setCheckable(true);
        btn->setProperty("parserIndex", m_protoButtons.size());
        connect(btn, &QPushButton::clicked, this, &ParsePanel::onProtocolButtonClicked);
        layout->addWidget(btn);
        m_protoButtons.append(btn);
        delete parser;
    }
}

// ═══════════════════════════════════════════════════
//  加载文件
// ═══════════════════════════════════════════════════

void ParsePanel::onLoadFileClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择二进制数据文件"),
        QString(), QStringLiteral("数据文件 (*.bin *.dat *.log *.raw *.blf);;BLF文件 (*.blf);;所有文件 (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), QStringLiteral("无法打开文件。"));
        return;
    }
    m_rawData = tryDecodeHexText(file.readAll());
    file.close();

    m_filePath.clear();
    ui->outputTextEdit->clear();
    m_cachedFullText.clear();
    m_cachedCsvPath.clear();
    m_cachedFrames.clear();
    m_activeProtocolId = ProtocolID::Unknown;

    for (auto *b : m_protoButtons)
        b->setChecked(false);

    m_filePath = path;
    updateFileInfo();
}

void ParsePanel::updateFileInfo()
{
    QFileInfo fi(m_filePath);
    ui->filePathLabel->setText(fi.fileName());
    ui->fileInfoLabel->setText(
        QStringLiteral("大小:%1KB").arg(fi.size() / 1024.0, 0, 'f', 1));
}

void ParsePanel::setButtonsEnabled(bool enabled)
{
    for (auto *b : m_protoButtons)
        b->setEnabled(enabled);
}

// ═══════════════════════════════════════════════════
//  协议按钮点击 → 后台线程解析
// ═══════════════════════════════════════════════════

void ParsePanel::onProtocolButtonClicked()
{
    if (m_rawData.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先加载文件。"));
        auto *btn = qobject_cast<QPushButton*>(sender());
        if (btn) btn->setChecked(false);
        return;
    }

    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    // 互斥
    for (auto *b : m_protoButtons) {
        if (b != btn) b->setChecked(false);
    }

    if (!btn->isChecked()) {
        ui->outputTextEdit->clear();
        m_cachedFrames.clear();
        m_activeProtocolId = ProtocolID::Unknown;
        return;
    }

    int idx = btn->property("parserIndex").toInt();
    if (idx < 0 || idx >= m_protoButtons.size()) return;

    bool skipCrc = !ui->crcCheckBox->isChecked();
    bool isBlf = m_filePath.endsWith(".blf", Qt::CaseInsensitive);

    // 快速校验：检查协议/文件类型匹配
    {
        QList<ParserBase*> tmp = ParserFactory::createAll();
        if (idx < tmp.size()) {
            ParserBase *p = tmp[idx];
            if (isBlf && p->protocolId() != ProtocolID::Can_RTK) {
                QMessageBox::information(this, QStringLiteral("提示"),
                    QStringLiteral("BLF 文件请使用 联试_CAN981 协议解析。"));
                btn->setChecked(false);
                qDeleteAll(tmp);
                return;
            }
            if (!isBlf && p->protocolId() == ProtocolID::Can_RTK) {
                QMessageBox::information(this, QStringLiteral("提示"),
                    QStringLiteral("联试_CAN981 仅支持 BLF 文件。"));
                btn->setChecked(false);
                qDeleteAll(tmp);
                return;
            }
        }
        qDeleteAll(tmp);
    }

    // 禁用按钮，显示「解析中...」
    m_parsing = true;
    setButtonsEnabled(false);
    ui->outputTextEdit->setPlainText(QStringLiteral("解析中..."));
    ui->fileInfoLabel->setText(QStringLiteral("解析中..."));

    // 后台线程解析
    auto *watcher = new QFutureWatcher<ParseResult>(this);
    connect(watcher, &QFutureWatcher<ParseResult>::finished, this,
            [this, watcher]() {
        ParseResult result = watcher->result();
        ui->outputTextEdit->setPlainText(result.displayText);
        ui->fileInfoLabel->setText(result.infoText);
        m_cachedFullText = result.fullText;
        m_cachedCsvPath  = result.externalCsvPath;
        m_cachedFrames   = result.frames;
        m_activeProtocolId = result.protocolId;
        watcher->deleteLater();
        m_parsing = false;
        setButtonsEnabled(true);
    });

    QByteArray data = m_rawData;
    QString fpath = m_filePath;
    watcher->setFuture(QtConcurrent::run([=]() {
        return doParse(idx, skipCrc, data, fpath);
    }));
}

// ═══════════════════════════════════════════════════
//  后台线程执行的解析逻辑
// ═══════════════════════════════════════════════════

ParseResult ParsePanel::doParse(int protocolIndex, bool skipCrc,
                                const QByteArray &rawData,
                                const QString &filePath)
{
    QList<ParserBase*> parsers = ParserFactory::createAll();
    if (protocolIndex < 0 || protocolIndex >= parsers.size()) {
        qDeleteAll(parsers);
        ParseResult result;
        result.displayText = QStringLiteral("（解析器索引无效）\n");
        result.infoText = QStringLiteral("错误");
        return result;
    }

    ParserBase *parser = parsers[protocolIndex];
    parser->setSkipCrc(skipCrc);

    // 统一入口：多态分发，doParse 不识别具体协议类型
    ParseResult result = parser->parseToResult(rawData, filePath);

    qDeleteAll(parsers);
    return result;
}

// ═══════════════════════════════════════════════════
//  New Code — 字段选择 + 重新编码导出
// ═══════════════════════════════════════════════════

void ParsePanel::onNewCodeClicked()
{
    ColumnExtractDialog dlg(this);
    dlg.exec();
}

void ParsePanel::onDownsampleClicked()
{
    DownsampleDialog dlg(this);
    dlg.exec();
}

// ═══════════════════════════════════════════════════
//  导出 / 清空
// ═══════════════════════════════════════════════════

void ParsePanel::onExportClicked()
{
    // 凯芯数据：直接复制临时 CSV 到用户选择的位置
    if (isKaixinProtocol(m_activeProtocolId) && !m_cachedCsvPath.isEmpty()) {
        QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出凯芯 CSV"),
            QStringLiteral("kaixin_data.csv"),
            QStringLiteral("CSV文件 (*.csv);;所有文件 (*)"));
        if (path.isEmpty()) return;
        if (QFile::copy(m_cachedCsvPath, path)) {
            QFile src(m_cachedCsvPath);
            qint64 size = src.size();
            QMessageBox::information(this, QStringLiteral("导出成功"),
                QStringLiteral("已保存 %1 KB").arg(size / 1024.0, 0, 'f', 1));
        } else {
            // 可能目标已存在，先删再试
            QFile::remove(path);
            if (QFile::copy(m_cachedCsvPath, path)) {
                QFile src(m_cachedCsvPath);
                QMessageBox::information(this, QStringLiteral("导出成功"),
                    QStringLiteral("已保存 %1 KB").arg(src.size() / 1024.0, 0, 'f', 1));
            } else {
                QMessageBox::warning(this, QStringLiteral("导出失败"),
                    QStringLiteral("无法写入: %1").arg(path));
            }
        }
        return;
    }

    // DEBUG 调试数据：按帧 ID 导出 6 个 CSV
    if (isDebugProtocol(m_activeProtocolId)) {
        if (m_cachedFrames.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("请先解析调试数据后再导出。"));
            return;
        }
        QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择 DEBUG 数据导出目录"));
        if (dir.isEmpty()) return;
        const QList<ProtocolID> pids = {
            ProtocolID::Debug_IMU, ProtocolID::Debug_GPS, ProtocolID::Debug_DRTK,
            ProtocolID::Debug_OD, ProtocolID::Debug_KF, ProtocolID::Debug_Param
        };
        const QList<QString> names = {
            QStringLiteral("debug_0x51_组合导航IMU.csv"),
            QStringLiteral("debug_0x52_GNSS定位.csv"),
            QStringLiteral("debug_0x53_双天线RTK.csv"),
            QStringLiteral("debug_0x54_轮速.csv"),
            QStringLiteral("debug_0x55_卡尔曼滤波.csv"),
            QStringLiteral("debug_0x56_参数.csv"),
        };
        int written = exportFrameCsvs(m_cachedFrames, dir, pids, names,
                                      &DebugFrameParser::csvHeaderFor,
                                      &DebugFrameParser::formatFrameFor);
        QMessageBox::information(this, QStringLiteral("导出成功"),
                                 QStringLiteral("已导出 %1 个 CSV 到 %2")
                                     .arg(written).arg(dir));
        return;
    }

    // 614E-B 调试数据：按帧 ID 导出 3 个 CSV
    if (isEb614Protocol(m_activeProtocolId)) {
        if (m_cachedFrames.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("请先解析 614E-B 调试数据后再导出。"));
            return;
        }
        QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择 614E-B 数据导出目录"));
        if (dir.isEmpty()) return;
        const QList<ProtocolID> pids = {
            ProtocolID::Eb614_IMU, ProtocolID::Eb614_GPS, ProtocolID::Eb614_KF
        };
        const QList<QString> names = {
            QStringLiteral("614EB_0x0101_IMU.csv"),
            QStringLiteral("614EB_0x0111_GNSS.csv"),
            QStringLiteral("614EB_0x0131_KF.csv"),
        };
        int written = exportFrameCsvs(m_cachedFrames, dir, pids, names,
                                      &Eb614DebugParser::csvHeaderFor,
                                      &Eb614DebugParser::formatFrameFor);
        QMessageBox::information(this, QStringLiteral("导出成功"),
                                 QStringLiteral("已导出 %1 个 CSV 到 %2")
                                     .arg(written).arg(dir));
        return;
    }

    QString text = m_cachedFullText;
    if (text.isEmpty()) {
        text = ui->outputTextEdit->toPlainText();
        if (text.isEmpty()) return;
    }

    // 优先按当前协议描述符判断是否为 CSV；未知/外部协议回退到文本前缀判断
    const ProtocolDescriptor *desc = findProtocol(m_activeProtocolId);
    bool isCsv = (desc && !desc->csvHeader.isEmpty());
    if (!isCsv) {
        isCsv = text.startsWith("UTC天数") ||
                text.startsWith("Timer(us)") ||
                text.startsWith("GPS(ms)") ||
                text.startsWith("GPS周秒");
    }
    QString defaultExt = isCsv ? QStringLiteral(".csv") : QStringLiteral(".txt");
    QString defaultPath = QStringLiteral("parse_result") + defaultExt;

    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出解析结果"), defaultPath,
        isCsv ? QStringLiteral("CSV文件 (*.csv);;所有文件 (*)")
              : QStringLiteral("文本文件 (*.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入: %1").arg(f.errorString()));
        return;
    }
    f.write(text.toUtf8());
    f.close();

    QMessageBox::information(this, QStringLiteral("导出成功"),
                             QStringLiteral("已保存 %1 KB").arg(f.size() / 1024.0, 0, 'f', 1));
}

void ParsePanel::onClearClicked()
{
    ui->outputTextEdit->clear();
    m_cachedFullText.clear();
    m_cachedCsvPath.clear();
    m_cachedFrames.clear();
    m_activeProtocolId = ProtocolID::Unknown;
    for (auto *b : m_protoButtons)
        b->setChecked(false);
}
