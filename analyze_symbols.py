#!/usr/bin/env python3
"""
分析 main.map 文件中的 .data 和 .bss 段符号，按大小排序显示
"""
import re
import argparse
from typing import List, Tuple, Dict
from pathlib import Path

class Symbol:
    def __init__(self, name: str, address: str, size: int, section: str, module: str):
        self.name = name
        self.address = address
        self.size = size
        self.section = section
        self.module = module
        
    def __repr__(self):
        return f"Symbol(name='{self.name}', size={self.size}, section='{self.section}')"

def parse_map_file(map_file_path: str) -> List[Symbol]:
    """解析 map 文件，提取 .data 和 .bss 段的符号信息"""
    symbols = []
    
    with open(map_file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    # 查找 .data 和 .bss 段的开始位置
    data_start = None
    bss_start = None
    bss_end = None
    
    for i, line in enumerate(lines):
        if re.match(r'^\.data\s+0x[0-9a-fA-F]+', line):
            data_start = i
        elif re.match(r'^\.bss\s+0x[0-9a-fA-F]+', line):
            bss_start = i
        elif bss_start and re.match(r'^\s*0x[0-9a-fA-F]+\s+__bss_end__', line):
            bss_end = i
            break
    
    if data_start is None or bss_start is None:
        print("未找到 .data 或 .bss 段")
        return symbols
    
    # 解析 .data 段
    print(f"解析 .data 段 (从第 {data_start} 行到第 {bss_start} 行)...")
    symbols.extend(parse_section(lines, data_start, bss_start, '.data'))
    
    # 解析 .bss 段
    print(f"解析 .bss 段 (从第 {bss_start} 行到第 {bss_end} 行)...")
    if bss_end:
        symbols.extend(parse_section(lines, bss_start, bss_end, '.bss'))
    else:
        symbols.extend(parse_section(lines, bss_start, len(lines), '.bss'))
    
    return symbols

def parse_section(lines: List[str], start: int, end: int, section_name: str) -> List[Symbol]:
    """解析指定段的符号"""
    symbols = []
    
    for i in range(start, end):
        line = lines[i].strip()
        
        # 匹配符号行的模式: .data.symbol_name 0x地址 大小 模块路径
        match = re.match(r'^\.(data|bss)\.(\S+)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+)$', line)
        if match:
            section = match.group(1)
            symbol_name = match.group(2)
            address = match.group(3)
            size_hex = match.group(4)
            module = match.group(5)
            
            size = int(size_hex, 16)
            
            # 过滤掉大小为0的符号
            if size > 0:
                symbols.append(Symbol(symbol_name, address, size, section, module))
        
        # 匹配另一种格式: 符号名在地址后面
        match2 = re.match(r'^\s*0x([0-9a-fA-F]+)\s+([A-Za-z_]\S*)', line)
        if match2 and i > start:  # 确保不是段头
            # 查看前一行获取大小信息
            prev_line = lines[i-1].strip() if i > 0 else ""
            size_match = re.search(r'0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+)$', prev_line)
            if size_match:
                address = match2.group(1)
                symbol_name = match2.group(2)
                size = int(size_match.group(2), 16)
                module = size_match.group(3)
                
                if size > 0:
                    symbols.append(Symbol(symbol_name, address, size, section_name.replace('.', ''), module))
    
    return symbols

def analyze_symbols(symbols: List[Symbol]) -> Dict:
    """分析符号统计信息"""
    data_symbols = [s for s in symbols if s.section == 'data']
    bss_symbols = [s for s in symbols if s.section == 'bss']
    
    data_total_size = sum(s.size for s in data_symbols)
    bss_total_size = sum(s.size for s in bss_symbols)
    
    # 按模块统计
    module_stats = {}
    for symbol in symbols:
        # 简化模块路径，只保留文件名
        module_name = Path(symbol.module).stem if '\\' in symbol.module or '/' in symbol.module else symbol.module
        if module_name not in module_stats:
            module_stats[module_name] = {'data': 0, 'bss': 0, 'total': 0}
        
        module_stats[module_name][symbol.section] += symbol.size
        module_stats[module_name]['total'] += symbol.size
    
    return {
        'data_symbols': data_symbols,
        'bss_symbols': bss_symbols,
        'data_total': data_total_size,
        'bss_total': bss_total_size,
        'module_stats': module_stats
    }

def print_symbols_by_size(symbols: List[Symbol], section_name: str, top_n: int = 50):
    """按大小排序打印符号"""
    sorted_symbols = sorted(symbols, key=lambda x: x.size, reverse=True)
    
    print(f"\n{'='*80}")
    print(f"{section_name} 段符号 (按大小排序，前 {min(top_n, len(sorted_symbols))} 个)")
    print(f"{'='*80}")
    print(f"{'序号':<4} {'符号名':<40} {'大小(字节)':<12} {'大小(KB)':<10} {'模块'}")
    print(f"{'-'*80}")
    
    for i, symbol in enumerate(sorted_symbols[:top_n]):
        module_name = Path(symbol.module).stem if '\\' in symbol.module or '/' in symbol.module else symbol.module
        size_kb = symbol.size / 1024
        print(f"{i+1:<4} {symbol.name:<40} {symbol.size:<12} {size_kb:<10.2f} {module_name}")

def print_module_stats(module_stats: Dict, top_n: int = 20):
    """按模块统计打印"""
    sorted_modules = sorted(module_stats.items(), key=lambda x: x[1]['total'], reverse=True)
    
    print(f"\n{'='*80}")
    print(f"模块统计 (按总大小排序，前 {min(top_n, len(sorted_modules))} 个)")
    print(f"{'='*80}")
    print(f"{'序号':<4} {'模块名':<35} {'DATA(字节)':<12} {'BSS(字节)':<12} {'总计(字节)':<12} {'总计(KB)'}")
    print(f"{'-'*80}")
    
    for i, (module, stats) in enumerate(sorted_modules[:top_n]):
        total_kb = stats['total'] / 1024
        print(f"{i+1:<4} {module:<35} {stats['data']:<12} {stats['bss']:<12} {stats['total']:<12} {total_kb:.2f}")

def main():
    parser = argparse.ArgumentParser(description='分析 main.map 文件中的 .data 和 .bss 段符号')
    parser.add_argument('map_file', help='map 文件路径')
    parser.add_argument('--top-symbols', '-s', type=int, default=50, help='显示前N个最大的符号 (默认: 50)')
    parser.add_argument('--top-modules', '-m', type=int, default=20, help='显示前N个最大的模块 (默认: 20)')
    
    args = parser.parse_args()
    
    if not Path(args.map_file).exists():
        print(f"错误: 文件 '{args.map_file}' 不存在")
        return 1
    
    print(f"正在分析 map 文件: {args.map_file}")
    
    # 解析符号
    symbols = parse_map_file(args.map_file)
    
    if not symbols:
        print("未找到任何符号")
        return 1
    
    # 分析统计
    stats = analyze_symbols(symbols)
    
    # 打印总体统计
    print(f"\n{'='*80}")
    print(f"总体统计")
    print(f"{'='*80}")
    print(f"DATA 段总大小: {stats['data_total']} 字节 ({stats['data_total']/1024:.2f} KB)")
    print(f"BSS 段总大小:  {stats['bss_total']} 字节 ({stats['bss_total']/1024:.2f} KB)")
    print(f"总计:         {stats['data_total'] + stats['bss_total']} 字节 ({(stats['data_total'] + stats['bss_total'])/1024:.2f} KB)")
    print(f"DATA 段符号数: {len(stats['data_symbols'])}")
    print(f"BSS 段符号数:  {len(stats['bss_symbols'])}")
    
    # 打印符号详情
    print_symbols_by_size(stats['data_symbols'], 'DATA', args.top_symbols)
    print_symbols_by_size(stats['bss_symbols'], 'BSS', args.top_symbols)
    
    # 打印模块统计
    print_module_stats(stats['module_stats'], args.top_modules)
    
    return 0

if __name__ == '__main__':
    exit(main())