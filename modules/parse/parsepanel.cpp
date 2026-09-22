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
#include <QStandardItemModel>
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
    connect(ui->protocolCombo, QOverload<int>::of(&QComboBox::activated),
            this, &ParsePanel::onProtocolComboActivated);
    setupProtocolCombo();
}

ParsePanel::~ParsePanel()
{
    delete ui;
}

// ═══════════════════════════════════════════════════
//  动态生成协议下拉框（按类分组）
// ═══════════════════════════════════════════════════

void ParsePanel::setupProtocolCombo()
{
    ui->protocolCombo->clear();
    m_comboToParser.clear();

    // 占位项（不可选）
    ui->protocolCombo->addItem(QStringLiteral("请选择协议..."));
    m_comboToParser.append(-1);

    QList<ParserBase*> parsers = ParserFactory::createAll();
    QString lastCategory;
    int parserIndex = 0;
    for (auto *parser : parsers) {
        const QString cat = parser->category();
        if (cat != lastCategory) {
            ui->protocolCombo->addItem(QStringLiteral("── %1 ──").arg(cat));
            m_comboToParser.append(-1);
            lastCategory = cat;
        }
        ui->protocolCombo->addItem(parser->buttonLabel());
        m_comboToParser.append(parserIndex);
        ++parserIndex;
        delete parser;
    }

    // 分类头 / 占位项设为不可选
    if (auto *model = qobject_cast<QStandardItemModel*>(ui->protocolCombo->model())) {
        for (int i = 0; i < m_comboToParser.size(); ++i) {
            if (m_comboToParser.at(i) < 0 && model->item(i))
                model->item(i)->setEnabled(false);
        }
    }

    ui->protocolCombo->setCurrentIndex(0);
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

    // 先彻底释放上一次的解析结果与原始数据，再读入新文件，
    // 避免新旧数据同时驻留导致内存峰值翻倍
    releaseParsedData();
    QByteArray().swap(m_rawData);

    m_rawData = tryDecodeHexText(file.readAll());
    file.close();

    ui->protocolCombo->setCurrentIndex(0);

    m_filePath = path;
    updateFileInfo();
}

// 释放上一次解析占用的内存。
// 注意：clear() 只析构元素、不释放已分配的 capacity，
// 大文件（几十万帧）下残留会很明显，因此用 swap 与临时空对象交换强制归还。
void ParsePanel::releaseParsedData()
{
    ui->outputTextEdit->clear();
    QString().swap(m_cachedFullText);
    QString().swap(m_cachedCsvPath);
    QList<ParsedFrame>().swap(m_cachedFrames);
    m_activeProtocolId = ProtocolID::Unknown;
}

void ParsePanel::updateFileInfo()
{
    QFileInfo fi(m_filePath);
    ui->filePathLabel->setText(fi.fileName());
    ui->fileInfoLabel->setText(
        QStringLiteral("大小:%1KB").arg(fi.size() / 1024.0, 0, 'f', 1));
}

void ParsePanel::setComboEnabled(bool enabled)
{
    ui->protocolCombo->setEnabled(enabled);
}

// ═══════════════════════════════════════════════════
//  协议下拉选择 → 后台线程解析
// ═══════════════════════════════════════════════════

void ParsePanel::onProtocolComboActivated(int index)
{
    const int parserIdx = (index >= 0 && index < m_comboToParser.size())
                              ? m_comboToParser.at(index) : -1;
    if (parserIdx < 0)
        return;  // 分类头 / 占位项不可选

    if (m_rawData.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先加载文件。"));
        ui->protocolCombo->setCurrentIndex(0);
        return;
    }

    bool skipCrc = !ui->crcCheckBox->isChecked();
    bool isBlf = m_filePath.endsWith(".blf", Qt::CaseInsensitive);

    // 快速校验：检查协议/文件类型匹配
    {
        QList<ParserBase*> tmp = ParserFactory::createAll();
        if (parserIdx < tmp.size()) {
            ParserBase *p = tmp[parserIdx];
            if (isBlf && p->protocolId() != ProtocolID::Can_RTK) {
                QMessageBox::information(this, QStringLiteral("提示"),
                    QStringLiteral("BLF 文件请使用 联试_CAN981 协议解析。"));
                ui->protocolCombo->setCurrentIndex(0);
                qDeleteAll(tmp);
                return;
            }
            if (!isBlf && p->protocolId() == ProtocolID::Can_RTK) {
                QMessageBox::information(this, QStringLiteral("提示"),
                    QStringLiteral("联试_CAN981 仅支持 BLF 文件。"));
                ui->protocolCombo->setCurrentIndex(0);
                qDeleteAll(tmp);
                return;
            }
        }
        qDeleteAll(tmp);
    }

    // 禁用下拉框，显示「解析中...」
    m_parsing = true;
    setComboEnabled(false);
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
        setComboEnabled(true);
    });

    QByteArray data = m_rawData;
    QString fpath = m_filePath;
    watcher->setFuture(QtConcurrent::run([=]() {
        return doParse(parserIdx, skipCrc, data, fpath);
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
    releaseParsedData();
    ui->protocolCombo->setCurrentIndex(0);
}
