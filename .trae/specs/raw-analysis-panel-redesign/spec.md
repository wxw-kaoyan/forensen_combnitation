# 原始数据分析面板重构 Spec

## Why
当前 `RawAnalysisPanel` 是扁平单列布局，无法扩展不同数据类别的分析；
质量摘要以 HTML 表格展示，缺乏异常提示；图表高度固定，无法按需拉伸或细节浏览。

## What Changes
- **布局重构**：左右 QSplitter，左侧 QComboBox + QTreeWidget，右侧内容区
- **左侧导航**：
  - 顶部 QComboBox，选择当前分析的「数据类型」（目前仅「GNSS 原始观测值」，后续可扩展）
  - 下方 QTreeWidget，可展开节点：质量摘要 / 卫星数曲线 / SNR曲线
- **右侧内容区**：
  - 文件选择行（保留：路径编辑框 + 浏览 + 开始分析）
  - 替换原 HTML 表格：改为「检测报告」单段落，包含观测记录总数、历元总数、时长、采样率、丢历元数，并标注 SNR 异常时段
  - 移除「系统构成」小节（GPS/GLO/GAL/QZS/BDS 卫星数明细不再展示）
- **图表交互**：
  - 每个图表底部有拖拽柄，可上下拖动调整图表区域高度
  - 鼠标滚轮缩放：同时缩放 X 轴（时间）和 Y 轴（数值）
  - 鼠标拖拽平移
- **树节点联动**：点击左侧不同树节点，右侧内容区整体刷新显示对应内容

## Impact
- 受影响文件：
  - `modules/analysis/raw/rawanalysispanel.h`
  - `modules/analysis/raw/rawanalysispanel.cpp`
- 不受影响文件：`rinexreader.h` / `rinexreader.cpp`（数据解析逻辑不变）
- **BREAKING**：`RawAnalysisPanel` 的 UI 结构与之前完全不同，外部无直接依赖其私有成员，无破坏性风险

## ADDED Requirements
### Requirement: 左侧数据类别导航
面板左侧 SHALL 包含一个 QComboBox（数据类型选择）和一个 QTreeWidget（分析节点树）。
用户 SHALL 能通过 ComboBox 切换数据类型；当前仅实现「GNSS 原始观测值」。

#### Scenario: 默认状态
- **WHEN** 面板初始化完成
- **THEN** 左侧 ComboBox 显示「GNSS 原始观测值」，树节点「质量摘要」处于选中状态，右侧显示检测报告

### Requirement: 检测报告段落
右侧 SHALL 显示一段检测报告，包含：观测记录总数、历元总数、时长、采样率、丢历元数（含总间断时长），以及 SNR 异常提示（如有）。

#### Scenario: 数据正常
- **WHEN** 所有系统 SNR 均在正常范围内，无丢历元
- **THEN** 报告段落显示各项统计值，末尾标注「数据质量正常，无明显异常」

#### Scenario: 存在异常
- **WHEN** SNR 存在偏低频段或丢历元超过阈值
- **THEN** 报告段落标注异常时段及异常类型（如「L1 频段 SNR 偏低（均值 28dBHz）」「时段 T1~T2 存在历元丢失」）

### Requirement: 图表拖拽拉伸
每个图表 SHALL 底部带有可拖拽高度调节柄，用户拖动可改变图表在右侧内容区中的高度占比。

#### Scenario: 拖动图表柄
- **WHEN** 用户按住图表底部拖拽柄向上拖动
- **THEN** 该图表高度增大，相邻图表/区域高度相应缩小，总高度不变

### Requirement: 图表滚轮缩放与平移
每个图表 SHALL 支持鼠标滚轮双轴缩放和鼠标拖拽平移。

#### Scenario: 滚轮缩放
- **WHEN** 鼠标悬停在图表上并滚动滚轮
- **THEN** X 轴和 Y 轴同时放大或缩小（以鼠标位置为中心）

#### Scenario: 拖拽平移
- **WHEN** 用户在图表上按住左键并拖动
- **THEN** 图表内容沿拖动方向平移

## MODIFIED Requirements
### Requirement: 原质量摘要表格
**Reason**: 扁平表格无法承载异常信息，且「系统构成」小节用户不需要。
**替换为**: 检测报告段落（单段落 HTML 格式，含统计值 + 异常标注）

### Requirement: 原固定高度图表区
**Reason**: 无法查看细节也无法整体浏览。
**替换为**: 带拖拽柄的浮动图表区 + 滚轮缩放 + 拖拽平移

## REMOVED Requirements
### Requirement: 系统构成小节
**Reason**: 用户明确表示不需要。
**迁移**: 如需查看各系统卫星数，后续可在树节点中增加「卫星分布」子项（不在此次实现）

### Requirement: SNR 表格（QTableWidget）
**Reason**: 已整合进检测报告段落，表格形式不再需要。
**迁移**: SNR 统计值保留在检测报告段落中展示
