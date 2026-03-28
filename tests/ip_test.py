# -*- coding: utf-8 -*-

def GET_CURRENT_SUBNET_BROADCAST_IP():
    # type: () -> str | None
    """获取当前网段广播地址（x.x.x.255）
    """
    ip = None
    try:
        import socket
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    finally:
        s.close()
    if not ip:
        return None
    parts = ip.split(".")
    if len(parts) != 4:
        return None
    return "%s.%s.%s.255" % (parts[0], parts[1], parts[2])


print(GET_CURRENT_SUBNET_BROADCAST_IP())