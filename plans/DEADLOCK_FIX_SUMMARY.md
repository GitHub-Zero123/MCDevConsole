# MCDevConsole 卡死问题修复总结

## 已修复的关键问题

### 🔴 问题 1：`recv()` 在持有锁时执行（已修复）
**位置**: `network.py:253-275`

**原问题**:
```python
with self.mLock:
    if not self.sock or not self.connected:
        continue
    sock = self.sock

try:
    chunk = sock.recv(4096)  # ← 阻塞操作在锁内！
```

**修复方案**:
```python
# 在锁外获取 socket 引用
sock = None
with self.mLock:
    if self.sock and self.connected:
        sock = self.sock

if sock:
    # recv() 在锁外执行
    chunk = sock.recv(4096)
```

**影响**: 避免主线程调用 `send_stdout()` 时被阻塞，防止游戏卡死。

---

### 🔴 问题 2：`_processRecvBuffer()` 在持有锁时调用（已修复）
**位置**: `network.py:273-275`

**原问题**:
```python
with self.mLock:
    self.recvBuffer.extend(chunk)
    self._processRecvBuffer()  # ← 在锁内调用！
```

这会导致**死锁链**:
1. 监听线程持有 `mLock`
2. 调用 `_processRecvBuffer()` → 调用 handler
3. Handler 触发 `print()` → 日志系统调用 `send_stdout()`
4. `send_stdout()` 尝试获取 `mLock` → **死锁**！

**修复方案**:
```python
with self.mLock:
    self.recvBuffer.extend(chunk)
# 在锁外处理缓冲区
self._processRecvBuffer()
```

**影响**: 避免 handler 中的日志输出导致死锁。

---
