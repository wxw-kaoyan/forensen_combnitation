// ============================================================================
//  nav/common/rotation.hpp
//  旋转表示转换：四元数 / DCM / 欧拉角 / 旋转矢量
//
//  来源：KF-GINS (i2Nav Group, Wuhan University), GPL-3.0
//  重构：去掉 <iostream> 与奇异角时的 std::cout 打印（改为静默处理），
//        补 <cmath>，加 namespace
// ============================================================================
#ifndef NAV_COMMON_ROTATION_HPP
#define NAV_COMMON_ROTATION_HPP

#include <Eigen/Geometry>
#include <cmath>

#include "angle.hpp"

namespace nav {

using Eigen::Matrix3d;
using Eigen::Quaterniond;
using Eigen::Vector3d;

class Rotation {
public:
    static Quaterniond matrix2quaternion(const Matrix3d &matrix) {
        return Quaterniond(matrix);
    }

    static Matrix3d quaternion2matrix(const Quaterniond &quaternion) {
        return quaternion.toRotationMatrix();
    }

    // ZYX 旋转顺序，前右下 IMU，输出 [roll, pitch, yaw]
    // 万向锁时 roll 置 0（原实现会打印警告，此处静默）
    static Vector3d matrix2euler(const Matrix3d &dcm) {
        Vector3d euler;

        const double sp = dcm(2, 0);
        euler[1] = std::atan(-sp / std::sqrt(dcm(2, 1) * dcm(2, 1) + dcm(2, 2) * dcm(2, 2)));

        if (sp <= -0.999) {
            euler[0] = 0.0;
            euler[2] = std::atan2((dcm(1, 2) - dcm(0, 1)), (dcm(0, 2) + dcm(1, 1)));
        } else if (sp >= 0.999) {
            euler[0] = 0.0;
            euler[2] = PI + std::atan2((dcm(1, 2) + dcm(0, 1)), (dcm(0, 2) - dcm(1, 1)));
        } else {
            euler[0] = std::atan2(dcm(2, 1), dcm(2, 2));
            euler[2] = std::atan2(dcm(1, 0), dcm(0, 0));
        }

        // heading 归一化到 [0, 2PI)
        if (euler[2] < 0.0) {
            euler[2] = 2.0 * PI + euler[2];
        }

        return euler;
    }

    static Vector3d quaternion2euler(const Quaterniond &quaternion) {
        return matrix2euler(quaternion.toRotationMatrix());
    }

    static Quaterniond rotvec2quaternion(const Vector3d &rotvec) {
        const double angle = rotvec.norm();
        if (angle < 1e-12) {
            // 零旋转：直接返回单位四元数，避免 normalized() 出现 NaN
            return Quaterniond::Identity();
        }
        return Quaterniond(Eigen::AngleAxisd(angle, rotvec / angle));
    }

    static Vector3d quaternion2vector(const Quaterniond &quaternion) {
        const Eigen::AngleAxisd axisd(quaternion);
        return axisd.angle() * axisd.axis();
    }

    // RPY -> C_b^n，ZYX 顺序
    static Matrix3d euler2matrix(const Vector3d &euler) {
        return Matrix3d(Eigen::AngleAxisd(euler[2], Vector3d::UnitZ()) *
                        Eigen::AngleAxisd(euler[1], Vector3d::UnitY()) *
                        Eigen::AngleAxisd(euler[0], Vector3d::UnitX()));
    }

    static Quaterniond euler2quaternion(const Vector3d &euler) {
        return Quaterniond(Eigen::AngleAxisd(euler[2], Vector3d::UnitZ()) *
                           Eigen::AngleAxisd(euler[1], Vector3d::UnitY()) *
                           Eigen::AngleAxisd(euler[0], Vector3d::UnitX()));
    }

    static Matrix3d skewSymmetric(const Vector3d &v) {
        Matrix3d mat;
        mat <<  0.0,  -v(2),  v(1),
               v(2),   0.0, -v(0),
              -v(1),  v(0),   0.0;
        return mat;
    }

    static Eigen::Matrix4d quaternionleft(const Quaterniond &q) {
        Eigen::Matrix4d ans;
        ans(0, 0)             = q.w();
        ans.block<1, 3>(0, 1) = -q.vec().transpose();
        ans.block<3, 1>(1, 0) = q.vec();
        ans.block<3, 3>(1, 1) = q.w() * Matrix3d::Identity() + skewSymmetric(q.vec());
        return ans;
    }

    static Eigen::Matrix4d quaternionright(const Quaterniond &p) {
        Eigen::Matrix4d ans;
        ans(0, 0)             = p.w();
        ans.block<1, 3>(0, 1) = -p.vec().transpose();
        ans.block<3, 1>(1, 0) = p.vec();
        ans.block<3, 3>(1, 1) = p.w() * Matrix3d::Identity() - skewSymmetric(p.vec());
        return ans;
    }
};

} // namespace nav

#endif // NAV_COMMON_ROTATION_HPP
