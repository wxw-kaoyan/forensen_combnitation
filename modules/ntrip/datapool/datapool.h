#ifndef DATAPOOL_H
#define DATAPOOL_H

#include <QObject>
#include <QByteArray>
#include <QReadWriteLock>
#include <QVector>

// 线程安全的数据池 —— 生产者-消费者模型
// - NTRIP 连接作为生产者持续 feed() 数据
// - 每个转发目标作为一个消费者，独立 read() 不互相阻塞
class DataPool : public QObject
{
    Q_OBJECT

public:
    explicit DataPool(int capacityBytes = 2 * 1024 * 1024, QObject *parent = nullptr);
    ~DataPool() override = default;

    // 生产者：写入数据（任意线程安全）
    void feed(const QByteArray &data);

    // 消费者：注册 → 获取自上次读取后的新数据
    int subscribe();
    QByteArray read(int consumerId);
    int availableBytes(int consumerId) const;

signals:
    void dataAvailable();  // 通知所有消费者有新数据可读

private:
    mutable QReadWriteLock m_lock;
    QByteArray m_buffer;
    int m_capacity;
    qint64 m_writePos = 0;
    QVector<qint64> m_readPositions;
};

#endif // DATAPOOL_H
