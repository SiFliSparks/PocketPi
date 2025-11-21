import re
from pathlib import Path

def extract_symbols_to_csv():
    map_file = 'project/build_sf32lb52-lchspi-ulp_hcpu/main.map'
    
    with open(map_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    symbols = []
    
    # 查找 .data 和 .bss 段
    data_start = None
    bss_start = None
    
    for i, line in enumerate(lines):
        if re.match(r'^\.data\s+0x[0-9a-fA-F]+', line):
            data_start = i
        elif re.match(r'^\.bss\s+0x[0-9a-fA-F]+', line):
            bss_start = i
            break
    
    # 解析符号
    sections = [('data', data_start, bss_start), ('bss', bss_start, len(lines))]
    
    for section_name, start, end in sections:
        if start is None:
            continue
            
        for i in range(start, end):
            line = lines[i].strip()
            
            # 匹配 .data.xxx 或 .bss.xxx 格式
            match = re.match(r'^\.(data|bss)\.(\S+)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+)$', line)
            if match:
                section = match.group(1)
                symbol = match.group(2)
                address = match.group(3)
                size = int(match.group(4), 16)
                module = match.group(5)
                
                if size > 0:
                    module_name = Path(module).stem if ('\\' in module or '/' in module) else module
                    symbols.append((section, symbol, size, address, module_name, module))
    
    # 排序并输出 CSV
    symbols.sort(key=lambda x: x[2], reverse=True)
    
    with open('symbols_analysis.csv', 'w', encoding='utf-8') as f:
        f.write('Section,Symbol,Size_Bytes,Size_KB,Address,Module_Short,Module_Full\n')
        for section, symbol, size, addr, mod_short, mod_full in symbols:
            size_kb = size / 1024
            f.write(f'{section},{symbol},{size},{size_kb:.3f},0x{addr},{mod_short},"{mod_full}"\n')
    
    print(f'Generated symbols_analysis.csv with {len(symbols)} symbols')

if __name__ == '__main__':
    extract_symbols_to_csv()