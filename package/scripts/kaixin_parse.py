# -*- coding: utf-8 -*-
"""
凯芯数据解析脚本
调用方式：python kaixin_parse.py <dat_path> <out_csv_path>
依赖：MATLAB Runtime 的 libmx.dll（自动搜索）与 decode_kaixin.mexw64（自动搜索脚本同目录/exe目录/桌面）
输入给 mex 的是 uint8 原始字节数组（与 MATLAB 脚本一致）
输出：CSV（UTF-8，103列，已做单位换算）+ stdout JSON摘要
"""
import sys, os, json, ctypes, struct, collections, math
import numpy as np

def _find_matlab_bin():
    """按优先级搜索包含 libmx.dll 的目录（MATLAB / MATLAB Runtime 的 bin/win64），返回目录；找不到返回 None"""
    here = os.path.dirname(os.path.abspath(__file__))
    exe_dir = os.path.dirname(here)
    candidates = [
        os.path.join(here, 'bin', 'win64'),            # 脚本同目录下打包的 Runtime
        os.path.join(here, 'matlab', 'bin', 'win64'),
        os.path.join(here, 'runtime', 'bin', 'win64'),
        os.path.join(exe_dir, 'bin', 'win64'),         # exe 目录下打包的 Runtime
        os.path.join(exe_dir, 'runtime', 'bin', 'win64'),
        os.environ.get('MATLAB_BIN', ''),
        os.path.join(os.environ.get('MATLAB_ROOT', ''), 'bin', 'win64'),
        r'B:\matlab\bin\win64',                        # 开发机兜底
    ]

    # MATLAB Runtime 常见安装目录（取版本号最高的子目录）
    pf = os.environ.get('ProgramFiles', r'C:\Program Files')
    rt_root = os.path.join(pf, 'MATLAB', 'MATLAB Runtime')
    if os.path.isdir(rt_root):
        try:
            vers = sorted([d for d in os.listdir(rt_root) if d.upper().startswith('R')],
                          reverse=True)
        except OSError:
            vers = []
        for v in vers:
            candidates.append(os.path.join(rt_root, v, 'bin', 'win64'))

    for p in candidates:
        if p and os.path.isfile(os.path.join(p, 'libmx.dll')):
            return os.path.normpath(p)
    return None


def _find_mex():
    """按优先级搜索 decode_kaixin.mexw64，返回路径；找不到返回 None"""
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(here, 'decode_kaixin.mexw64'),          # 与脚本同目录（打包后）
        os.path.join(here, '..', 'decode_kaixin.mexw64'),    # 脚本上一级（exe 目录）
        os.path.join(os.getcwd(), 'decode_kaixin.mexw64'),   # 当前工作目录
        os.path.join(os.path.expanduser('~'), 'Desktop', 'decode_kaixin.mexw64'),  # 桌面（旧路径）
    ]
    for p in candidates:
        p = os.path.normpath(p)
        if os.path.isfile(p):
            return p
    return None


MATLAB_BIN = _find_matlab_bin()
if MATLAB_BIN:
    os.add_dll_directory(MATLAB_BIN)
    os.environ['PATH'] = MATLAB_BIN + os.pathsep + os.environ.get('PATH', '')

# ──────────────────────────────────────────────
# 全部 146 列名（0-based，对应 MATLAB 1-based 注释）
# ──────────────────────────────────────────────
ALL_NAMES = [
    'itow','week','gx','gy','gz','ax','ay','az','temp','temp_vel',                 # 0-9
    'imu_type','zupt','roll_ahrs','pitch_ahrs','yaw_ahrs',                         # 10-14
    'increment_type','anglex','angley','anglez','speedx','speedy','speedz',         # 15-21
    'gps_pos_itow','week2','pps_us','recv_time_ms','fix_type','numsv','multiSVs',  # 22-28
    'lat','lng','hgt','undulation','p_hacc','p_vacc','diff_age',                   # 29-35
    'gps_vel_itow','vn','ve','vd','gspeed','headMot','v_hacc','v_vacc',            # 36-43
    'itow2','fix_type2','heading','heading_std','pitch2','pitch_std','length','svnum', # 44-51
    'kf_itow',                                                                     # 52
    'pk0','pk1','pk2','pk3','pk4','pk5','pk6','pk7','pk8','pk9','pk10',           # 53-63
    'pk11','pk12','pk13','pk14','pk15','pk16','pk17','pk18','pk19','pk20',        # 64-73
    'innov0','innov1','innov2','innov3','innov4','innov5',                         # 74-79
    'att0','att1','att2','vel0','vel1','vel2','pos0','pos1','pos2',               # 80-88
    'imu_bias0','imu_bias1','imu_bias2','imu_bias3','imu_bias4','imu_bias5',       # 89-94
    'gyro_bias0','gyro_bias1','gyro_bias2','arm_x','arm_y','arm_z',               # 95-100
    'SETTING','GYRO_STD','ACCEL_STD','GYRO_ZUPT_VALUE','MEAS_CONSTRAIN_AHRS',     # 101-105
    'ALIGN_VEL','PARAM_ANTENNA_INST_ANGLE','ANTENNA_LENGTH','PARAM_INST_ROLL',    # 106-109
    'PARAM_INST_PITCH','PARAM_INST_YAW','PARAM_GPS_POS_X','PARAM_GPS_POS_Y',      # 110-113
    'PARAM_GPS_POS_Z','PARAM_OD_POS_X','PARAM_OD_POS_Y','PARAM_OD_POS_Z',         # 114-117
    'PARAM_CONVERT_POS_X','PARAM_CONVERT_POS_Y','PARAM_CONVERT_POS_Z',            # 118-120
    'RESULT_POS','LEVER_ARM_ENABLE','RTK_FLAG','INCREMENT_FLAG','KF_VERSION','VERSION', # 121-126
]
for _i in range(127, 146):
    ALL_NAMES.append(f'col{_i}')

# ──────────────────────────────────────────────
# 全 0 列（实测全程为0，剔除）
# ──────────────────────────────────────────────
ALL_ZERO_COLS = {
    9,                                            # temp_vel
    44,45,46,47,48,49,50,51,                      # 双天线 DRTK 8 列
    70,71,72,73,                                  # pk17~pk20
    95,96,97,                                     # gyro_bias0~2
    101,                                          # SETTING
    107,108,109,110,111,                          # PARAM_ANTENNA_INST_ANGLE..PARAM_INST_YAW
    121,                                          # RESULT_POS
    125,                                          # KF_VERSION
}
ALL_ZERO_COLS.update(range(127, 146))

KEEP_IDX   = [i for i in range(146) if i not in ALL_ZERO_COLS]
KEEP_NAMES = [ALL_NAMES[i] for i in KEEP_IDX]

# ──────────────────────────────────────────────
# 中文表头
# ──────────────────────────────────────────────
CN = {
    'itow':'周内毫秒(IMU)','week':'GPS周',
    'gx':'陀螺X(deg/s)','gy':'陀螺Y(deg/s)','gz':'陀螺Z(deg/s)',
    'ax':'加表X(g)','ay':'加表Y(g)','az':'加表Z(g)','temp':'温度(C)',
    'imu_type':'IMU类型','zupt':'零速检测',
    'roll_ahrs':'AHRS横滚(deg)','pitch_ahrs':'AHRS俯仰(deg)','yaw_ahrs':'AHRS航向(deg)',
    'increment_type':'增量类型',
    'anglex':'角增量X','angley':'角增量Y','anglez':'角增量Z',
    'speedx':'速度增量X','speedy':'速度增量Y','speedz':'速度增量Z',
    'gps_pos_itow':'周内毫秒(GPS位置)','week2':'GPS周(GPS)',
    'pps_us':'PPS时刻(us)','recv_time_ms':'接收时刻(ms)',
    'fix_type':'定位类型','numsv':'解算星数','multiSVs':'多频星数',
    'lat':'纬度(deg)','lng':'经度(deg)','hgt':'海拔(m)',
    'undulation':'大地水准面差(m)','p_hacc':'水平精度(cm)','p_vacc':'垂直精度(cm)',
    'diff_age':'差分龄期',
    'gps_vel_itow':'周内毫秒(GPS速度)',
    'vn':'北向速度(m/s)','ve':'东向速度(m/s)','vd':'地向速度(m/s)',
    'gspeed':'地面速度(m/s)','headMot':'运动航向(deg)',
    'v_hacc':'速度水平精度','v_vacc':'速度垂直精度',
    'kf_itow':'周内毫秒(KF)',
    'att0':'组合横滚(deg)','att1':'组合俯仰(deg)','att2':'组合航向(deg)',
    'vel0':'组合北向速度(m/s)','vel1':'组合东向速度(m/s)','vel2':'组合地向速度(m/s)',
    'pos0':'组合纬度(deg)','pos1':'组合经度(deg)','pos2':'组合高度(m)',
    'arm_x':'杆臂X(m)','arm_y':'杆臂Y(m)','arm_z':'杆臂Z(m)',
    'GYRO_STD':'陀螺振动STD','ACCEL_STD':'加表振动STD',
    'GYRO_ZUPT_VALUE':'ZUPT陀螺阈值','MEAS_CONSTRAIN_AHRS':'AHRS量测约束',
    'ALIGN_VEL':'对准速度阈值',
    'PARAM_GPS_POS_X':'GPS安装X','PARAM_GPS_POS_Y':'GPS安装Y','PARAM_GPS_POS_Z':'GPS安装Z',
    'PARAM_OD_POS_X':'里程计安装X','PARAM_OD_POS_Y':'里程计安装Y','PARAM_OD_POS_Z':'里程计安装Z',
    'PARAM_CONVERT_POS_X':'转换X','PARAM_CONVERT_POS_Y':'转换Y','PARAM_CONVERT_POS_Z':'转换Z',
    'LEVER_ARM_ENABLE':'杆臂使能','RTK_FLAG':'RTK标志',
    'INCREMENT_FLAG':'增量标志','VERSION':'版本号',
}
for i in range(21): CN[f'pk{i}']     = f'P对角线{i}'
for i in range(6):  CN[f'innov{i}']  = f'量测新息{i}'
for i in range(6):  CN[f'imu_bias{i}'] = f'IMU零偏{i}'

R2D = 180.0 / math.pi


def _load_mex():
    if not MATLAB_BIN:
        raise FileNotFoundError(
            "未找到 MATLAB 运行时 (libmx.dll)。\n"
            "请安装 MATLAB Runtime，或将包含 libmx.dll 的 bin/win64 目录放到脚本/exe 目录下。")
    mx = ctypes.CDLL(os.path.join(MATLAB_BIN, 'libmx.dll'))
    mwSize = ctypes.c_size_t
    mxArray = ctypes.c_void_p
    mxUINT8_CLASS = 9
    mxREAL = 0

    def decl(obj, name, restype=None, argtypes=None):
        f = getattr(obj, name)
        if restype is not None: f.restype = restype
        if argtypes is not None: f.argtypes = argtypes
        return f

    def try_decl(obj, names, restype=None, argtypes=None):
        last_err = None
        for n in names:
            try: return decl(obj, n, restype, argtypes)
            except AttributeError as e: last_err = e
        raise last_err

    CreateNA = try_decl(mx,
        ['mxCreateNumericArray_730','mxCreateNumericArray_800','mxCreateNumericArray'],
        mxArray, [mwSize, ctypes.POINTER(mwSize), ctypes.c_int, ctypes.c_int])
    GetData  = decl(mx, 'mxGetData',   ctypes.c_void_p,              [mxArray])
    GetPr    = decl(mx, 'mxGetPr',     ctypes.POINTER(ctypes.c_double), [mxArray])
    GetM     = decl(mx, 'mxGetM',      mwSize,                       [mxArray])
    GetN     = decl(mx, 'mxGetN',      mwSize,                       [mxArray])
    Destroy  = decl(mx, 'mxDestroyArray', None,                      [mxArray])

    mex_path = _find_mex()
    if not mex_path:
        raise FileNotFoundError(
            "未找到 decode_kaixin.mexw64（凯芯解码器）。\n"
            "请将其放到 kaixin_parse.py 同目录、exe 目录或桌面。")
    lib = ctypes.CDLL(mex_path)
    mexFn = lib.mexFunction
    mexFn.restype  = None
    mexFn.argtypes = [ctypes.c_int, ctypes.POINTER(mxArray),
                      ctypes.c_int, ctypes.POINTER(mxArray)]
    return (mx, lib, CreateNA, GetData, GetPr, GetM, GetN, Destroy,
            mwSize, mxArray, mxUINT8_CLASS, mxREAL)


def parse_kaixin(dat_path):
    """返回 (N,146) numpy 数组"""
    (mx, lib, CreateNA, GetData, GetPr, GetM, GetN, Destroy,
     mwSize, mxArray, mxUINT8_CLASS, mxREAL) = _load_mex()

    with open(dat_path, 'rb') as f:
        raw = f.read()
    N = len(raw)

    dims = (mwSize * 2)(N, 1)
    pa = CreateNA(2, dims, mxUINT8_CLASS, mxREAL)
    if not pa:
        raise RuntimeError("mxCreateNumericArray failed")
    ctypes.memmove(GetData(pa), raw, N)

    plhs = (mxArray * 1)()
    prhs = (mxArray * 1)(pa)
    lib.mexFunction(1, plhs, 1, prhs)
    out = plhs[0]
    if not out:
        Destroy(pa)
        raise RuntimeError("mexFunction 未返回结果")

    m, n = int(GetM(out)), int(GetN(out))
    buf = ctypes.cast(GetPr(out), ctypes.POINTER(ctypes.c_double))
    arr = np.ctypeslib.as_array(buf, shape=(m*n,)).reshape(n, m).copy()
    Destroy(pa); Destroy(out)
    return arr


def count_frames(dat_path):
    with open(dat_path, 'rb') as f:
        d = f.read()
    ids = collections.Counter()
    i, n = 0, len(d)
    while i < n - 6:
        if d[i] == 0xAA and d[i+1] == 0x55:
            fid = struct.unpack_from('<H', d, i+2)[0]
            ln  = struct.unpack_from('<H', d, i+4)[0]
            ids[fid] += 1
            i += 6 + ln + 4
        else:
            i += 1
    return ids, len(d)


def write_csv(arr, csv_path):
    """从 146 列剔除全零列、做单位换算后写 CSV"""
    data = arr[:, KEEP_IDX].astype(np.float64, copy=True)
    n2i  = {n: i for i, n in enumerate(KEEP_NAMES)}

    def scale(name, factor):
        if name not in n2i: return
        ci = n2i[name]
        m = data[:, ci] != 0
        data[m, ci] = data[m, ci] * factor

    # ahrs 角度：×0.01（原始为 0.01deg）
    for k in ('roll_ahrs','pitch_ahrs','yaw_ahrs'):
        scale(k, 0.01)
    # GPS 高度：/100000 → 米
    scale('hgt', 1e-5)
    # 组合经纬度：弧度 → 度
    scale('pos0', R2D)
    scale('pos1', R2D)
    # 组合高度 pos2 已经是米，不动
    # att0/1/2 已经是度（运动时约1.7°/-1.9°/356°），不动
    # lat/lng 已经是度，不动
    # gx/gy/gz 是 deg/s，ax/ay/az 是 g，不动

    header_cn = [CN.get(n, n) for n in KEEP_NAMES]
    out_dir = os.path.dirname(csv_path)
    if out_dir: os.makedirs(out_dir, exist_ok=True)
    with open(csv_path, 'w', encoding='utf-8') as f:
        f.write(','.join(header_cn) + '\n')
        chunk = 200000
        for i in range(0, data.shape[0], chunk):
            np.savetxt(f, data[i:i+chunk], delimiter=',', fmt='%.12g')


def main():
    if len(sys.argv) < 3:
        print("usage: kaixin_parse.py <dat_path> <out_csv_path>", file=sys.stderr)
        sys.exit(1)
    dat_path, csv_path = sys.argv[1], sys.argv[2]

    ids, fsize = count_frames(dat_path)
    arr = parse_kaixin(dat_path)
    total_rows = arr.shape[0]

    # 主轴用 itow（200Hz），剔除 itow==0 的未填充行
    itow_idx = 0
    valid = arr[:, itow_idx] != 0
    arr = arr[valid]
    valid_rows = arr.shape[0]

    write_csv(arr, csv_path)

    # 统计
    gps_rows = int(np.count_nonzero(arr[:, 29]))   # lat!=0
    fix50    = int(np.count_nonzero((arr[:,29]!=0) & (arr[:,26]==50)))  # fix_type=50 固定解
    week_val     = int(arr[0, 1])  if valid_rows else 0
    inc_type     = int(arr[0, 15]) if valid_rows else 0
    ver_col = ALL_NAMES.index('VERSION')
    vers_nz = arr[arr[:, ver_col] != 0, ver_col]
    version_val = int(vers_nz[0]) if len(vers_nz) else 0

    summary = {
        'ok': True,
        'fileSize': fsize,
        'totalRows': int(total_rows),
        'validRows': int(valid_rows),
        'gpsRows': gps_rows,
        'rtkFixRows': fix50,
        'week': week_val,
        'incrementType': inc_type,
        'version': version_val,
        'frameIds': {('0x%04X' % k): v for k, v in sorted(ids.items())},
        'csvPath': csv_path,
        'columns': len(KEEP_NAMES),
    }
    print("KAIXIN_SUMMARY_BEGIN")
    print(json.dumps(summary, ensure_ascii=False))
    print("KAIXIN_SUMMARY_END")


if __name__ == '__main__':
    main()
