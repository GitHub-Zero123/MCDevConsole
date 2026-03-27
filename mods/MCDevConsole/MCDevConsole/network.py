# -*- coding: utf-8 -*-
""" MCDevConsole py2 NET 系统处理器

功能：
- 通过 UDP 广播自动发现服务器（端口 18232）
- 周期性尝试连接到发现的服务器（0.3s 重试一次）
- 支持非阻塞连接和通信
- 提供 stdout/stderr 重定向方法
- 提供连接状态检查方法
 """

import socket
import struct
import threading
import time
import json
# import sys
TYPE_STDOUT_LOG = 1
TYPE_STDERR_LOG = 2
TYPE_EXEC_CLIENT = 3
TYPE_EXEC_SERVER = 4

DISCOVERY_PORT = 18232
DISCOVERY_BROADCAST = "MCDEVCONSOLE_DISCOVER"
DISCOVERY_RESPONSE_PREFIX = "MCDEVCONSOLE_TCP"
DISCOVERY_INTERVAL = 0.3  # UDP 发现间隔（秒）
RECONNECT_INTERVAL = 0.3  # TCP 重连间隔（秒）
SOCKET_TIMEOUT = 0.05     # 套接字超时（秒）

def U16_BE(b):
    """ 大端序读取 2 字节无符号整数 """
    # type: (bytearray | str) -> int
    if isinstance(b, bytearray):
        return (b[0] << 8) | b[1]
    return (ord(b[0]) << 8) | ord(b[1])

def U32_BE(b):
    """ 大端序读取 4 字节无符号整数 """
    # type: (bytearray | str) -> int
    if isinstance(b, bytearray):
        return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]
    return (ord(b[0]) << 24) | (ord(b[1]) << 16) | (ord(b[2]) << 8) | ord(b[3])

def SERIALIZE_PACKET(type_id, data):
    """ 序列化数据包：[typeId 2B big-endian][size 4B big-endian][data] """
    # type: (int, str | bytearray) -> bytearray
    payload = data.encode("utf-8") if isinstance(data, str) else data
    size = len(payload)
    header = struct.pack(">HI", type_id, size)
    return bytearray(header) + bytearray(payload)

class NETSystem(object):
    """ NET 系统 - 支持 UDP 自动发现和非阻塞连接 """
    def __init__(self):
        # type: () -> None
        self.serverHost = None
        self.serverPort = None
        self.sock = None
        self.connected = False
        self.running = False
        self.mLock = threading.Lock()
        self.handlers = {}
        self.recvBuffer = bytearray()
        self.lastDiscoveryTime = 0
        self.lastReconnectTime = 0
        self.listenThread = None
        self.discoveryThread = None
        self.connectedHandler = None
    
    def setConnectedHandler(self, handler):
        """ 设置连接成功回调函数，连接成功时会调用 handler() """
        # type: (callable) -> None
        self.connectedHandler = handler
        return self
    
    def registerHandler(self, type_id, handler):
        """ 注册消息处理器 """
        # type: (int, callable) -> None
        with self.mLock:
            self.handlers[type_id] = handler
    
    def updateHandlers(self, handlers):
        """ 批量更新消息处理器 """
        # type: (dict[int, callable]) -> None
        with self.mLock:
            self.handlers.update(handlers)
    
    def isConnected(self):
        """ 检查是否处于通信状态（非阻塞） """
        # type: () -> bool
        with self.mLock:
            return self.connected and self.sock is not None
    
    def send_stdout(self, message, metadata=None):
        """ 发送 stdout 日志，支持批量发送 (message 可以是 str 或 list) """
        # type: (str | list, dict | None) -> bool
        return self._send_log(TYPE_STDOUT_LOG, message, metadata)
    
    def send_stderr(self, message, metadata=None):
        """ 发送 stderr 日志，支持批量发送 (message 可以是 str 或 list) """
        # type: (str | list, dict | None) -> bool
        return self._send_log(TYPE_STDERR_LOG, message, metadata)
    
    def _send_log(self, typeId, message, metadata=None):
        """ 内部方法：发送日志，message 可以是字符串或列表 """
        # type: (int, str | list[str], dict | None) -> bool
        if not self.isConnected():
            return False

        try:
            payload = {"message": message}
            if metadata:
                payload.update(metadata)
            
            data = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
            packet = SERIALIZE_PACKET(typeId, data)
            
            with self.mLock:
                if self.sock:
                    self.sock.sendall(bytes(packet))
                    return True
            return False
        except Exception as e:
            print("[NETSystem] 发送日志失败: " + str(e))
            return False
    
    def start(self):
        """ 启动 NET 系统（非阻塞） """
        with self.mLock:
            if self.running:
                return
            self.running = True
        
        # 启动 UDP 发现线程
        self.discoveryThread = threading.Thread(target=self._threadDiscoveryLoop)
        self.discoveryThread.daemon = True
        self.discoveryThread.start()
        
        # 启动 TCP 监听线程
        self.listenThread = threading.Thread(target=self._threadListenLoop)
        self.listenThread.daemon = True
        self.listenThread.start()
        
        print("[NETSystem] NET 系统已启动，开始 UDP 发现...")
    
    def close(self):
        """ 关闭 NET 系统 """
        with self.mLock:
            self.running = False
            sock = self.sock
            self.sock = None
            self.connected = False
        
        if sock:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except:
                pass
            try:
                sock.close()
            except:
                pass
        
        print("[NETSystem] 已关闭")
    
    def _threadDiscoveryLoop(self):
        """ 内部线程：UDP 发现循环 """
        print("[NETSystem] UDP 发现线程已启动")
        
        udp_sock = None
        try:
            udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            udp_sock.settimeout(DISCOVERY_INTERVAL)
            
            while True:
                with self.mLock:
                    if not self.running:
                        break
                
                current_time = time.time()
                if current_time - self.lastDiscoveryTime >= DISCOVERY_INTERVAL:
                    try:
                        # 发送 UDP 广播发现包
                        udp_sock.sendto(DISCOVERY_BROADCAST.encode("utf-8"), ("255.255.255.255", DISCOVERY_PORT))
                        
                        # 尝试接收响应
                        try:
                            data, addr = udp_sock.recvfrom(512)
                            response = data.decode("utf-8", errors="ignore")
                            
                            if response.startswith(DISCOVERY_RESPONSE_PREFIX):
                                parts = response.split()
                                if len(parts) >= 3:
                                    serverIp = parts[1]
                                    serverPort = int(parts[2])
                                    
                                    with self.mLock:
                                        self.serverHost = serverIp
                                        self.serverPort = serverPort
                                    
                                    # print(serverIp)
                                    # print("[NETSystem] 发现服务器: " + serverIp + ":" + str(serverPort))
                        except socket.timeout:
                            pass
                    
                    except Exception as e:
                        print("[NETSystem] UDP 发现异常: " + str(e))
                        # import traceback
                        # traceback.print_exc()
                    
                    self.lastDiscoveryTime = current_time
                else:
                    time.sleep(0.01)
        
        except Exception as e:
            print("[NETSystem] UDP 发现线程异常: " + str(e))
        finally:
            if udp_sock:
                try:
                    udp_sock.close()
                except:
                    pass
            print("[NETSystem] UDP 发现线程已退出")
    
    def _threadListenLoop(self):
        """ 内部线程：TCP 监听循环 - 周期性尝试连接和接收数据 """
        print("[NETSystem] TCP 监听线程已启动")
        
        while True:
            with self.mLock:
                if not self.running:
                    break
            
            # 尝试连接
            if not self.isConnected():
                with self.mLock:
                    host = self.serverHost
                    port = self.serverPort
                
                if host and port:
                    current_time = time.time()
                    if current_time - self.lastReconnectTime >= RECONNECT_INTERVAL:
                        self._tryConnect(host, port)
                        self.lastReconnectTime = current_time
                    else:
                        time.sleep(0.01)
                        continue
                else:
                    time.sleep(0.1)
                    continue
            
            # 接收数据
            try:
                with self.mLock:
                    if not self.sock or not self.connected:
                        continue
                    sock = self.sock
                
                try:
                    chunk = sock.recv(4096)
                    if not chunk:
                        # 连接已关闭
                        with self.mLock:
                            self.connected = False
                            self.sock = None
                        print("[NETSystem] 服务器断开连接")
                        continue
                    
                    self.recvBuffer.extend(chunk)
                    self._processRecvBuffer()
                
                except socket.timeout:
                    # 超时是正常的，继续
                    pass
                except socket.error as e:
                    with self.mLock:
                        self.connected = False
                        self.sock = None
                    print("[NETSystem] 套接字错误: " + str(e))
            
            except Exception as e:
                print("[NETSystem] 接收异常: " + str(e))
                with self.mLock:
                    self.connected = False
                    self.sock = None
                time.sleep(0.1)
        
        with self.mLock:
            if self.sock:
                try:
                    self.sock.close()
                except:
                    pass
                self.sock = None
        
        print("[NETSystem] TCP 监听线程已退出")
    
    def _tryConnect(self, host, port):
        """ 尝试连接到服务器 """
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(SOCKET_TIMEOUT)
            sock.connect((host, port))
            
            with self.mLock:
                self.sock = sock
                self.connected = True
                self.recvBuffer = bytearray()
            
            # 连接成功后立即发送初始化消息，避免服务器超时断开
            self._sendHandshake()
            if self.connectedHandler:
                self.connectedHandler()
            # print("[NETSystem] 已连接到调试服务器，地址：%s:%d" % (host, port))
        except socket.error:
            # 连接失败是正常的，会在下一个周期重试
            pass
        except Exception as e:
            # print("[NETSystem] 连接异常: " + str(e))
            import traceback
            traceback.print_exc()
    
    @staticmethod
    def getPlatformInfoName():
        """获取平台信息字符串"""
        try:
            import platform
            return platform.system() + " " + platform.release()
        except:
            return "Unknown Platform"
    
    def _sendHandshake(self):
        """发送握手消息"""
        try:            
            handshakeData = json.dumps({
                "message": "Python Client Connected",
                "platform": NETSystem.getPlatformInfoName(),
                "name": "Minecraft"
            }, ensure_ascii=True, separators=(",", ":"))
            packet = SERIALIZE_PACKET(TYPE_STDOUT_LOG, handshakeData)
            
            with self.mLock:
                if self.sock:
                    self.sock.sendall(bytes(packet))
        except Exception as e:
            print("[NETSystem] 握手失败: " + str(e))
    
    def _processRecvBuffer(self):
        """处理接收缓冲区中的数据包"""
        while len(self.recvBuffer) >= 6:
            try:
                # 解析包头
                typeID = U16_BE(self.recvBuffer[0:2])
                dataLength = U32_BE(self.recvBuffer[2:6])
                
                # 检查数据长度合法性
                if dataLength > 1024 * 1024:
                    print("[NETSystem] 数据包过大，丢弃")
                    self.recvBuffer = bytearray()
                    break
                
                # 检查是否收到完整数据包
                if len(self.recvBuffer) < 6 + dataLength:
                    break
                
                # 提取数据
                data = self.recvBuffer[6:6 + dataLength]
                self.recvBuffer = self.recvBuffer[6 + dataLength:]
                
                # 调用处理器
                with self.mLock:
                    if typeID in self.handlers:
                        handler = self.handlers[typeID]
                    else:
                        handler = None
                
                if handler:
                    try:
                        handler(data)
                    except Exception as e:
                        print("[NETSystem] 处理器异常: " + str(e))
                        import traceback
                        traceback.print_exc()
                else:
                    print("[NETSystem] 未知的 TypeID 数据包: " + str(typeID))
            
            except Exception as e:
                print("[NETSystem] 处理数据包异常: " + str(e))
                import traceback
                traceback.print_exc()
                break

# 全局 NET 系统实例
_NETSYSTEM = None   # type: NETSystem | None

def GET_NET_SYSTEM():
    """获取或创建全局 NET 系统实例"""
    # type: () -> NETSystem
    global _NETSYSTEM
    if _NETSYSTEM is None:
        _NETSYSTEM = NETSystem()
    return _NETSYSTEM