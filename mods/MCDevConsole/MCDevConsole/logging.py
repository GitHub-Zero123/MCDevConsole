# -*- coding: utf-8 -*-
from .ub import sys
import threading
from .containers import CachedLists
from time import time

class LogINFO:
    def __init__(self, log):
        self.mLog = log
        self.createTime = time()

class LoggingProxySystem:
    _INSTANCE = None

    @staticmethod
    def getInstance():
        if LoggingProxySystem._INSTANCE is None:
            LoggingProxySystem._INSTANCE = LoggingProxySystem()
        return LoggingProxySystem._INSTANCE

    def __init__(self):
        self.stdout = sys.stdout
        self.stderr = sys.stderr
        self.logCache = CachedLists()
        self.errorCache = CachedLists()
        self.sendLogHandler = None
        self.sendErrorLogHandler = None
    
    def setLogHandler(self, handler):
        """ 设置日志发送处理器，handler 接受一个字符串列表参数 """
        self.sendLogHandler = handler
        return self
    
    def setErrorLogHandler(self, handler):
        """ 设置错误日志发送处理器，handler 接受一个字符串列表参数 """
        self.sendErrorLogHandler = handler
        return self

    def logUpdateHandler(self, line):
        if self.sendLogHandler and self.sendLogHandler(line):
            return
        self.logCache.append(LogINFO(line))

    def errorUpdateHandler(self, line):
        if self.sendErrorLogHandler and self.sendErrorLogHandler(line):
            return
        self.errorCache.append(LogINFO(line))

    def updateCachedLogs(self):
        """ 释放并发送缓存的日志，按时间顺序分组合批发送 """
        if not self.sendLogHandler and not self.sendErrorLogHandler:
            return
        if len(self.logCache) == 0 and len(self.errorCache) == 0:
            return
        
        # 收集所有日志并按时间排序
        all_logs = []
        if self.sendLogHandler and len(self.logCache) > 0:
            all_logs.extend([(info.createTime, 0, info.mLog) for info in self.logCache])
            self.logCache.clear()
        
        if self.sendErrorLogHandler and len(self.errorCache) > 0:
            all_logs.extend([(info.createTime, 1, info.mLog) for info in self.errorCache])
            self.errorCache.clear()
        
        if not all_logs:
            return
        
        # 按时间排序
        all_logs.sort(key=lambda x: x[0])
        
        # 按类型连续分组发送
        current_type = None
        current_batch = []
        
        for _, log_type, log_msg in all_logs:
            if log_type != current_type:
                # 发送上一批
                if current_batch:
                    if current_type == 0 and self.sendLogHandler:
                        self.sendLogHandler(current_batch)
                    elif current_type == 1 and self.sendErrorLogHandler:
                        self.sendErrorLogHandler(current_batch)
                
                # 开始新批次
                current_type = log_type
                current_batch = [log_msg]
            else:
                current_batch.append(log_msg)
        
        # 发送最后一批
        if current_batch:
            if current_type == 0 and self.sendLogHandler:
                self.sendLogHandler(current_batch)
            elif current_type == 1 and self.sendErrorLogHandler:
                self.sendErrorLogHandler(current_batch)

    def startProxy(self):
        sys.stdout = STD_OUT_WRAPPER(sys.stdout, updateHandler=self.logUpdateHandler)
        sys.stderr = STD_OUT_WRAPPER(sys.stderr, updateHandler=self.errorUpdateHandler)

    def closeProxy(self):
        sys.stdout = self.stdout
        sys.stderr = self.stderr

class STD_OUT_WRAPPER(object):
    def __init__(self, baseIO, updateHandler=None):
        self.baseIO = baseIO
        self.updateHandler = updateHandler
        self.writeLock = threading.Lock()
        self._buffer = []

    def __getattr__(self, name):
        return getattr(self.baseIO, name)

    def write(self, data):
        # type: (str) -> None
        with self.writeLock:
            parts = data.splitlines(True)
            for part in parts:
                if part.endswith("\n"):
                    if self._buffer:
                        line = "".join(self._buffer) + part
                        self._buffer = []
                    else:
                        line = part
                    # self.baseIO.write("[{}] ".format(self.getFormatTime()) + line)
                    self.baseIO.write(line)
                    if self.updateHandler:
                        self.updateHandler(line)
                else:
                    self._buffer.append(part)

    def close(self):
        return self.baseIO.close()

    def writelines(self, lines):
        for line in lines:
            self.write(line)

    def fileno(self):
        return self.baseIO.fileno()