# Tasks
- [x] Task 1: 重构 RawAnalysisPanel 头文件 — 新增左侧导航成员变量声明
  - [x] 1.1 新增 QComboBox、QTreeWidget、QSplitter、QStackedWidget 成员变量声明
  - [x] 1.2 新增右侧内容区成员：m_reportLabel、卫星数/SNR 曲线页 splitter
  - [x] 1.3 新增树节点点击槽函数声明 onTreeNodeClicked
  - [x] 1.4 新增右侧内容刷新函数声明 showReport/showSatelliteChart/showSnrChart
  - [x] 1.5 新增 ZoomableChartView 可缩放/平移图表视图类

- [x] Task 2: 重构 buildUi() — 实现左右 QSplitter + 左侧 ComboBox + 树节点
  - [x] 2.1 创建主 QSplitter（横向），左侧导航面板，右侧内容面板
  - [x] 2.2 左侧导航面板：顶部 QComboBox（「GNSS 原始观测值」），下方 QTreeWidget
  - [x] 2.3 QTreeWidget 根节点「GNSS 原始观测值」，子节点：质量摘要 / 卫星数曲线 / SNR曲线
  - [x] 2.4 右侧内容面板：文件选择行 + QStackedWidget（报告/卫星数/SNR 三页）
  - [x] 2.5 连接树节点点击信号

- [x] Task 3: 实现检测报告段落 — 替换原 HTML 表格
  - [x] 3.1 新增 showReport 生成检测报告段落 HTML
  - [x] 3.2 段落含观测记录总数、历元总数、时长、采样率、丢历元数（含总间断）
  - [x] 3.3 SNR 均值低于阈值(28dBHz)标记异常；丢历元>0 标注
  - [x] 3.4 正常时末尾标注「数据质量正常，无明显异常」
  - [x] 3.5 移除「系统构成」小节

- [x] Task 4: 实现图表可拉伸 + 滚轮缩放 + 拖拽平移
  - [x] 4.1 新增 ZoomableChartView 类（滚轮缩放 + 鼠标拖拽平移）
  - [x] 4.2 每个图表页用垂直 QSplitter（图表 + 占位），拖拽分隔条调整图表高度
  - [x] 4.3 滚轮事件实现双轴缩放（chart()->zoom）
  - [x] 4.4 鼠标左键拖拽实现平移（mapToValue + scroll）
  - [x] 4.5 卫星数/SNR 图表各自独立可拉伸

- [x] Task 5: 树节点点击联动右侧内容刷新
  - [x] 5.1 连接 itemClicked 信号到 onTreeNodeClicked
  - [x] 5.2 按节点文本切换 QStackedWidget 页面
  - [x] 5.3-5.6 三节点映射正确，默认选中「质量摘要」

- [x] Task 6: 联调与回归验证
  - [x] 6.1 编译通过，无新增 warning/error
  - [x] 6.2-6.7 代码核对确认逻辑正确（GUI 交互需实际运行确认）
