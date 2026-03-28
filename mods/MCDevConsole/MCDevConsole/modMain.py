# -*- coding: utf-8 -*-
from mod.common.mod import Mod
import mod.server.extraServerApi as serverApi
import mod.client.extraClientApi as clientApi
from . import network
from . import logging
from .ub import mCompile, mEval

_SYSTEM_REF = 0
_NET_SYSTEM = network.NETSystem.getInstance()
_ENABLE_LOG_TEST = True

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

def TRY_CALL(func):
    try:
        func()
    except Exception:
        import traceback
        traceback.print_exc()

def ADD_SYSTEM_REF():
    global _SYSTEM_REF
    _SYSTEM_REF += 1
    if _SYSTEM_REF == 1:
        TRY_CALL(lambda: _LOG_PROXY.startProxy())
        TRY_CALL(lambda: _NET_SYSTEM.start())
    return _SYSTEM_REF

def DELETE_SYSTEM_REF():
    global _SYSTEM_REF
    _SYSTEM_REF -= 1
    if _SYSTEM_REF == 0:
        TRY_CALL(lambda: _NET_SYSTEM.close())
        TRY_CALL(lambda: _LOG_PROXY.closeProxy())
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
class MCDevConsole(object):
    @Mod.InitServer()
    def serverInit(self):
        serverApi.GetEngineCompFactory()    # safe initialize
        ADD_SYSTEM_REF()

    @Mod.InitClient()
    def clientInit(self):
        clientApi.GetEngineCompFactory()
        ADD_SYSTEM_REF()
        if not _ENABLE_LOG_TEST:
            return

        from .loggingSystem import LoggingSystem
        clientApi.RegisterSystem(
            self.__class__.__name__,
            LoggingSystem.__name__, 
            LoggingSystem.__module__ + "." + LoggingSystem.__name__
        )

    @Mod.DestroyServer()
    def serverDestroy(self):
        DELETE_SYSTEM_REF()

    @Mod.DestroyClient()
    def clientDestroy(self):
        DELETE_SYSTEM_REF()