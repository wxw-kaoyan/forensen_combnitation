#ifndef DOWNSAMPLEDIALOG_H
#define DOWNSAMPLEDIALOG_H

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QLabel;
class QTableWidget;
class QProgressBar;
class QPushButton;
class QFutureWatcherBase;

// ════════════════════════════════════════════════════════════════
//  降采样对话框：
//    1. 选择 CSV/TXT 文件 → 预览前 50 行数据
//    2. 用户选择起始行（基准点）和降采样倍数
//    3. 从起始行开始每隔 N 行取 1 行，异步写入新文件
// ════════════════════════════════════════════════════════════════
class DownsampleDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DownsampleDialog(QWidget *parent = nullptr);
    ~DownsampleDialog() override;

private slots:
    void browseFile();
    void onFileEntered();
    void onPreviewLoaded();
    void onFactorChanged(int idx);
    void onStartRowChanged(int row);
    void onCellClicked(int row, int col);
    void onStartClicked();
    void onCancelClicked();
    void onExportFinished();

private:
    struct PreviewData {
        QString     path;
        QString     error;
        QChar       delim = ',';
        QStringList header;           // 表头列
        QList<QStringList> rows;      // 前若干行（不含表头）
        int         totalRows = 0;    // 估算总行数（-1=未知）
    };
    struct ExportResult {
        bool   ok = false;
        QString error;
        qint64 inRows = 0, outRows = 0;
        qint64 size = 0;
    };

    QLineEdit     *m_fileEdit     = nullptr;
    QComboBox     *m_factorCombo  = nullptr;
    QLineEdit     *m_customEdit   = nullptr;
    QSpinBox      *m_startSpin    = nullptr;
    QLabel        *m_infoLabel    = nullptr;
    QTableWidget  *m_table        = nullptr;
    QProgressBar  *m_progressBar  = nullptr;
    QPushButton   *m_startBtn     = nullptr;
    QPushButton   *m_browseBtn    = nullptr;
    bool           m_busy = false;

    QFutureWatcherBase *m_watcher = nullptr;

    QString        m_srcPath;
    QString        m_dstPath;
    int            m_factor     = 10;   // 降采样倍数
    int            m_startRow   = 1;    // 起始数据行（1-based，1 = 表头下第一行）
    QChar          m_delim      = ',';
    QStringList    m_header;            // 完整表头（缓存，导出台词用）

    static const int PREVIEW_ROWS = 50;

    void loadPreview(const QString &path);
    void populateTable(const PreviewData &pd);
    void setBusy(bool busy, const QString &msg = QString());
    void closeEvent(class QCloseEvent *e) override;

    static PreviewData  readPreview(const QString &path, int nRows);
    static ExportResult doDownsample(const QString &src, const QString &dst,
                                     QChar delim, int factor, int startRow);
};

#endif // DOWNSAMPLEDIALOG_H
