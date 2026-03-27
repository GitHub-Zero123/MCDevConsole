# MCDevConsole - 我的世界开发者远程调试工具

## 项目概述

**项目名称**: MCDevConsole（我的世界开发者控制台）  
**完整名称**: NetEase MCDevConsole / MCDevConsole for Netease Minecraft  
**项目类型**: PC 远程调试客户端  
**技术栈**: C++ (WebView2 宿主) + Vue3 + Vite (前端)  
**目标用户**: 网易我的世界开发者  
**核心场景**: 手机游戏环境 ↔ PC客户端远程链接测试  
**部署范围**: PC 客户端仅实现

---

## 核心功能

### 1. 远程连接 (Remote Connection) ⭐ 核心
- **PC端**: 调试客户端（WebView2 + Vue3 实现）
- TCP/WebSocket双向通信
- **UDP自动发现**: 监听固定UDP端口（18232）自动发现局域网设备
- **多客户端支持**: 支持多个手机设备同时连接
- 连接状态实时监控
- 断线自动重连机制
- **会话保持**: 设备断开连接后保留会话数据，除非用户显性关闭

### 2. 实时日志捕获 (Real-time Log Capture)
- 游戏运行时日志实时推送到PC
- 日志级别分类（DEBUG, INFO, WARN, ERROR, FATAL）
- 时间戳和线程ID记录
- **内存缓存**: 最多缓存 10,000 行日志
- **自动清理**: 超过 10,000 行时自动清理最老的 25% 数据（2,500 行）
- **导出功能**: 用户点击导出按钮可将日志保存为文件

### 3. 日志搜索和筛选 (Log Search & Filter)
- 关键词搜索（支持正则表达式）
- 按日志级别筛选
- 按时间范围筛选
- 按模块/标签筛选
- 搜索历史记录
- 高亮显示匹配结果
- 导出搜索结果

### 4. 远程代码执行 (Remote Code Execution)
- 在手机游戏环境中执行代码
- 支持Lua脚本执行
- 支持Python脚本执行（通过C++桥接）
- 执行结果实时反馈
- 错误堆栈跟踪
- 执行超时控制
- 输出大小限制

### 5. 多设备管理 (Multi-Device Management)
- 左侧面板显示所有已发现和已连接设备
- 实时显示设备连接状态（在线/离线）
- 断开连接后保留会话数据和日志
- 用户可手动关闭会话以清理数据
- 支持切换不同设备会话查看

### 6. 主题和界面 (Theme & UI)
- **黑白主题切换**: 支持深色和浅色主题
- **主题持久化**: 主题设置保存在浏览器 localStorage
- **VSCode风格布局**: 高端化设计，简洁线条，微妙阴影
- **无边框窗口**: 自定义标题栏和窗口控制

---

## 项目结构

```
MCDevConsole/
│
├── docs/                          # 文档目录
│   ├── README.md                  # 项目说明
│   ├── ARCHITECTURE.md            # 架构设计文档
│   ├── REMOTE_PROTOCOL.md         # 远程通信协议文档
│   ├── API.md                     # API文档
│   └── DEVELOPMENT.md             # 开发指南
│
├── src/
│   └── client/                    # PC客户端（WebView2 + Vue3）
│       ├── cpp/                   # C++ WebView2 宿主程序
│       │   ├── CMakeLists.txt
│       │   ├── include/
│       │   │   ├── WebView2Host.h        # WebView2宿主
│       │   │   ├── ThemeManager.h        # 主题管理（黑白切换）
│       │   │   ├── WindowManager.h       # 无边框窗口管理
│       │   │   └── Bridge.h              # C++与前端通信桥接
│       │   └── src/
│       │       ├── main.cpp
│       │       ├── WebView2Host.cpp
│       │       ├── ThemeManager.cpp
│       │       ├── WindowManager.cpp
│       │       └── Bridge.cpp
│       │
│       └── web/                   # 前端代码（Vue3 + Vite）
│           ├── package.json
│           ├── vite.config.js
│           ├── index.html
│           ├── src/
│           │   ├── main.js
│           │   ├── App.vue
│           │   ├── components/
│           │   │   ├── DeviceList.vue         # 设备列表（左侧面板）
│           │   │   ├── LogViewer.vue          # 日志查看器
│           │   │   ├── SearchPanel.vue        # 搜索面板
│           │   │   ├── ExecutorPanel.vue      # 代码执行面板
│           │   │   ├── ConnectionPanel.vue    # 连接管理面板
│           │   │   ├── TitleBar.vue           # 自定义标题栏
│           │   │   └── SettingsDialog.vue     # 设置对话框
│           │   ├── stores/
│           │   │   ├── connection.js          # 连接状态管理
│           │   │   ├── logs.js                # 日志数据管理（内存缓存）
│           │   │   ├── devices.js             # 设备管理
│           │   │   └── theme.js               # 主题管理（localStorage）
│           │   ├── api/
│           │   │   ├── client.js              # 远程客户端API
│           │   │   ├── protocol.js            # 协议实现
│           │   │   └── discovery.js           # UDP设备发现
│           │   ├── utils/
│           │   │   ├── logger.js              # 日志工具
│           │   │   ├── export.js              # 导出工具
│           │   │   └── constants.js           # 常量定义
│           │   ├── styles/
│           │   │   ├── theme-light.css        # 白色主题
│           │   │   ├── theme-dark.css         # 黑色主题
│           │   │   └── vscode-layout.css      # VSCode风格布局
│           │   └── assets/
│           └── public/
│
├── protocol/                      # 通信协议定义
│   ├── messages.json              # 消息格式定义
│   └── protocol_spec.md           # 协议规范文档
│
├── build/                         # 构建输出目录
│   └── .gitkeep
│
├── tests/                         # 测试
│   ├── unit_tests.cpp
│   └── integration_tests.cpp
│
├── examples/                      # 使用示例
│   ├── basic_connection.js
│   └── device_discovery.js
│
├── .gitignore
├── CMakeLists.txt                 # C++项目配置
├── LICENSE
└── README.md                      # 项目主说明文档
```

---

## 技术栈详情

### PC客户端 (WebView2 + Vue3)
- **宿主程序**: C++ + WebView2 SDK
- **窗口**: 无边框应用程序（自定义标题栏）
- **主题**: 支持黑白主题切换，保存在 localStorage
- **布局风格**: VSCode布局，高端黑白风格
- **前端框架**: Vue3 + Vite
- **状态管理**: Pinia
- **网络**: 轻量级现代 C++ 跨平台 socket 库 + WebSocket / Fetch API
- **网络库约束**: 禁止引入 Boost.Asio、Qt Network 等重量级库，优先选择 header-only 或最小依赖方案
- **推荐方向**: 独立封装 WinSock/BSD Socket，或引入轻量级跨平台库（如 standalone Asio）
- **UI组件**: 自定义组件库
- **日志存储**: 内存缓存（最多10,000行）

---

## 核心设计

### 日志缓存管理
```javascript
// 日志缓存配置
const LOG_CONFIG = {
  MAX_LOGS: 10000,           // 最大日志行数
  CLEANUP_THRESHOLD: 10000,  // 触发清理的阈值
  CLEANUP_RATIO: 0.25        // 清理比例（25%）
};

// 自动清理逻辑
function addLog(log) {
  logs.push(log);
  
  if (logs.length > LOG_CONFIG.CLEANUP_THRESHOLD) {
    const removeCount = Math.floor(
      LOG_CONFIG.CLEANUP_THRESHOLD * LOG_CONFIG.CLEANUP_RATIO
    );
    logs.splice(0, removeCount);  // 删除最老的2500行
  }
}
```

### 主题持久化
```javascript
// 主题保存到 localStorage
function setTheme(theme) {
  localStorage.setItem('app-theme', theme);
  applyTheme(theme);
}

// 应用启动时恢复主题
function initTheme() {
  const savedTheme = localStorage.getItem('app-theme') || 'dark';
  applyTheme(savedTheme);
}
```

### 日志导出
```javascript
// 用户点击导出按钮
function exportLogs() {
  const content = logs.map(log => 
    `[${log.timestamp}] [${log.level}] ${log.message}`
  ).join('\n');
  
  const blob = new Blob([content], { type: 'text/plain' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `logs-${Date.now()}.txt`;
  a.click();
}
```

### UDP自动发现
```cpp
// 基于轻量级 UDP socket 封装监听广播端口 18232
void start_discovery() {
  udp_socket.bind(18232);

  udp_socket.on_message([](std::string_view data, const Endpoint& remote) {
    auto device = parse_device_announce(data);
    add_device(device);
  });
}
```

### WebView2桥接
```cpp
// C++端：发送消息到前端
void Bridge::send_to_web(const std::string& message) {
  webview_->PostWebMessageAsJson(
    utf8_to_utf16(message).c_str()
  );
}

// C++端：接收前端消息
void Bridge::on_web_message(const std::string& message) {
  auto json = nlohmann::json::parse(message);
  handle_message(json);
}
```

```javascript
// JavaScript端：接收C++消息
window.chrome.webview.addEventListener('message', (event) => {
  const message = JSON.parse(event.data);
  handleMessage(message);
});

// JavaScript端：发送消息到C++
function sendToCpp(message) {
  window.chrome.webview.postMessage(JSON.stringify(message));
}
```

---

## 远程通信协议

### UDP设备发现流程
```
1. 服务器端启动
   - 监听固定UDP端口 18232
   - 定期广播设备信息到局域网
   
2. 客户端发现
   - 监听UDP广播端口 18232
   - 接收设备信息：{device_id, device_name, ip, tcp_port}
   - 在左侧面板显示发现的设备列表
   - 自动感知新设备加入和离线
```

### TCP连接建立流程
```
1. 用户选择设备
   PC端 -> 手机端: CONNECT {client_id, client_name, version}
   手机端 -> PC端: CONNECT_ACK {session_id, server_version, capabilities}

2. 会话管理
   - 为每个设备创建独立会话
   - 会话ID用于标识不同设备
   - 支持多个设备同时连接

3. 心跳保活
   PC端 <-> 手机端: HEARTBEAT (每30秒)
   
4. 断线处理
   - 设备断开：会话标记为离线，数据保留
   - 用户显性关闭：清理会话数据
   - 自动重连：使用原session_id恢复会话
```

### 二进制协议格式（TCP 连接后）

所有 TCP 数据包采用统一的二进制格式：

```
[typeId: 2B][size: 4B][data: variable]
```

- **typeId** (2 字节，大端序): 消息类型 ID
- **size** (4 字节，大端序): 数据部分长度（不包括 typeId 和 size 本身）
- **data** (variable): 实际数据内容（JSON 或二进制）

### 消息类型定义

| typeId | 方向 | 说明 | 数据格式 |
|--------|------|------|---------|
| 1 | 服务端 → PC | stdout 日志 | JSON: `{level, tag, message, timestamp, thread_id}` |
| 2 | 服务端 → PC | stderr 日志 | JSON: `{level, tag, message, timestamp, thread_id}` |
| 3 | PC → 服务端 | 执行客户端代码 | JSON: `{code, language, timeout}` |
| 4 | 服务端 → PC | 执行服务端代码 | JSON: `{code, language, timeout}` |

### 消息格式（JSON 部分）
```json
{
  "type": "MESSAGE_TYPE",
  "session_id": "xxx",
  "timestamp": 1234567890,
  "payload": {
    // 具体数据
  }
}
```

### 消息类型

#### 设备发现相关
```
UDP_ANNOUNCE: UDP广播设备信息
  {device_id, device_name, ip, tcp_port, version}
```

#### 连接相关
```
CONNECT: 连接请求
  {client_id, client_name, version}

CONNECT_ACK: 连接确认
  {session_id, server_version, capabilities}

HEARTBEAT: 心跳
  {}

DISCONNECT: 断开连接
  {reason}

SESSION_CLOSE: 关闭会话（清理数据）
  {session_id}

CONNECTION_STATUS: 连接状态
  {status, message}
```

#### 日志相关
```
LOG_MESSAGE: 日志消息推送
  {level, tag, message, timestamp, thread_id}

LOG_BATCH: 批量日志推送
  {logs: [{level, tag, message, timestamp}...]}

CLEAR_LOGS: 清空日志
  {}
```

#### 代码执行相关
```
EXECUTE_CODE: 执行代码
  {code, language, timeout}

EXECUTE_RESULT: 执行结果
  {status, output, error, execution_time}

EXECUTE_CANCEL: 取消执行
  {execution_id}
```

#### 搜索相关
```
SEARCH_LOGS: 搜索日志
  {keyword, filters, limit, offset}

SEARCH_RESULT: 搜索结果
  {matches: [{...}...], total_count}
```

#### 设备管理相关
```
DEVICE_LIST: 获取设备列表
  {}

DEVICE_LIST_RESPONSE: 设备列表响应
  {devices: [{device_id, device_name, status, connected_at}...]}

DEVICE_SWITCH: 切换设备
  {device_id}
```

---

## 命名规范

### 代码命名
- **C++**: PascalCase (类名), snake_case (函数/变量)
- **JavaScript/Vue**: camelCase (函数/变量), PascalCase (组件名)
- **文件**: snake_case.cpp / kebab-case.vue / kebab-case.js

### 包/模块命名
- **C++命名空间**: 统一使用 `MCDevConsole` 作为根命名空间，例如 `MCDevConsole::WebView`, `MCDevConsole::Bridge`, `MCDevConsole::Network`
- **命名空间要求**: 所有 C++ 项目代码、类型、模块封装均应归属到 `MCDevConsole` 命名空间体系下，禁止继续使用 `mcdevconsole`、匿名散落命名空间作为对外主命名空间
- **JavaScript模块**: `@/components`, `@/api`, `@/stores`

### 常量命名
- **C++**: UPPER_SNAKE_CASE
- **JavaScript**: UPPER_SNAKE_CASE

### 协议常量定义
```cpp
namespace MCDevConsole::Protocol {

// 消息类型 ID
constexpr uint16_t TYPE_STDOUT_LOG = 1;      // 服务端 -> PC: stdout 日志
constexpr uint16_t TYPE_STDERR_LOG = 2;      // 服务端 -> PC: stderr 日志
constexpr uint16_t TYPE_EXEC_CLIENT = 3;     // PC -> 服务端: 执行客户端代码
constexpr uint16_t TYPE_EXEC_SERVER = 4;     // 服务端 -> PC: 执行服务端代码

// 协议头大小
constexpr size_t HEADER_SIZE = 6;            // typeId(2B) + size(4B)
constexpr size_t TYPE_ID_SIZE = 2;
constexpr size_t SIZE_FIELD_SIZE = 4;

// 最大数据包大小
constexpr size_t MAX_PACKET_SIZE = 1024 * 1024;  // 1MB

} // namespace MCDevConsole::Protocol
```

---

## 数据存储策略

### 浏览器 localStorage
```javascript
// 主题设置
localStorage.setItem('app-theme', 'dark');

// 用户偏好设置
localStorage.setItem('app-preferences', JSON.stringify({
  autoScroll: true,
  logDisplayLimit: 10000,
  searchHistory: [...]
}));

// 设备连接历史
localStorage.setItem('device-history', JSON.stringify([
  { device_id, device_name, last_connected }
]));
```

### 内存缓存
```javascript
// 日志缓存（最多10,000行）
const logStore = {
  logs: [],
  
  addLog(log) {
    this.logs.push(log);
    if (this.logs.length > 10000) {
      this.logs.splice(0, 2500);  // 清理最老的25%
    }
  },
  
  getLogs(filter) {
    return this.logs.filter(filter);
  },
  
  clear() {
    this.logs = [];
  }
};
```

### 文件导出
```javascript
// 用户点击导出按钮时生成文件
function exportLogs(format = 'txt') {
  const content = formatLogs(logStore.logs, format);
  downloadFile(content, `logs-${Date.now()}.${format}`);
}

function exportSearchResults(results) {
  const content = formatResults(results, 'txt');
  downloadFile(content, `search-results-${Date.now()}.txt`);
}
```

---

## UI设计要点

### 布局结构（VSCode风格）
```
┌─────────────────────────────────────────────────────┐
│  [标题栏] MCDevConsole        [主题] [最小化] [关闭] │
├──────────┬──────────────────────────────────────────┤
│          │  [连接状态] [搜索框] [工具栏]              │
│  设备    ├──────────────────────────────────────────┤
│  列表    │                                          │
│          │                                          │
│  ● 设备1 │          日志查看区域                     │
│  ○ 设备2 │                                          │
│  ○ 设备3 │                                          │
│          │                                          │
│  [+发现] ├──────────────────────────────────────────┤
│          │  [代码执行面板]                           │
└──────────┴──────────────────────────────────────────┘
```

### 主题设计
- **黑色主题**: 深色背景 (#1e1e1e)，高对比度文字
- **白色主题**: 浅色背景 (#ffffff)，柔和文字
- **高端化**: 简洁线条，微妙阴影，流畅动画
- **VSCode风格**: 侧边栏、面板、状态栏布局

### 设备列表（左侧面板）
- 显示所有已发现和已连接设备
- 在线状态：绿色圆点 ●
- 离线状态：灰色圆点 ○
- 点击切换不同设备会话
- 右键菜单：重连、关闭会话、查看详情

### 日志查看器
- 实时显示日志流
- 按级别着色（DEBUG、INFO、WARN、ERROR、FATAL）
- 支持搜索和筛选
- 导出按钮导出当前日志
- 自动滚动到最新日志

---

## 开发阶段规划

### Phase 1: 基础架构 (Week 1-2)
- [ ] 项目结构搭建
- [ ] WebView2宿主程序框架
- [ ] 无边框窗口实现
- [ ] Vue3 + Vite 前端框架搭建
- [ ] C++ 与前端桥接实现

### Phase 2: 设备发现和连接 (Week 3-4)
- [ ] UDP设备发现实现
- [ ] TCP连接建立
- [ ] 设备列表UI
- [ ] 会话管理
- [ ] 心跳和重连机制

### Phase 3: 日志系统 (Week 5-6)
- [ ] 日志接收和内存缓存
- [ ] 自动清理机制（10,000行 -> 清理25%）
- [ ] 日志查看器UI
- [ ] 日志搜索功能
- [ ] 日志导出功能

### Phase 4: 代码执行 (Week 7-8)
- [ ] 代码执行引擎
- [ ] Lua脚本支持
- [ ] 执行结果反馈
- [ ] 执行面板UI

### Phase 5: 主题和优化 (Week 9-10)
- [ ] 黑白主题实现
- [ ] localStorage 主题持久化
- [ ] VSCode风格布局优化
- [ ] 动画和交互优化
- [ ] 性能优化

### Phase 6: 测试和发布 (Week 11-12)
- [ ] 单元测试
- [ ] 集成测试
- [ ] 错误处理完善
- [ ] 文档完善
- [ ] 版本发布

---

## 关键技术实现

### WebView2宿主程序
```cpp
#include <webview2.h>

class WebView2Host {
private:
    ICoreWebView2Controller* webview_controller_;
    ICoreWebView2* webview_;
    
public:
    void Initialize(HWND hwnd);
    void SendMessage(const std::string& message);
    void OnWebMessage(const std::string& message);
};
```

### 无边框窗口
```cpp
// 移除窗口边框
SetWindowLong(hwnd, GWL_STYLE, 
    GetWindowLong(hwnd, GWL_STYLE) & ~WS_CAPTION);

// 自定义标题栏处理
LRESULT OnNCHitTest(HWND hwnd, LPARAM lParam) {
    // 检测点击位置，允许拖动标题栏
    int y = GET_Y_LPARAM(lParam);
    if (y < TITLE_BAR_HEIGHT) {
        return HTCAPTION;
    }
    return HTCLIENT;
}
```

### 日志缓存管理
```javascript
// Pinia store
import { defineStore } from 'pinia';

export const useLogStore = defineStore('logs', {
  state: () => ({
    logs: [],
    MAX_LOGS: 10000,
    CLEANUP_RATIO: 0.25
  }),
  
  actions: {
    addLog(log) {
      this.logs.push(log);
      
      if (this.logs.length > this.MAX_LOGS) {
        const removeCount = Math.floor(
          this.MAX_LOGS * this.CLEANUP_RATIO
        );
        this.logs.splice(0, removeCount);
      }
    },
    
    exportLogs() {
      const content = this.logs
        .map(log => `[${log.timestamp}] [${log.level}] ${log.message}`)
        .join('\n');
      
      const blob = new Blob([content], { type: 'text/plain' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `logs-${Date.now()}.txt`;
      a.click();
    }
  }
});
```

### 主题管理
```javascript
// Pinia store
export const useThemeStore = defineStore('theme', {
  state: () => ({
    theme: localStorage.getItem('app-theme') || 'dark'
  }),
  
  actions: {
    setTheme(theme) {
      this.theme = theme;
      localStorage.setItem('app-theme', theme);
      document.documentElement.setAttribute('data-theme', theme);
    }
  }
});
```

### UDP设备发现
```cpp
namespace MCDevConsole::Network {

class UdpDiscoveryClient {
public:
    void Start();
    void Stop();

private:
    void HandleMessage(std::string_view payload);
    LightweightUdpSocket socket_;
};

} // namespace MCDevConsole::Network
```

---

## 版本号规范

采用语义化版本 (Semantic Versioning): `MAJOR.MINOR.PATCH`

- **MAJOR**: 不兼容的API变更
- **MINOR**: 新增功能（向后兼容）
- **PATCH**: 错误修复

示例: `1.0.0`, `1.1.0`, `1.1.1`

---

## 许可证

建议使用: MIT License 或 Apache 2.0

---

## 更新日志

### v1.0.0 (计划发布)
- 初始版本发布
- UDP自动设备发现（端口18232）
- 多设备同时连接支持
- 会话持久化（断线保留数据）
- WebView2无边框客户端
- 黑白主题切换（localStorage存储）
- VSCode风格布局
- 实时日志捕获和显示
- 日志内存缓存（最多10,000行，自动清理25%）
- 日志导出功能
- 日志搜索功能
- 代码执行功能
