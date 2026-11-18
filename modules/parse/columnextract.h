#ifndef COLUMNEXTRACT_H
#define COLUMNEXTRACT_H

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>

// ════════════════════════════════════════════════════════════════
//  New Code 通用工具：从空格/逗号/Tab 分隔的文本文件里选列，
//  多个文件时按时间戳对齐合并（低 Hz 为主轴，最近邻匹配）。
// ════════════════════════════════════════════════════════════════

// 一个文本数据表（首行表头）
struct TextFileTable
{
    QString            path;
    QStringList        header;      // 表头列名
    QList<QStringList> rows;        // 数据行（字符串，与 header 对齐）
    QVector<double>    times;       // 时间列数值（与 rows 一一对应）
    int                timeColumn = 0;
    QChar              delimiter = ',';
    int                decimateStep = 1;   // 加载时实际使用的抽稀步长（1=未抽稀）
    QString            error;       // 非空表示加载失败
};

// 加载文本文件；timeColumn < 0 时自动识别时间列
// maxRows > 0 时按整数步长抽稀，使载入行数不超过 maxRows（用于百万行 CSV 的快速绘图）
TextFileTable loadTextFileTable(const QString &path, int timeColumn = -1,
                                int maxRows = 0);

// 时间序列单调展开。
// 设备里的 Timer 常常是 uint16/uint32 计数器（如微秒计数，2^32 us ≈ 4295 s 回绕一次）。
// 不做展开的话，画图时 X 轴每隔一个回绕周期跳回 0，曲线被拉成来回折返的锯齿。
void unwrapTimeColumn(QVector<double> &times);

class QPushButton;
class QLabel;
class QListWidget;
class QComboBox;
class QProgressBar;
class QDialogButtonBox;
class QFutureWatcherBase;

class ColumnExtractDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ColumnExtractDialog(QWidget *parent = nullptr);
    ~ColumnExtractDialog() override;

private slots:
    void onOkClicked();      // "开始导出"按钮：校验 + 异步导出，不直接关闭
    void onCancelClicked();  // 取消：如果正在导出则等待/忽略，否则关闭

private:
    struct FileUi {
        TextFileTable table;
        QList<bool>   checked;      // 每列是否勾选
        int           timeColumn = 0;
    };
    QList<FileUi> m_files;

    // 导出状态
    QString        m_exportPath;
    int            m_exportMaster = 0;
    bool           m_exporting = false;   // 正在导出（此时不允许关闭）

    // UI 控件
    QListWidget    *m_fileList    = nullptr;
    QListWidget    *m_columnList  = nullptr;
    QComboBox      *m_timeCombo   = nullptr;
    QComboBox      *m_masterCombo = nullptr;
    QPushButton    *m_addBtn      = nullptr;
    QPushButton    *m_removeBtn   = nullptr;
    QPushButton    *m_selAllBtn   = nullptr;
    QPushButton    *m_deselAllBtn = nullptr;
    QPushButton    *m_okBtn       = nullptr;
    QLabel         *m_hintLabel   = nullptr;
    QProgressBar   *m_progressBar = nullptr;
    QLabel         *m_progressLbl = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;

    // 异步加载 / 异步导出（共用一个 watcher 指针，不同时运行）
    bool                 m_loading = false;
    QFutureWatcherBase  *m_watcher = nullptr;

    void addFiles();
    void startAsyncLoad(const QStringList &paths);
    void onLoadFinished();
    void removeCurrentFile();
    void refreshFileList();
    void refreshColumnUI();
    void onCurrentFileChanged(int row);
    void onColumnItemChanged();
    void selectAllColumns(bool checked);
    int  currentFileIndex() const;
    int  autoMasterIndex() const;   // 行数最少（低 Hz）的文件

    void setBusy(bool busy, const QString &msg = QString());
    void startExport(const QString &path, int masterIdx);
    void onExportFinished();

    void closeEvent(class QCloseEvent *e) override;

    void scheduleDataCleanup();   // 将 m_files 大数据移到后台线程释放，避免 UI 卡顿

    static QString buildMergedCsv(const QList<FileUi> &files, int masterIndex);
    static bool    writeMergedCsv(const QList<FileUi> &files, int masterIndex,
                                  const QString &outPath, QString *errorOut, qint64 *sizeOut);
};

#endif // COLUMNEXTRACT_H
