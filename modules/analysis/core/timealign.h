#ifndef ANALYSIS_TIMEALIGN_H
#define ANALYSIS_TIMEALIGN_H

#include "datamodel.h"
#include <cmath>
#include <algorithm>

namespace Analysis {

// ════════════════════════════════════════════════════════════════
//  时间对齐 —— 对标 MATLAB data_align_2
//  两路数据按时间戳最近邻配对
//  返回: 对齐后的两个数据子集 (按公共时间戳)
// ════════════════════════════════════════════════════════════════
struct AlignedPair {
    QVector<DataPoint> ref;      // 参考数据（对齐后）
    QVector<DataPoint> test;     // 待测数据（对齐后）
    int matchedCount = 0;
    int skippedRef   = 0;
    int skippedTest  = 0;
};

inline AlignedPair timeAlignNearest(const QVector<DataPoint> &ref,
                                     const QVector<DataPoint> &test,
                                     double toleranceMs = 1000.0)
{
    AlignedPair result;
    if (ref.isEmpty() || test.isEmpty()) return result;

    int ri = 0, ti = 0;
    while (ri < ref.size() && ti < test.size()) {
        double tr = ref[ri].gpstMs;
        double tt = test[ti].gpstMs;

        if (std::abs(tr - tt) <= toleranceMs) {
            result.ref.append(ref[ri]);
            result.test.append(test[ti]);
            result.matchedCount++;
            ri++; ti++;
        } else if (tr < tt) {
            ri++;
            result.skippedRef++;
        } else {
            ti++;
            result.skippedTest++;
        }
    }
    return result;
}

// ════════════════════════════════════════════════════════════════
//  为两路数据计算时间戳交集（精准匹配）
//  对标 MATLAB data_align_2 用 intersect 的逻辑
// ════════════════════════════════════════════════════════════════
inline AlignedPair timeAlignExact(const QVector<DataPoint> &ref,
                                   const QVector<DataPoint> &test,
                                   double precisionMs = 1.0)
{
    AlignedPair result;

    int ri = 0, ti = 0;
    while (ri < ref.size() && ti < test.size()) {
        double tr = ref[ri].gpstMs;
        double tt = test[ti].gpstMs;

        if (std::abs(tr - tt) <= precisionMs) {
            result.ref.append(ref[ri]);
            result.test.append(test[ti]);
            result.matchedCount++;
            ri++; ti++;
        } else if (tr < tt) {
            ri++;
            result.skippedRef++;
        } else {
            ti++;
            result.skippedTest++;
        }
    }
    return result;
}

} // namespace Analysis

#endif // ANALYSIS_TIMEALIGN_H
