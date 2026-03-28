# -*- coding: utf-8 -*-
import mod.client.extraClientApi as clientApi
from .network import NETSystem

ClientSystem = clientApi.GetClientSystemCls()
playerId = clientApi.GetLocalPlayerId()
MAX_NOTIFY_LINES = 40
MAX_NOTIFY_LENGTH = 120

class LoggingSystem(ClientSystem):
    # 仅作移动端测试使用
    def __init__(self, namespace, systemName):
        ClientSystem.__init__(self, namespace, systemName)
        mcNamespace = clientApi.GetEngineNamespace()
        mcSystemName = clientApi.GetEngineSystemName()
        self._lastCacheSize = 0

        self.ListenForEvent(mcNamespace, mcSystemName, "ClientJumpButtonReleaseEvent", self, self.ClientJumpButtonReleaseEvent)

    def ClientJumpButtonReleaseEvent(self, _=None):
        cached = NETSystem.getInstance().loggingCached
        if not cached:
            self._showLeftCornerNotify(["[MCDevConsole] 暂无日志"])
            return

        self._lastCacheSize = len(cached)
        lines = []
        for msg in cached[-MAX_NOTIFY_LINES:]:
            text = self._normalizeText(msg)
            if text:
                lines.append(text)

        if not lines:
            self._showLeftCornerNotify(["[MCDevConsole] 暂无可显示日志"])
            return
        self._showLeftCornerNotify(lines)

    def _showLeftCornerNotify(self, lines):
        notifyComp = clientApi.GetEngineCompFactory().CreateTextNotifyClient(None)
        normalized = []
        for line in lines:
            uline = self._toUnicode(line)
            normalized.append(uline[:MAX_NOTIFY_LENGTH])
        text = u"\n".join(normalized)
        notifyComp.SetLeftCornerNotify(text.encode("utf-8"))

    def _normalizeText(self, value):
        # 先转 unicode 再处理，避免字节串处理问题
        text = self._toUnicode(value)
        text = text.replace(u"\r", u"").replace(u"\n", u"")
        return text.strip()

    def _toUnicode(self, value):
        if isinstance(value, unicode):
            return value
        if isinstance(value, str):
            try:
                return value.decode("utf-8")
            except Exception:
                return value.decode("utf-8", "ignore")
        return unicode(value)
            