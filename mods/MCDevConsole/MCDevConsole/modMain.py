# -*- coding: utf-8 -*-
from mod.common.mod import Mod
import mod.server.extraServerApi as serverApi
import mod.client.extraClientApi as clientApi
from . import network

_SYSTEM_REF = 0
_NET_SYSTEM = network.NETSystem()

def ADD_SYSTEM_REF():
    global _SYSTEM_REF
    _SYSTEM_REF += 1
    if _SYSTEM_REF == 1:
        _NET_SYSTEM.start()
    return _SYSTEM_REF

def DELETE_SYSTEM_REF():
    global _SYSTEM_REF
    _SYSTEM_REF -= 1
    if _SYSTEM_REF == 0:
        _NET_SYSTEM.close()
    return _SYSTEM_REF

def clientExecCode(data):
    code = compile(str(data), "<string>", "exec")
    def _EXEC_CODE():
        print("[CLIENT_CODE] Executed successfully: " + str(eval(code)))
    clientApi.GetEngineCompFactory().CreateGame(None).AddTimer(
        0, _EXEC_CODE
    )

def serverExecCode(data):
    code = compile(str(data), "<string>", "exec")
    def _EXEC_CODE():
        print("[SERVER_CODE] Executed successfully: " + str(eval(code)))
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