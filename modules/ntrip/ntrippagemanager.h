#ifndef NTRIPPAGEMANAGER_H
#define NTRIPPAGEMANAGER_H

#include <QObject>
#include <QFile>
#include <QTimer>

class QPushButton;
class QPlainTextEdit;
class QLabel;
class QCheckBox;
class QFrame;
class QLineEdit;
class QBoxLayout;
class ForwarderTableManager;

// 封装 NTRIP 页面全部逻辑：配置按钮、FixGGA、状态卡片、保存数据
// MainWindow 只需 new 一个实例，传入对应控件即可
class NtripPageManager : public QObject
{
    Q_OBJECT
public:
    struct Widgets {
        QPushButton   *configButton;
        QPlainTextEdit *logEdit;
        QLabel        *connectStatusLabel;
        QLabel        *rateLabel;
        QCheckBox     *fixGgaCheckBox;
        QFrame        *fixGgaFrame;
        QLineEdit     *fixLonEdit;
        QLineEdit     *fixLatEdit;
        QLineEdit     *fixHeightEdit;
        QBoxLayout       *headerLayout;  // horizontalLayout_7，用于插入保存按钮
    };

    NtripPageManager(const Widgets &w, ForwarderTableManager *ftm,
                     QObject *parent = nullptr);
    ~NtripPageManager() override;

private slots:
    void onToggleSaveData();

private:
    void loadFixGgaConfig();
    void setupConnections();
    void createSaveButton();

    Widgets m_w;
    ForwarderTableManager *m_ftm;

    // 保存数据状态
    QPushButton *m_saveDataButton = nullptr;
    QFile        m_saveFile;
    QTimer      *m_saveTimer = nullptr;
    int          m_saveConsumerId = -1;
};

#endif // NTRIPPAGEMANAGER_H
