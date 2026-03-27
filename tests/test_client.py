#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MCDevConsole 测试客户端

功能：
- 每 0.3s 广播 UDP 发现包到 255.255.255.255:18232
- 收到服务端响应后解析 IP 和端口，建立 TCP 连接
- 连接成功后每秒输出 5 条 INFO 日志 + 1 条 STDERR 日志
- 接收并执行服务端下发的 Python 代码（客户端/服务端）
"""

import socket
import struct
import time
import threading
import json
import sys
from datetime import datetime

DISCOVERY_PORT = 18232
BROADCAST_INTERVAL = 0.3
LOG_INTERVAL = 0.2

TYPE_STDOUT_LOG = 1
TYPE_STDERR_LOG = 2
TYPE_EXEC_CLIENT = 3
TYPE_EXEC_SERVER = 4


def serialize_packet(type_id, data):
    """序列化数据包：[typeId 2B big-endian][size 4B big-endian][data]"""
    payload = data.encode('utf-8') if isinstance(data, str) else data
    size = len(payload)
    header = struct.pack('>HI', type_id, size)
    return header + payload


def deserialize_packet(buffer):
    """反序列化数据包，返回 (type_id, data, consumed_size) 或 None"""
    if len(buffer) < 6:
        return None
    
    type_id, size = struct.unpack('>HI', buffer[:6])
    if size > 1024 * 1024:
        return None
    
    total_size = 6 + size
    if len(buffer) < total_size:
        return None
    
    data = buffer[6:total_size]
    return type_id, data, total_size


class MCDevConsoleClient:
    def __init__(self):
        self.tcp_socket = None
        self.running = False
        self.connected = False
        self.recv_buffer = bytearray()
        self.log_counter = 0
        
    def discover_server(self):
        """UDP 广播发现服务端"""
        udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        udp_sock.settimeout(BROADCAST_INTERVAL)
        
        print(f"[发现] 开始 UDP 广播发现，目标端口 {DISCOVERY_PORT}")
        
        while self.running and not self.connected:
            try:
                udp_sock.sendto(b'MCDEVCONSOLE_DISCOVER', ('255.255.255.255', DISCOVERY_PORT))
                
                try:
                    data, addr = udp_sock.recvfrom(512)
                    response = data.decode('utf-8', errors='ignore')
                    
                    if response.startswith('MCDEVCONSOLE_TCP'):
                        parts = response.split()
                        if len(parts) >= 3:
                            server_ip = parts[1]
                            server_port = int(parts[2])
                            server_name = parts[3] if len(parts) > 3 else 'MCDevConsole'
                            print(f"[发现] 找到服务端 {server_name} @ {server_ip}:{server_port}")
                            udp_sock.close()
                            return server_ip, server_port
                except socket.timeout:
                    pass
                
            except Exception as e:
                print(f"[发现] 广播异常: {e}")
                time.sleep(BROADCAST_INTERVAL)
        
        udp_sock.close()
        return None, None
    
    def connect_server(self, host, port):
        """建立 TCP 连接"""
        try:
            self.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.tcp_socket.connect((host, port))
            self.connected = True
            print(f"[连接] TCP 连接成功 {host}:{port}")
            return True
        except Exception as e:
            print(f"[连接] TCP 连接失败: {e}")
            return False
    
    def send_log(self, level, message, time_str=''):
        """发送日志到服务端"""
        if not self.connected or not self.tcp_socket:
            return
        
        try:
            payload = {
                'message': message,
                'platform': sys.platform,
                'name': socket.gethostname()
            }
            if time_str:
                payload['time'] = time_str
            
            type_id = TYPE_STDERR_LOG if level == 'STDERR' else TYPE_STDOUT_LOG
            packet = serialize_packet(type_id, json.dumps(payload, ensure_ascii=False, separators=(',', ':')))
            self.tcp_socket.sendall(packet)
        except Exception as e:
            print(f"[日志] 发送失败: {e}")
            self.connected = False
    
    def recv_thread(self):
        """接收线程：处理服务端下发的执行命令"""
        print("[接收] 接收线程已启动")
        
        while self.running and self.connected:
            try:
                chunk = self.tcp_socket.recv(4096)
                if not chunk:
                    print("[接收] 服务端断开连接")
                    self.connected = False
                    break
                
                self.recv_buffer.extend(chunk)
                
                while True:
                    result = deserialize_packet(self.recv_buffer)
                    if not result:
                        break
                    
                    type_id, data, consumed = result
                    self.recv_buffer = self.recv_buffer[consumed:]
                    
                    code = data.decode('utf-8', errors='ignore')
                    target = '服务端' if type_id == TYPE_EXEC_SERVER else '客户端'
                    print(f"[执行] 收到 {target} 代码执行请求")
                    print(f"[执行] 代码内容:\n{code}")
                    
                    try:
                        exec_globals = {'__name__': '__main__'}
                        exec(code, exec_globals)
                        self.send_log('INFO', f'[执行成功] {target} 代码已执行')
                    except Exception as e:
                        self.send_log('STDERR', f'[执行失败] {target} 代码执行异常: {e}')
                
            except Exception as e:
                if self.running:
                    print(f"[接收] 异常: {e}")
                self.connected = False
                break
    
    def log_thread(self):
        """日志线程：每秒输出 5 条 INFO + 1 条 STDERR"""
        print("[日志] 日志线程已启动")
        
        while self.running and self.connected:
            try:
                for i in range(5):
                    self.log_counter += 1
                    time_str = datetime.now().strftime('%H:%M:%S')
                    self.send_log('INFO', f'测试日志 #{self.log_counter}', time_str)
                    time.sleep(LOG_INTERVAL)
                
                time_str = datetime.now().strftime('%H:%M:%S')
                self.send_log('STDERR', f'测试错误日志 #{self.log_counter // 5}', time_str)
                
            except Exception as e:
                print(f"[日志] 异常: {e}")
                break
    
    def run(self):
        """主运行循环"""
        self.running = True
        
        while self.running:
            server_ip, server_port = self.discover_server()
            
            if not server_ip or not self.running:
                continue
            
            if not self.connect_server(server_ip, server_port):
                time.sleep(1)
                continue

            self.send_log('INFO', '[握手] 客户端已连接', datetime.now().strftime('%H:%M:%S'))
            
            recv_t = threading.Thread(target=self.recv_thread, daemon=True)
            log_t = threading.Thread(target=self.log_thread, daemon=True)
            
            recv_t.start()
            log_t.start()
            
            recv_t.join()
            log_t.join()
            
            if self.tcp_socket:
                try:
                    self.tcp_socket.close()
                except:
                    pass
                self.tcp_socket = None
            
            self.connected = False
            print("[主循环] 连接断开，3 秒后重新发现...")
            time.sleep(3)
    
    def stop(self):
        """停止客户端"""
        self.running = False
        self.connected = False
        if self.tcp_socket:
            try:
                self.tcp_socket.close()
            except:
                pass


def main():
    client = MCDevConsoleClient()
    
    try:
        client.run()
    except KeyboardInterrupt:
        print("\n[主程序] 收到中断信号，正在退出...")
        client.stop()
        sys.exit(0)


if __name__ == '__main__':
    main()
