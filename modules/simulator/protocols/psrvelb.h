#ifndef SIM_PROTOCOL_PSRVELB_H
#define SIM_PROTOCOL_PSRVELB_H

#include "modules/simulator/simprotocol.h"
#include "modules/simulator/encoders/novatelencoder.h"
#include "modules/simulator/encoders/bytewriter.h"

// NovAtel PSRVELB —— 伪距/载波速度（message ID = 100）
class Psrvelb : public SimProtocol
{
public:
    QByteArray encode(const SimClock &c) override
    {
        QByteArray payload;
        ByteWriter::appendI32(payload, solStatusValue(params.value("solStatus").toString("SOL_COMPUTED")));
        ByteWriter::appendI32(payload, velTypeValue(params.value("velType").toString("DOPPLER_VELOCITY")));
        ByteWriter::appendFloat(payload, static_cast<float>(params.value("latency").toDouble(0.0)));
        ByteWriter::appendFloat(payload, static_cast<float>(params.value("diffAge").toDouble(0.0)));
        ByteWriter::appendDouble(payload, params.value("horSpeed").toDouble(0.5));   // 水平速度(m/s)
        ByteWriter::appendDouble(payload, params.value("trackGround").toDouble(90.0)); // 地面航向(度)
        ByteWriter::appendDouble(payload, params.value("verSpeed").toDouble(0.0));   // 垂直速度(m/s)
        ByteWriter::appendFloat(payload, 0.0f);                                      // reserved (float)

        return NovatelEncoder::pack(100, payload, c);
    }

private:
    static int32_t solStatusValue(const QString &s)
    {
        if (s == "INSUFFICIENT_OBS") return 1;
        if (s == "COLD_START")       return 6;
        return 0; // SOL_COMPUTED
    }

    static int32_t velTypeValue(const QString &s)
    {
        if (s == "DOPPLER_VELOCITY") return 8;
        return 0; // NO_VELOCITY
    }
};

#endif // SIM_PROTOCOL_PSRVELB_H
