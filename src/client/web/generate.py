# -*- coding: utf-8 -*-
import os
import sys
from pathlib import Path
from base64 import b64encode

# 修复 Windows 下的 Unicode 输出问题
if sys.platform == 'win32':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

def GET_CURRENT_PATH() -> str:
    """ 获取脚本所在目录 """
    return os.path.dirname(os.path.abspath(__file__))

def FORMAT_PATH(path: str, startWith="/") -> str:
    """ 格式化路径 """
    formatPath = os.path.normpath(path).replace(os.sep, "/")
    if startWith and not formatPath.startswith(startWith):
        formatPath = startWith + formatPath
    return formatPath

class EmbedResGenerator:
    """ 嵌入资源生成器 """
    GEN_ALL = 0
    GEN_HEADER_ONLY = 1
    BASE64_TRANS = str.maketrans({'+': '', '-': '', '=': '', '/': '', })
    def __init__(
        self,
        resPath: str,
        cppOutPath: str,
        namespace: str = "EmbedRes",
        mapName: str = "resourceMap",
        lineMaxCount: int = 100,
        fileSystemMode: bool = False,
    ):
        self.genMode = -1
        """ 生成模式"""
        self.resPath = resPath
        self.namespace = namespace
        self.mapName = mapName
        self.lineMaxCount = lineMaxCount
        self.fileSystemMode = fileSystemMode
        pathObj = Path(cppOutPath)
        self.target = Path(pathObj.parent, pathObj.stem)
        suffix = pathObj.suffix
        if not suffix or suffix in (".cpp", ".cxx"):
            # 自动生成.cpp + .h
            self.genMode = EmbedResGenerator.GEN_ALL
        elif suffix in (".hpp", ".hxx"):
            # 显性修饰 hpp/hxx 后缀 仅生成.hpp(Header-Only)
            self.genMode = EmbedResGenerator.GEN_HEADER_ONLY
        else:
            raise ValueError(f"不支持的后缀名: '{suffix}'，请使用 .cpp/.cxx/.hpp/.hxx 后缀名")

    def fileSearch(self):
        """ 遍历目录，返回所有目标文件以及其映射名 """
        for root, _, files in os.walk(self.resPath):
            for file in files:
                filePath = os.path.join(root, file)
                yield (filePath, self.createMapName(filePath))

    def createMapName(self, filePath: str) -> str:
        """ 创建文件映射名 """
        return os.path.relpath(filePath, self.resPath)

    def parseFile(self, filePath: str, indent=4):
        """ 解析文件, 返回二进制数组 """
        with open(filePath, "rb") as f:
            data = f.read()
        hexBytes = [f"0x{b:02x}" for b in data]
        space = " " * indent
        lines = []
        for i in range(0, len(hexBytes), self.lineMaxCount):
            line = ", ".join(hexBytes[i:i+self.lineMaxCount])
            lines.append(line)
        return ("{\n" + ",\n".join("".join((space, "    ", line)) for line in lines) + "\n{}}}".format(space), len(hexBytes))

    def createVarName(self, tMapPath: str) -> str:
        """ 创建变量名 """
        return "".join((
            "f",
            hex(hash(tMapPath)).replace("-", "r"),
            "_",
            b64encode(tMapPath.encode()).decode().translate(EmbedResGenerator.BASE64_TRANS)
        ))
    
    def formatPath(self, path: str) -> str:
        if self.fileSystemMode:
            return FORMAT_PATH(path)
        return FORMAT_PATH(path, startWith="")

    def generate(self):
        """ 生成嵌入资源的代码 """
        self.target.parent.mkdir(parents=True, exist_ok=True)
        hLineTmpList = []
        varNameList = []    # type: list[tuple[str, str, int]]
        parsDataList = []   # type: list[tuple[str, int]]
        for filePath, mapName in self.fileSearch():
            tMapPath = self.formatPath(mapName)
            parsData, fileSize = self.parseFile(filePath)
            varName = self.createVarName(tMapPath)
            hLineTmp = self.createHeaderVarLineTmp(varName)
            if self.genMode == EmbedResGenerator.GEN_HEADER_ONLY:
                # 仅生成头文件下直接处理
                hLineTmp = hLineTmp.format(parsData)
            varNameList.append((varName, tMapPath, fileSize))
            if fileSize > 0:
                hLineTmpList.append(hLineTmp)
            parsDataList.append((parsData, fileSize))
        # 头文件生成
        if self.genMode == EmbedResGenerator.GEN_HEADER_ONLY:
            # 生成单头MAP
            headerMap = self.createHeaderMapTmp()
            mapContent = "\n".join(
                ("        {{ \"{}\", {{ {varName}, {svarName} }} }},".format(mapFileName, varName=varName if fileSize > 0 else "nullptr",
                    svarName="sizeof({})".format(varName) if fileSize > 0 else "0"
                ) for varName, mapFileName, fileSize in varNameList)
            )
            hLineTmpList.append(headerMap.format("{{\n{}\n    }}".format(mapContent)))
        else:
            hLineTmpList.append(self.createHeaderMapTmp())
        hFileTarget = self.target.with_suffix(".hpp" if self.genMode == EmbedResGenerator.GEN_HEADER_ONLY else ".h")
        with open(hFileTarget, "w", encoding="utf-8") as hFile:
            hFile.write(self.createHeaderContentTmp().replace(
                "{TARGETS}",
                "\n".join(hLineTmpList),
            ))
            print(f"生成头文件: {hFileTarget}")
        if self.genMode == EmbedResGenerator.GEN_HEADER_ONLY:
            return
        # 源代文件生成
        sLineTmpList = []
        for i in range(len(varNameList)):
            parsData, fileSize = parsDataList[i]
            varName, tMapPath, _ = varNameList[i]
            if fileSize > 0:
                sLineTmpList.append(self.createSourceVarLineTmp(varName).format(parsData))
        mapContent = "\n".join(
            ("        {{ \"{}\", {{ {varName}, {svarName} }} }},".format(mapFileName, varName=varName if fileSize > 0 else "nullptr", 
                svarName="sizeof({})".format(varName) if fileSize > 0 else "0"
            ) for varName, mapFileName, fileSize in varNameList)
        )
        sLineTmpList.append(self.createSourceMapTmp().format("{{\n{}\n    }}".format(mapContent)))
        sFileTarget = self.target.with_suffix(".cpp")
        with open(sFileTarget, "w", encoding="utf-8") as sFile:
            sFile.write(self.createSourceContentTmp().replace(
                "{TARGETS}",
                "\n".join(sLineTmpList),
            ))
        print(f"生成源文件: {sFileTarget}")

    def createSourceContentTmp(self):
        """ 创建源文件内容模板 """
        return """#include "{}"

// Generated by EmbedResGenerator
namespace {namespace}
{{
{{TARGETS}}
}} // namespace {namespace}
""".format(self.target.with_suffix(".hpp" if self.genMode == EmbedResGenerator.GEN_HEADER_ONLY else ".h").name, namespace=self.namespace)

    def createHeaderContentTmp(self):
        """ 创建头文件内容模板 """
        return """#pragma once
#include <unordered_map>
#include <string>

// Generated by EmbedResGenerator
namespace {namespace}
{{
{{TARGETS}}
}} // namespace {namespace}
""".format(namespace=self.namespace)

    def createSourceVarLineTmp(self, varName: str, indent: int = 4):
        """ 创建源文件变量行模板 """
        return " " * indent + "const unsigned char {}[] = {{}};".format(varName)

    def createSourceMapTmp(self, indent: int = 4):
        """ 创建源文件映射行模板 """
        return " " * indent + "const std::unordered_map<std::string, std::pair<const unsigned char*, size_t>> {} = {{}};".format(self.mapName)

    def createHeaderVarLineTmp(self, varName: str, indent: int = 4):
        """ 创建头文件变量行模板 """
        if self.genMode == EmbedResGenerator.GEN_HEADER_ONLY:
            return " " * indent + "inline const unsigned char {}[] = {{}};".format(varName)
        return " " * indent + "extern const unsigned char {}[];".format(varName)

    def createHeaderMapTmp(self, indent: int = 4):
        """ 创建头文件映射行模板 """
        if self.genMode == EmbedResGenerator.GEN_HEADER_ONLY:
            return " " * indent + "inline const std::unordered_map<std::string, std::pair<const unsigned char*, size_t>> {} = {{}};".format(self.mapName)
        return " " * indent + "extern const std::unordered_map<std::string, std::pair<const unsigned char*, size_t>> {};".format(self.mapName)

def generateCode():
    # 生成嵌入资源代码
    currentPath = GET_CURRENT_PATH()
    # 输出到 src/client/cpp/include/MCDevConsole/
    outDir = os.path.join(currentPath, "..", "cpp", "include", "MCDevConsole")
    outDir = os.path.abspath(outDir)
    if not os.path.exists(outDir):
        os.makedirs(outDir)
    
    # 从 dist 目录读取构建后的前端资源
    distPath = os.path.join(currentPath, "dist")
    if not os.path.exists(distPath):
        print(f"错误: dist 目录不存在: {distPath}")
        print("请先运行 'npm run build' 构建前端资源")
        return
    
    # 检查 dist 目录是否有文件
    hasFiles = False
    for root, dirs, files in os.walk(distPath):
        if files:
            hasFiles = True
            break
    
    if not hasFiles:
        print(f"错误: dist 目录为空: {distPath}")
        print("请先运行 'npm run build' 构建前端资源")
        return
    
    target = EmbedResGenerator(
        distPath,
        os.path.join(outDir, "WebResource.hpp"),
        namespace="MCDevConsole::WebResource",
        mapName="resourceMap",
        fileSystemMode=True,  # 启用文件系统模式，保留完整路径
    )
    target.generate()
    print("[OK] 前端资源已内联到 C++ 代码")

if __name__ == "__main__":
    generateCode()