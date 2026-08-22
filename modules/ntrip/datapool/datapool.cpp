#include "datapool.h"
#include <algorithm>

DataPool::DataPool(int capacityBytes, QObject *parent)
    : QObject(parent)
    , m_capacity(capacityBytes)
{
    m_buffer.resize(m_capacity);
}

void DataPool::feed(const QByteArray &data)
{
    if (data.isEmpty()) return;

    QWriteLocker locker(&m_lock);

    int len = data.size();
    qint64 start = m_writePos % m_capacity;

    if (start + len <= m_capacity) {
        // 不跨边界，直接拷贝
        std::memcpy(m_buffer.data() + start, data.constData(), len);
    } else {
        // 跨边界，分两段拷贝
        int firstPart = m_capacity - static_cast<int>(start);
        std::memcpy(m_buffer.data() + start, data.constData(), firstPart);
        std::memcpy(m_buffer.data(), data.constData() + firstPart, len - firstPart);
    }

    m_writePos += len;
    locker.unlock();

    emit dataAvailable();
}

int DataPool::subscribe()
{
    QWriteLocker locker(&m_lock);
    int id = m_readPositions.size();
    m_readPositions.append(m_writePos); // 新消费者从当前位置开始，不回溯历史
    return id;
}

QByteArray DataPool::read(int consumerId)
{
    if (consumerId < 0 || consumerId >= m_readPositions.size())
        return {};

    QReadLocker locker(&m_lock);

    qint64 readPos  = m_readPositions[consumerId];
    qint64 writePos = m_writePos;

    if (readPos >= writePos)
        return {}; // 无新数据

    // 如果消费者落后太多（超过 buffer 容量），跳到有效起点
    if (writePos - readPos > m_capacity) {
        readPos = writePos - m_capacity;
        m_readPositions[consumerId] = readPos;
    }

    qint64 len = writePos - readPos;
    qint64 start = readPos % m_capacity;
    QByteArray result;
    result.resize(static_cast<int>(len));

    if (start + len <= m_capacity) {
        std::memcpy(result.data(), m_buffer.constData() + start, len);
    } else {
        int firstPart = m_capacity - static_cast<int>(start);
        std::memcpy(result.data(), m_buffer.constData() + start, firstPart);
        std::memcpy(result.data() + firstPart, m_buffer.constData(), len - firstPart);
    }

    m_readPositions[consumerId] = writePos;
    return result;
}

int DataPool::availableBytes(int consumerId) const
{
    if (consumerId < 0 || consumerId >= m_readPositions.size())
        return 0;

    QReadLocker locker(&m_lock);
    qint64 diff = m_writePos - m_readPositions[consumerId];
    return static_cast<int>(std::min(diff, static_cast<qint64>(m_capacity)));
}
