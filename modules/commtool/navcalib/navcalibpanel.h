#ifndef NAVCALIB_NAVCALIBPANEL_H
#define NAVCALIB_NAVCALIBPANEL_H

#include <QDialog>

#include "leverarmestimator.h"

class QCheckBox;
class QCloseEvent;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class NavCalibPlotWindow;

// ════════════════════════════════════════════════════════════════
//  超达投影估计 —— 独立对话框（非模态，可同时操作主界面的连接）
//
//  从 0x0166 融合帧里取位置 + 航向，车辆原地旋转时拟合旋转中心
//  与杆臂。左侧结果，右侧实时轨迹图，窗口足够宽。
//  计算逐帧进行（10Hz），文字逐帧刷新，轨迹图 5Hz 刷新。
// ════════════════════════════════════════════════════════════════
class NavCalibPanel : public QDialog
{
    Q_OBJECT
public:
    explicit NavCalibPanel(QWidget *parent = nullptr);

    bool isCollecting() const { return m_collecting; }
    bool strictCrc() const;

    // CommTool 收到帧后调用（只处理 0x0166）
    void submitFrame(const ParsedFrame &f);

public slots:
    // 串口/TCP 断开时由 CommTool 调用
    void stopCollecting();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStartStop();
    void onReset();
    void onRefreshPlot();

private:
    void updateText();
    QString qualityText() const;
    void pushToPlot();

    LeverArmEstimator        m_est;
    LeverArmEstimator::Result m_result;
    bool m_collecting = false;
    bool m_plotDirty  = false;

    QPushButton    *m_startBtn    = nullptr;
    QPushButton    *m_resetBtn    = nullptr;
    QCheckBox      *m_crcCheck    = nullptr;
    QLabel         *m_statusLabel = nullptr;
    QPlainTextEdit *m_resultEdit  = nullptr;
    QDoubleSpinBox *m_curLx       = nullptr;
    QDoubleSpinBox *m_curLy       = nullptr;
    QTimer         *m_plotTimer   = nullptr;

    NavCalibPlotWindow *m_plot = nullptr;   // 内嵌轨迹图
};

#endif // NAVCALIB_NAVCALIBPANEL_H
