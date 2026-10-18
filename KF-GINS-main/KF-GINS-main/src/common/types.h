/*
 * OB_GINS: An Optimization-Based GNSS/INS Integrated Navigation System
 *
 * Copyright (C) 2022 i2Nav Group, Wuhan University
 *
 *     Author : Hailiang Tang
 *    Contact : thl@whu.edu.cn
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYPES_H
#define TYPES_H

#include <Eigen/Geometry>

using Eigen::Matrix3d;
using Eigen::Quaterniond;
using Eigen::Vector3d;

typedef struct GNSS {
    double time;

    Vector3d blh;
    Vector3d std;

    // GNSS速度(n系, 北东地)及其标准差, 仅在观测文件包含速度时有效
    // GNSS velocity (n-frame, NED) and its std, valid only if the file contains velocity
    Vector3d vel;
    Vector3d stdvel;
    bool hasvel;

    // 航向量测(双天线航向, 已折算到本机航向基准), 仅在观测文件包含航向时有效
    // heading measurement (dual-antenna heading, in this unit's yaw reference)
    double heading;
    double stdheading;
    bool hasheading;

    bool isvalid;
} GNSS;

typedef struct IMU {
    double time;
    double dt;

    Vector3d dtheta;
    Vector3d dvel;

    double odovel;
} IMU;

typedef struct Pose {
    Matrix3d R;
    Vector3d t;
} Pose;

#endif // TYPES_H
