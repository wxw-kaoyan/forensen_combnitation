#ifndef ANALYSIS_RAW_RAWANALYSISPANEL_H
#define ANALYSIS_RAW_RAWANALYSISPANEL_H

#include <QWidget>

class QLineEdit;
class QPushButton;
class QTableWidget;
class QLabel;

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  原始数据分析面板
//  读取 RINEX 观测文件（.O / .obs），做数据质量检核：
//  · 观测值统计（历元数、记录数、各系统卫星数）
//  · 丢数检测（丢历元、掉星）
//  · SNR 信噪比统计（按系统 / 频点）
// ════════════════════════════════════════════════════════════════
class RawAnalysisPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RawAnalysisPanel(QWidget *parent = nullptr);

private slots:
    void onBrowse();
    void onAnalyze();

private:
    QLineEdit   *m_pathEdit   = nullptr;
    QPushButton *m_browseBtn  = nullptr;
    QPushButton *m_analyzeBtn = nullptr;
    QLabel      *m_summaryLabel = nullptr;
    QTableWidget *m_snrTable  = nullptr;

    void buildUi();
    void showSummary(const struct RinexSummary &s);
};

} // namespace Analysis

#endif // ANALYSIS_RAW_RAWANALYSISPANEL_H
