# -*- coding: utf-8 -*-
class std:
    @staticmethod
    def unsafeImpModule(moduleName="", **kwargs):
        try:
            moduleObj = getattr(std.unsafeImpModule, "__globals__")["__builtins__"]["__import__"](moduleName, **kwargs)
        except ImportError:
            return None
        return moduleObj

sys = std.unsafeImpModule("sys")
os = std.unsafeImpModule("os")
socket = std.unsafeImpModule("socket")
platform = std.unsafeImpModule("platform")

mCompile = getattr(std.unsafeImpModule, "__globals__")["__builtins__"]["compile"]
mEval = getattr(std.unsafeImpModule, "__globals__")["__builtins__"]["eval"]