#ifndef FORWARDERTABLEMANAGER_H
#define FORWARDERTABLEMANAGER_H

#include <QObject>
#include <QMap>

class QTableWidget;
class QAbstractButton;
class DataViewerDialog;

// 管理 NTRIP 转发表：构建、行状态更新、数据查看器、新增目标对话框
class ForwarderTableManager : public QObject
{
    Q_OBJECT
public:
    ForwarderTableManager(QTableWidget *table,
                          QAbstractButton *addTcpBtn,
                          QAbstractButton *addSerialBtn,
                          QObject *parent = nullptr);

    // 清除所有 GGA 源勾选（供 FixGGA 勾选时调用）
    void clearAllGgaSources();

signals:
    // 表中 GGA 源被勾选 → 通知外部取消 FixGGA
    void tableGgaSourceChecked();

private slots:
    void onAddTcpTarget();
    void onAddSerialTarget();

private:
    void connectSignals();
    void rebuildTable();
    void updateRowStatus(int index, const QString &status);
    void openDataViewer(int index);
    void closeDataViewerForIndex(int index);

    QTableWidget    *m_table;
    QAbstractButton *m_addTcpBtn;
    QAbstractButton *m_addSerialBtn;
    QMap<int, DataViewerDialog *> m_dataViewers;
};

#endif // FORWARDERTABLEMANAGER_H
