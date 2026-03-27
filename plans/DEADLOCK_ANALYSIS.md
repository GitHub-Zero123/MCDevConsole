# MCDevConsole 卡死问题分析

## 发现的关键问题

### 1. **严重：`_threadListenLoop` 中的死锁风险** ⚠️ 最高优先级
**位置**: [`network.py:226-297`](mods/MCDevConsole/MCDevConsole/network.py:226)

**问题描述**:
```python
def _threadListenLoop(self):
    while True:
        with self.mLock:
            if not self.running:
                break
        
        # 尝试连接
        if not self.isConnected():  # ← 这里再次获取锁！
            with self.mLock:
                host = self.serverHost
                port = self.serverPort
            ...
        
        # 接收数据
        try:
            with self.mLock:  # ← 再次获取锁
                if not self.sock or not self.connected:
                    continue
                sock = self.sock
            
            try:
                chunk = sock.recv(4096)  # ← 在持有锁的情况下进行阻塞操作！
```

**为什么会卡死**:
- `sock.recv(4096)` 是一个**阻塞操作**，即使设置了 `SOCKET_TIMEOUT = 0.05`
- 在 `with self.mLock:` 块内执行 `recv()` 会导致**长时间持有锁**
- 如果主线程（或其他线程）尝试调用 `send_stdout()` 或 `send_stderr()`，会在 `with self.mLock:` 处**永久阻塞**
- 这会导致日志发送卡死，进而导致整个 Mod 卡死

**具体场景**:
1. TCP 监听线程在 `sock.recv()` 处阻塞（等待数据）
2. 游戏主线程调用 `send_stdout()` 发送日志
3. `send_stdout()` 尝试获取 `self.mLock`，但被监听线程持有
4. 主线程卡死，游戏卡顿

---

### 2. **严重：`_processRecvBuffer` 中的长时间锁持有**
**位置**: [`network.py:386-429`](mods/MCDevConsole/MCDevConsole/network.py:386)

**问题描述**:
```python
def _processRecvBuffer(self):
    while len(self.recvBuffer) >= 6:
        try:
            # ... 处理数据包 ...
            with self.mLock:
                if typeID in self.handlers:
                    handler = self.handlers[typeID]
                else:
                    handler = None
            
            if handler:
                try:
                    handler(data)  # ← 在锁外调用处理器（好的）
                except Exception as e:
                    print("[NETSystem] 处理器异常: " + str(e))
```

**问题**:
- 虽然处理器在锁外调用，但如果处理器内部调用 `send_stdout()` 等方法，会导致**嵌套锁**
- 如果处理器执行时间过长，会阻塞接收线程

---

### 3. **中等：`_tryConnect` 中的阻塞操作**
**位置**: [`network.py:299-328`](mods/MCDevConsole/MCDevConsole/network.py:299)

**问题描述**:
```python
def _tryConnect(self, host, port):
    try:
        ip = socket.gethostbyname(host)  # ← 可能阻塞！
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(SOCKET_TIMEOUT)
        sock.connect((ip, port))  # ← 虽然有超时，但仍可能阻塞
```

**问题**:
- `socket.gethostbyname()` 可能进行 DNS 查询，导致长时间阻塞
- 虽然 `connect()` 有 0.05 秒超时，但在某些网络条件下仍可能卡顿

---

### 4. **中等：`logging.py` 中的锁竞争**
**位置**: [`logging.py:116-132`](mods/MCDevConsole/MCDevConsole/logging.py:116)

**问题描述**:
```python
def write(self, data):
    with self.writeLock:  # ← 持有锁
        parts = data.splitlines(True)
        for part in parts:
            if part.endswith("\n"):
                # ...
                self.baseIO.write(line)  # ← 可能阻塞！
                if self.updateHandler:
                    self.updateHandler(line)  # ← 调用网络发送
```

**问题**:
- `self.baseIO.write()` 可能阻塞（写入文件/控制台）
- `self.updateHandler()` 会调用 `send_stdout()`，尝试获取网络锁
- 如果网络线程也在尝试写入日志，会形成**死锁**

---

## 修复方案

### 方案 1：分离 `recv()` 和锁的作用域（推荐）
```python
def _threadListenLoop(self):
    while True:
        with self.mLock:
            if not self.running:
                break
        
        # 尝试连接
        if not self.isConnected():
            # ... 连接逻辑 ...
        
        # 接收数据 - 在锁外进行
        sock = None
        with self.mLock:
            if self.sock and self.connected:
                sock = self.sock
        
        if sock:
            try:
                chunk = sock.recv(4096)  # ← 在锁外！
                if not chunk:
                    with self.mLock:
                        self.connected = False
                        self.sock = None
                    continue
                
                with self.mLock:
                    self.recvBuffer.extend(chunk)
                    self._processRecvBuffer()
            except socket.timeout:
                pass
            except socket.error as e:
                with self.mLock:
                    self.connected = False
                    self.sock = None
```

### 方案 2：使用非阻塞套接字
```python
sock.setblocking(False)
try:
    chunk = sock.recv(4096)
except BlockingIOError:
    pass  # 没有数据可读
```

### 方案 3：使用 `select` 或 `selectors` 模块
```python
import selectors

selector = selectors.DefaultSelector()
selector.register(sock, selectors.EVENT_READ)
events = selector.select(timeout=0.05)
if events:
    chunk = sock.recv(4096)
```

---

## 总结

| 问题 | 严重程度 | 影响 | 修复难度 |
|------|--------|------|--------|
| `recv()` 在锁内 | 🔴 严重 | 导致主线程卡死 | 低 |
| 长时间锁持有 | 🔴 严重 | 日志发送卡顿 | 低 |
| DNS 查询阻塞 | 🟡 中等 | 连接延迟 | 中 |
| 日志写入阻塞 | 🟡 中等 | 日志系统卡顿 | 中 |

**建议立即修复**：问题 1 和 2，这两个是导致卡死的主要原因。
