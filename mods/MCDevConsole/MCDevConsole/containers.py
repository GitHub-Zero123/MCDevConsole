# -*- coding: utf-8 -*-
""" 容器模块 - 提供带缓存上限的列表 """

class CachedLists(object):
    """ 带缓存上限的列表，超出上限时自动清理最老的元素 """
    
    def __init__(self, max_size=1000, cleanup_count=100):
        # type: (int, int) -> None
        """
        初始化缓存列表
        
        Args:
            max_size: 最大缓存上限
            cleanup_count: 超出上限时一次性清理的元素数量
        """
        self.max_size = max_size
        self.cleanup_count = cleanup_count
        self._data = []
    
    def append(self, item):
        """ 添加元素，超出上限时自动清理 """
        # type: (any) -> None
        self._data.append(item)
        if len(self._data) > self.max_size:
            del self._data[:self.cleanup_count]
    
    def extend(self, items):
        """ 批量添加元素，超出上限时自动清理 """
        # type: (list) -> None
        self._data.extend(items)
        if len(self._data) > self.max_size:
            del self._data[:self.cleanup_count]
    
    def __len__(self):
        """ 返回当前元素数量 """
        # type: () -> int
        return len(self._data)
    
    def __getitem__(self, index):
        """ 获取指定索引的元素 """
        return self._data[index]
    
    def __iter__(self):
        """ 迭代器 """
        return iter(self._data)
    
    def clear(self):
        """ 清空所有元素 """
        # type: () -> None
        self._data = []
    
    def get_all(self):
        """ 获取所有元素 """
        # type: () -> list
        return self._data[:]
