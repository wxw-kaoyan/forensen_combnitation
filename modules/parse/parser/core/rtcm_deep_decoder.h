#ifndef RTCM_DEEP_DECODER_H
#define RTCM_DEEP_DECODER_H

#include "modules/parse/parser/core/rtklib/rtklib.h"
#include <QByteArray>
#include <QString>
#include <QList>

// ════════════════════════════════════════════════════════════════
//  单历元观测值（RINEX 格式的单行观测）
// ════════════════════════════════════════════════════════════════
struct RtcmObsEpoch {
    gtime_t time;                   // 历元 GPST
    QList<obsd_t> observations;     // 该历元所有卫星的观测值
};

// ════════════════════════════════════════════════════════════════
//  RTCM 3.x 深解码器 — 使用 RTKLIB 的 input_rtcm3() 将原始 RTCM 3.x 字节流
//  解码为 RINEX 观测值格式
// ════════════════════════════════════════════════════════════════
class RtcmDeepDecoder
{
public:
    RtcmDeepDecoder();
    ~RtcmDeepDecoder();

    // 解码整个字节流，返回所有历元的观测值
    QList<RtcmObsEpoch> decode(const QByteArray &rawData);

    // 格式化为 RINEX 观测值文本
    static QString toRinexText(const QList<RtcmObsEpoch> &epochs);

    // 格式化为简化 CSV 文本（对于文本显示更友好）
    static QString toCsvText(const QList<RtcmObsEpoch> &epochs);

    // 将卫星编号转为 RINEX ID 字符串 (G01, C01, R01...)
    static QString satIdStr(int sat);

    // 将观测码转为 RINEX 字符串
    static QString obsCodeStr(unsigned char code);

private:
    void reset();

    rtcm_t m_rtcm;
    QList<RtcmObsEpoch> m_epochs;
    bool m_initialized = false;
};

#endif // RTCM_DEEP_DECODER_H
