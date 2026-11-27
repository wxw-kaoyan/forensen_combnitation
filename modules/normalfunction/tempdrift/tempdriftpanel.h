#ifndef TEMPDRIFTPANEL_H
#define TEMPDRIFTPANEL_H

#include "tempdriftanalysis.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QThread;

class TempDriftWorker;

/**
 * @brief 拉温（温漂）分析面板 —— 与 Allan / 6+12 同构的离线工具
 *
 * 选文件 → 选 6 轴 + 温度列 → 填采样率/保温阈值 → 后台计算
 * 结果：每轴峰峰值、标准差（全段 / 保温段）、温漂斜率
 * 曲线：计算完成后弹一个 2×3 窗口，画 6 轴的「值-温度」特性
 */
class TempDriftPanel : public QDialog
{
    Q_OBJECT

public:
    explicit TempDriftPanel(QWidget *parent = nullptr, QThread *workerThread = nullptr);
    ~TempDriftPanel() override;

private slots:
    void onBrowse();
    void onCompute();
    void onProgress(int percent);
    void onError(const QString &msg);
    void onResultReadyWith(const TempDriftAnalysis::Result &result);

private:
    void refreshColumns(const QString &path);
    void fillTable(const TempDriftAnalysis::Result &r);
    void fillLog(const TempDriftAnalysis::Result &r);
    void showChartWindow();

    QLineEdit      *m_fileEdit   = nullptr;
    QPushButton    *m_browseBtn  = nullptr;
    QLabel         *m_previewLbl = nullptr;
    QComboBox      *m_colCombo[7] = {nullptr};   // ax ay az gx gy gz temp
    QSpinBox       *m_skipSpin   = nullptr;
    QDoubleSpinBox *m_fsSpin     = nullptr;
    QDoubleSpinBox *m_rampSpin   = nullptr;
    QPushButton    *m_computeBtn = nullptr;

    QProgressBar   *m_progress   = nullptr;
    QLabel         *m_statusLbl  = nullptr;
    QTableWidget   *m_table      = nullptr;
    QPlainTextEdit *m_log        = nullptr;

    QThread         *m_thread    = nullptr;
    TempDriftWorker *m_worker    = nullptr;

    TempDriftAnalysis::Result m_last;
    bool m_hasResult = false;
};

#endif // TEMPDRIFTPANEL_H
