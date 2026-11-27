#ifndef CALIB612PANEL_H
#define CALIB612PANEL_H

#include "calib612analysis.h"

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

class Calib612Worker;

/**
 * @brief 6+12 位置标定面板（纯 UI，计算全部在后台线程）
 *
 * 交互与 AllanPanel 一致：
 *   选文件 → 预览前几行 → 选 6 个 IMU 列 → 填采样率 → 开始计算
 * 结果表：加计/陀螺的标度误差、零偏、安装角、最大非正交角
 * 下方日志：自动检出的每段起点/均值，以及缺段诊断
 */
class Calib612Panel : public QDialog
{
    Q_OBJECT

public:
    explicit Calib612Panel(QWidget *parent = nullptr, QThread *workerThread = nullptr);
    ~Calib612Panel() override;

private slots:
    void onBrowse();
    void onCompute();
    void onProgress(int percent);
    void onError(const QString &msg);
    void onResultReadyWith(const Calib612Analysis::Result &result);

private:
    void refreshColumns(const QString &path);
    void fillTable(const Calib612Analysis::Result &r);
    void fillLog(const Calib612Analysis::Result &r);

    // 输入
    QLineEdit      *m_fileEdit    = nullptr;
    QPushButton    *m_browseBtn   = nullptr;
    QLabel         *m_previewLbl  = nullptr;
    QComboBox      *m_colCombo[6] = {nullptr};
    QSpinBox       *m_skipSpin    = nullptr;
    QDoubleSpinBox *m_fsSpin      = nullptr;
    QDoubleSpinBox *m_rateSpin    = nullptr;
    QPushButton    *m_computeBtn  = nullptr;

    // 进度 / 结果
    QProgressBar   *m_progress    = nullptr;
    QLabel         *m_statusLbl   = nullptr;
    QTableWidget   *m_table       = nullptr;
    QPlainTextEdit *m_log         = nullptr;

    // 后台
    QThread        *m_thread      = nullptr;
    Calib612Worker *m_worker      = nullptr;
};

#endif // CALIB612PANEL_H
