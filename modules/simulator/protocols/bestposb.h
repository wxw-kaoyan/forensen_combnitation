#ifndef SIM_PROTOCOL_BESTPOSB_H
#define SIM_PROTOCOL_BESTPOSB_H

#include "modules/simulator/simprotocol.h"
#include "modules/simulator/encoders/novatelencoder.h"
#include "modules/simulator/encoders/bytewriter.h"

// NovAtel BESTPOSB —— 最佳位置（message ID = 42）
class BestPosb : public SimProtocol
{
public:
    QByteArray encode(const SimClock &c) override
    {
        QByteArray payload;
        ByteWriter::appendI32(payload, solStatusValue(params.value("solStatus").toString("SOL_COMPUTED")));
        ByteWriter::appendI32(payload, posTypeValue(params.value("posType").toString("NARROW_INT")));
        ByteWriter::appendDouble(payload, params.value("lat").toDouble(39.9042));
        ByteWriter::appendDouble(payload, params.value("lon").toDouble(116.4074));
        ByteWriter::appendDouble(payload, params.value("height").toDouble(50.0));     // 椭球高(m)
        ByteWriter::appendFloat(payload, static_cast<float>(params.value("undulation").toDouble(0.0)));
        ByteWriter::appendI32(payload, params.value("datumId").toInt(61));            // WGS84
        ByteWriter::appendFloat(payload, static_cast<float>(params.value("latStd").toDouble(0.01)));
        ByteWriter::appendFloat(payload, static_cast<float>(params.value("lonStd").toDouble(0.01)));
        ByteWriter::appendFloat(payload, static_cast<float>(params.value("heightStd").toDouble(0.02)));
        payload.append("    ");                                                       // base station id
        ByteWriter::appendFloat(payload, static_cast<float>(params.value("diffAge").toDouble(0.0)));
        ByteWriter::appendFloat(payload, static_cast<float>(params.value("solAge").toDouble(0.0)));
        const int numSats = params.value("numSats").toInt(18);
        ByteWriter::appendU8(payload, static_cast<uint8_t>(numSats));                          // #SVs
        ByteWriter::appendU8(payload, static_cast<uint8_t>(params.value("numSatsInSol").toInt(18))); // #solnSVs
        ByteWriter::appendU8(payload, static_cast<uint8_t>(numSats));                          // #solnL1SVs
        ByteWriter::appendU8(payload, static_cast<uint8_t>(numSats));                          // #solnMultiSVs
        payload.append(1, '\0');                                                      // reserved
        payload.append(1, '\0');                                                      // extended solution status
        payload.append(1, '\0');                                                      // Galileo/BeiDou signal-used mask
        payload.append(1, '\0');                                                      // GPS/GLONASS signal-used mask

        return NovatelEncoder::pack(42, payload, c);
    }

private:
    static int32_t solStatusValue(const QString &s)
    {
        if (s == "INSUFFICIENT_OBS") return 1;
        if (s == "COLD_START")       return 6;
        return 0; // SOL_COMPUTED
    }

    static int32_t posTypeValue(const QString &s)
    {
        if (s == "FIXEDPOS")     return 1;
        if (s == "FIXEDHEIGHT")  return 2;
        if (s == "SINGLE")       return 16;
        if (s == "PSRDIFF")      return 17;
        if (s == "WAAS")         return 18;
        if (s == "L1_FLOAT")     return 32;
        if (s == "NARROW_FLOAT") return 34;
        if (s == "L1_INT")       return 48;
        if (s == "WIDE_INT")     return 49;
        if (s == "NARROW_INT")   return 50;
        if (s == "PPP")          return 69;
        return 0; // NONE
    }
};

#endif // SIM_PROTOCOL_BESTPOSB_H
