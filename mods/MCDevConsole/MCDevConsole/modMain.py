# -*- coding: utf-8 -*-
from mod.common.mod import Mod
import mod.server.extraServerApi as serverApi
import mod.client.extraClientApi as clientApi
from . import network
from . import logging
from .ub import mCompile, mEval

_SYSTEM_REF = 0
_NET_SYSTEM = network.NETSystem()

def sendLogHandler(logLines):
    if not _NET_SYSTEM.isConnected():
        return False
    return _NET_SYSTEM.send_stdout(logLines)

def sendErrorLogHandler(logLines):
    if not _NET_SYSTEM.isConnected():
        return False
    return _NET_SYSTEM.send_stderr(logLines)

_LOG_PROXY = logging.LoggingProxySystem() \
    .setLogHandler(sendLogHandler) \
    .setErrorLogHandler(sendErrorLogHandler)

_NET_SYSTEM.setConnectedHandler(lambda: _LOG_PROXY.updateCachedLogs())

def ADD_SYSTEM_REF():
    global _SYSTEM_REF
    _SYSTEM_REF += 1
    if _SYSTEM_REF == 1:
        _LOG_PROXY.startProxy()
        _NET_SYSTEM.start()
    return _SYSTEM_REF

def DELETE_SYSTEM_REF():
    global _SYSTEM_REF
    _SYSTEM_REF -= 1
    if _SYSTEM_REF == 0:
        _NET_SYSTEM.close()
        _LOG_PROXY.closeProxy()
    return _SYSTEM_REF

def clientExecCode(data):
    code = mCompile(str(data), "<string>", "exec")
    def _EXEC_CODE():
        mEval(code)
        print("[CLIENT_CODE] Executed successfully!")
    clientApi.GetEngineCompFactory().CreateGame(None).AddTimer(
        0, _EXEC_CODE
    )

def serverExecCode(data):
    code = mCompile(str(data), "<string>", "exec")
    def _EXEC_CODE():
        mEval(code)
        print("[SERVER_CODE] Executed successfully!")
    serverApi.GetEngineCompFactory().CreateGame(None).AddTimer(
        0, _EXEC_CODE
    )

_NET_SYSTEM.updateHandlers({
    network.TYPE_EXEC_CLIENT: clientExecCode,
    network.TYPE_EXEC_SERVER: serverExecCode
})

@Mod.Binding(name = "MCDevConsole", version = "1.0.0")
class MyMod(object):
    @Mod.InitServer()
    def serverInit(self):
        serverApi.GetEngineCompFactory()    # safe initialize
        ADD_SYSTEM_REF()

    @Mod.InitClient()
    def clientInit(self):
        clientApi.GetEngineCompFactory()
        ADD_SYSTEM_REF()

    @Mod.DestroyServer()
    def serverDestroy(self):
        DELETE_SYSTEM_REF()

    @Mod.DestroyClient()
    def clientDestroy(self):
        DELETE_SYSTEM_REF()