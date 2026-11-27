#ifndef ALLANPANEL_H
#define ALLANPANEL_H

#include "allananalysis.h"   // slot 签名里用了 AllanAnalysis::MultiResult，必须完整定义

#include <QDialog>
#include <QString>
#include <QVector>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QLabel;
class QTableWidget;
class QTableWidgetItem;
class QProgressBar;
class QThread;

class AllanWorker;

/**
 * @brief Allan 方差分析面板（纯 UI，不含任何计算）
 *
 * 所有重活都交给 AllanWorker 在后台线程跑，本面板只做：
 *   - 文件选择 + 前几行预览（让用户自己选哪列、跳几行表头）
 *   - 显示 Worker emit 回来的结果（参数表 + log-log 曲线）
 *
 * 线程：本面板在构造时把 Worker 挂到外部传入的 QThread 上，
 *      计算期间 UI 不卡、可以继续操作其它窗口。
 */
class AllanPanel : public QDialog
{
    Q_OBJECT

public:
    explicit AllanPanel(QWidget *parent = nullptr, QThread *workerThread = nullptr);
    ~AllanPanel() override;

private slots:
    void onBrowse();
    void onCompute();

    // AllanWorker 信号（均已跨线程排队，安全）
    void onProgress(int percent);
    void onError(const QString &msg);
    void onResultReady();  // 先用重载：信号是带参的
    void onResultReadyWith(const AllanAnalysis::MultiResult &mr,
                           const QVector<AllanAnalysis::AccelParams> &acc,
                           const QVector<AllanAnalysis::GyroParams> &gyr);

private:
    void refreshColumnCombos(const QString &path);
    void showChartWindow();     // 计算完成后弹独立窗口画 Allan 曲线

    // UI 控件
    QLineEdit      *m_fileEdit   = nullptr;
    QPushButton    *m_browseBtn = nullptr;
    QLabel         *m_previewLbl= nullptr;   // 前几行预览
    QSpinBox       *m_skipSpin   = nullptr;
    QDoubleSpinBox *m_fsSpin     = nullptr;
    QPushButton    *m_computeBtn= nullptr;
    QProgressBar   *m_progress   = nullptr;
    QLabel         *m_statusLbl = nullptr;

    // 6 个列选择 ComboBox（顺序：ax ay az gx gy gz）
    QComboBox *m_colCombo[6] = {nullptr};

    QTableWidget *m_table = nullptr;

    // 后台计算
    QThread      *m_thread = nullptr;
    AllanWorker  *m_worker = nullptr;

    // 缓存最后一次结果，showChartWindow 要用
    AllanAnalysis::MultiResult m_lastResult;
    QVector<AllanAnalysis::AccelParams> m_lastAccel;
    QVector<AllanAnalysis::GyroParams>   m_lastGyro;

    bool m_hasResult = false;
};

#endif // ALLANPANEL_H
