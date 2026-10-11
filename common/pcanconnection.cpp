#include "pcanconnection.h"

#include <QByteArray>
#include <QLibrary>
#include <QTimer>
#include <cstring>

// ════════════════════════════════════════════════════════════════
//  PCAN-Basic 4.x 常量与结构（依 PEAK 官方 PCANBasic.h，节选所需部分）
//  导出函数通过 QLibrary 运行时解析，避免依赖 MSVC 格式的导入库
// ════════════════════════════════════════════════════════════════
namespace {

// 消息类型标志（TPCANMsg.MSGTYPE）
constexpr quint8 PCAN_MESSAGE_RTR      = 0x01;
constexpr quint8 PCAN_MESSAGE_EXTENDED = 0x02;

// 状态码（TPCANStatus，DWORD）
constexpr quint32 PCAN_ERROR_OK          = 0x00000;
constexpr quint32 PCAN_ERROR_XMTFULL     = 0x00001;
constexpr quint32 PCAN_ERROR_OVERRUN     = 0x00002;
constexpr quint32 PCAN_ERROR_BUSLIGHT    = 0x00004;
constexpr quint32 PCAN_ERROR_BUSHEAVY    = 0x00008;
constexpr quint32 PCAN_ERROR_BUSWARNING  = 0x00010;
constexpr quint32 PCAN_ERROR_QRCVEMPTY   = 0x00020;
constexpr quint32 PCAN_ERROR_QOVERRUN    = 0x00040;
constexpr quint32 PCAN_ERROR_NODRIVER    = 0x00200;
constexpr quint32 PCAN_ERROR_HWINUSE     = 0x00400;
constexpr quint32 PCAN_ERROR_ILLHANDLE   = 0x02000;
constexpr quint32 PCAN_ERROR_ILLPARAMVAL = 0x04000;
constexpr quint32 PCAN_ERROR_NOVENDOR    = 0x08000;
constexpr quint32 PCAN_ERROR_INITIALIZE  = 0x100000;

constexpr quint32 kBusErrMask = PCAN_ERROR_BUSLIGHT | PCAN_ERROR_BUSHEAVY
                              | PCAN_ERROR_BUSWARNING;

// 波特率代码（BTR0BTR1）
struct BaudEntry { quint16 code; const char *name; };
constexpr BaudEntry kBaudTable[] = {
    {0x0014, "1 Mbit/s"},
    {0x0016, "800 kbit/s"},
    {0x001C, "500 kbit/s"},
    {0x011C, "250 kbit/s"},
    {0x031C, "125 kbit/s"},
    {0x432F, "100 kbit/s"},
    {0x472F, "50 kbit/s"},
    {0x532F, "20 kbit/s"},
    {0x672F, "10 kbit/s"},
    {0x7F7F, "5 kbit/s"},
};

// 消息结构（自然对齐，与官方头文件布局一致）
struct PcanMsg {
    quint32 id;
    quint8  msgType;
    quint8  len;
    quint8  data[8];
};
// 时间戳结构（毫秒 + 溢出计数 + 微秒部分）
struct PcanTimestamp {
    quint32 millis;
    quint16 millisOverflow;
    quint16 micros;
};
static_assert(sizeof(PcanMsg) == 16, "TPCANMsg 布局与官方头文件不一致");
static_assert(sizeof(PcanTimestamp) == 8, "TPCANTimestamp 布局与官方头文件不一致");

// 导出函数指针（x64 下 __stdcall 等价默认约定；x86 下 PCANBasic.dll 为 stdcall）
using CanInitializeFn   = quint32 (__stdcall *)(quint8 channel, quint16 btr0btr1,
                                                quint8 hwType, quint32 ioPort,
                                                quint16 interrupt);
using CanUninitializeFn = quint32 (__stdcall *)(quint8 channel);
using CanReadFn         = quint32 (__stdcall *)(quint8 channel, PcanMsg *msg,
                                                PcanTimestamp *ts);
using CanWriteFn        = quint32 (__stdcall *)(quint8 channel, const PcanMsg *msg);
using CanGetStatusFn    = quint32 (__stdcall *)(quint8 channel);
using CanGetErrorTextFn = quint32 (__stdcall *)(quint32 status, quint16 lang,
                                                char *buffer);

// DLL 导出为进程全局：解析结果进程级共享一份
struct Api
{
    bool              ok        = false;
    CanInitializeFn   initialize   = nullptr;
    CanUninitializeFn uninitialize = nullptr;
    CanReadFn         read         = nullptr;
    CanWriteFn        write        = nullptr;
    CanGetStatusFn    getStatus    = nullptr;
    CanGetErrorTextFn errorText    = nullptr;
};

Api g_api;

QString baudName(quint16 code)
{
    for (const BaudEntry &b : kBaudTable) {
        if (b.code == code)
            return QString::fromLatin1(b.name);
    }
    return QStringLiteral("0x%1").arg(code, 0, 16);
}

QString channelHexName(quint8 handle)
{
    return QStringLiteral("0x%1").arg(handle, 2, 16, QChar('0')).toUpper();
}

} // namespace

// ════════════════════════════════════════════════════════════════
//  静态信息接口
// ════════════════════════════════════════════════════════════════
bool PcanConnection::loadApi()
{
    if (g_api.ok)
        return true;

    QLibrary lib(QStringLiteral("PCANBasic"));
    if (!lib.load())
        return false;

    g_api.initialize   = reinterpret_cast<CanInitializeFn>(lib.resolve("CAN_Initialize"));
    g_api.uninitialize = reinterpret_cast<CanUninitializeFn>(lib.resolve("CAN_Uninitialize"));
    g_api.read         = reinterpret_cast<CanReadFn>(lib.resolve("CAN_Read"));
    g_api.write        = reinterpret_cast<CanWriteFn>(lib.resolve("CAN_Write"));
    g_api.getStatus    = reinterpret_cast<CanGetStatusFn>(lib.resolve("CAN_GetStatus"));
    g_api.errorText    = reinterpret_cast<CanGetErrorTextFn>(lib.resolve("CAN_GetErrorText"));
    g_api.ok = g_api.initialize && g_api.uninitialize && g_api.read && g_api.write;
    return g_api.ok;
}

bool PcanConnection::isDriverAvailable()
{
    return loadApi();
}

QList<PcanConnection::ChannelInfo> PcanConnection::availableChannels()
{
    // 即插即用通道；ISA 为非即插即用（需 IO 口与中断参数），不提供
    QList<ChannelInfo> list;
    const struct { quint8 base; int count; const char *family; } families[] = {
        {0x51, 16, "USB"},
        {0x41, 16, "PCI"},
        {0xC1, 16, "PCC"},
        {0xE1, 16, "LAN"},
    };
    for (const auto &f : families) {
        for (int i = 0; i < f.count; ++i) {
            const quint8 handle = quint8(f.base + i);
            list.append({ handle,
                          QStringLiteral("%1%2 (%3)")
                              .arg(QString::fromLatin1(f.family)).arg(i + 1)
                              .arg(channelHexName(handle)) });
        }
    }
    return list;
}

QList<PcanConnection::BaudInfo> PcanConnection::availableBaudrates()
{
    QList<BaudInfo> list;
    for (const BaudEntry &b : kBaudTable)
        list.append({ b.code, QString::fromLatin1(b.name) });
    return list;
}

QString PcanConnection::statusText(quint32 status)
{
    switch (status) {
        case PCAN_ERROR_OK:          return QStringLiteral("成功");
        case PCAN_ERROR_XMTFULL:     return QStringLiteral("发送缓冲已满");
        case PCAN_ERROR_OVERRUN:     return QStringLiteral("接收缓冲溢出");
        case PCAN_ERROR_BUSLIGHT:    return QStringLiteral("总线错误（错误计数达被动上限）");
        case PCAN_ERROR_BUSHEAVY:    return QStringLiteral("总线错误（总线关闭倾向）");
        case PCAN_ERROR_BUSWARNING:  return QStringLiteral("总线警告（错误计数超警戒值）");
        case PCAN_ERROR_QRCVEMPTY:   return QStringLiteral("接收队列为空");
        case PCAN_ERROR_QOVERRUN:    return QStringLiteral("接收队列溢出");
        case PCAN_ERROR_NODRIVER:    return QStringLiteral("未安装 PCAN 驱动");
        case PCAN_ERROR_HWINUSE:     return QStringLiteral("通道已被其他程序占用");
        case PCAN_ERROR_ILLHANDLE:   return QStringLiteral("非法通道句柄");
        case PCAN_ERROR_ILLPARAMVAL: return QStringLiteral("非法参数值");
        case PCAN_ERROR_NOVENDOR:    return QStringLiteral("未检测到 PCAN 硬件");
        case PCAN_ERROR_INITIALIZE:  return QStringLiteral("通道已初始化");
        default: break;
    }
    // 未知/组合码 → 交给 DLL 自带的 CAN_GetErrorText（0x09 = 英语）
    char buf[256] = {};
    if (g_api.ok && g_api.errorText &&
        g_api.errorText(status, 0x09, buf) == PCAN_ERROR_OK && buf[0]) {
        return QStringLiteral("%1 (0x%2)").arg(QString::fromLatin1(buf))
                                          .arg(status, 0, 16);
    }
    return QStringLiteral("未知错误 (0x%1)").arg(status, 0, 16);
}

// ════════════════════════════════════════════════════════════════
//  实例生命周期
// ════════════════════════════════════════════════════════════════
PcanConnection::PcanConnection(QObject *parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(10);   // 接收轮询周期
    connect(m_pollTimer, &QTimer::timeout, this, &PcanConnection::onPollTimer);
}

PcanConnection::~PcanConnection()
{
    cleanup();
}

void PcanConnection::cleanup()
{
    if (m_pollTimer)
        m_pollTimer->stop();
    if (m_connected && g_api.ok)
        g_api.uninitialize(m_channel);
    m_connected = false;
}

// ════════════════════════════════════════════════════════════════
//  连接 / 断开 / 收发
// ════════════════════════════════════════════════════════════════
void PcanConnection::connectToChannel(int channelHandle, int baudCode)
{
    if (m_connected) {
        emit logMessage(QStringLiteral("PCAN 已连接（通道 %1），请先断开")
                            .arg(channelHexName(m_channel)));
        return;
    }
    if (!loadApi()) {
        emit errorOccurred(QStringLiteral("无法加载 PCANBasic.dll，请确认已安装 PEAK PCAN 驱动"));
        return;
    }

    m_channel = quint8(channelHandle);
    // 后三个参数仅供非即插即用硬件（ISA）使用，即插即用通道传 0
    const quint32 st = g_api.initialize(m_channel, quint16(baudCode), 0, 0, 0);
    if (st != PCAN_ERROR_OK) {
        emit errorOccurred(QStringLiteral("PCAN 初始化失败（通道 %1）：%2")
                               .arg(channelHexName(m_channel)).arg(statusText(st)));
        return;
    }

    m_connected    = true;
    m_lastBusState = 0;
    m_pollTimer->start();
    emit logMessage(QStringLiteral("PCAN 已连接（通道 %1，波特率 %2）")
                        .arg(channelHexName(m_channel)).arg(baudName(quint16(baudCode))));
    emit connected();
}

void PcanConnection::disconnectFromChannel()
{
    if (!m_connected)
        return;
    m_pollTimer->stop();
    m_connected = false;
    if (g_api.ok)
        g_api.uninitialize(m_channel);
    emit logMessage(QStringLiteral("PCAN 已断开（通道 %1）").arg(channelHexName(m_channel)));
    emit disconnected();
}

void PcanConnection::send(quint32 id, const QByteArray &data, bool extended)
{
    if (!m_connected) {
        emit logMessage(QStringLiteral("PCAN 未连接，发送被忽略"));
        return;
    }
    if (data.size() > 8) {
        emit errorOccurred(QStringLiteral("CAN 数据长度超限（≤8 字节，当前 %1）").arg(data.size()));
        return;
    }

    PcanMsg msg;
    msg.id      = id;
    msg.msgType = extended ? PCAN_MESSAGE_EXTENDED : quint8(0);
    msg.len     = quint8(data.size());
    if (!data.isEmpty())
        std::memcpy(msg.data, data.constData(), size_t(data.size()));

    const quint32 st = g_api.write(m_channel, &msg);
    if (st != PCAN_ERROR_OK)
        emit errorOccurred(QStringLiteral("PCAN 发送失败：%1").arg(statusText(st)));
}

void PcanConnection::onPollTimer()
{
    PcanMsg       msg;
    PcanTimestamp ts;
    // 单次最多排空 128 帧，防止高总线负载占死工作线程
    for (int i = 0; i < 128; ++i) {
        const quint32 st = g_api.read(m_channel, &msg, &ts);
        if (st == PCAN_ERROR_OK) {
            // 毫秒计数器回绕次数 × 2^32 还原绝对毫秒，再并入微秒部分
            const quint64 totalMs = quint64(ts.millis)
                                  + (quint64(1) << 32) * ts.millisOverflow;
            emit frameReceived(msg.id,
                               QByteArray(reinterpret_cast<const char *>(msg.data),
                                          msg.len),
                               msg.msgType,
                               totalMs * 1000 + ts.micros);
            continue;
        }
        if (st == PCAN_ERROR_QRCVEMPTY)
            break;

        if (st & kBusErrMask) {
            // 总线状态类错误只在状态变化时提示一次
            const int newState = (st & PCAN_ERROR_BUSHEAVY)   ? 3
                               : (st & PCAN_ERROR_BUSLIGHT)   ? 2
                               : 1;
            if (newState != m_lastBusState) {
                m_lastBusState = newState;
                emit logMessage(QStringLiteral("PCAN 总线状态：%1").arg(statusText(st)));
            }
            break;
        }
        if (st & (PCAN_ERROR_OVERRUN | PCAN_ERROR_QOVERRUN)) {
            emit logMessage(QStringLiteral("PCAN 接收溢出：%1").arg(statusText(st)));
            break;
        }
        // 其余（未连接/非法句柄等）视为致命：上报并断开
        emit errorOccurred(QStringLiteral("PCAN 读取失败：%1").arg(statusText(st)));
        disconnectFromChannel();
        break;
    }
}
