"""
amalgamate.py — 将拆分后的 xoaop.h 子模块合并为单个头文件

用法:
    python amalgamate.py [--output OUTPUT_FILE]

默认输出: build/xoaop.h
"""

import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# 子模块列表（按依赖顺序）
SUB_MODULES = [
    "xoaop_defer.h",
    "xoaop_pair.h",
    "xoaop_option.h",
    "xoaop_result.h",
    "xoaop_string.h",
    "xoaop_hash.h",
    "xoaop_hashmap.h",
    "xoaop_hashset.h",
    "xoaop_intern.h",
]


def read_sub_module(filename):
    """读取子模块文件，去掉 include guard 和依赖检查"""
    filepath = os.path.join(SCRIPT_DIR, filename)
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()

    result = []
    in_guard = False
    in_error_check = False
    in_block_comment = False
    header_done = False
    i = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # 跳过文件开头的 /* */ 注释块
        if not header_done:
            if stripped.startswith("/*"):
                in_block_comment = True
                i += 1
                continue
            if in_block_comment:
                if "*/" in stripped:
                    in_block_comment = False
                i += 1
                continue
            if stripped == "" or stripped.startswith("//"):
                i += 1
                continue
            # 第一个非注释非空行 → 头注释结束
            header_done = True
            in_block_comment = False

        # 处理 #ifndef / #define guard
        if stripped.startswith("#ifndef XOAOP_") and "_H" in stripped:
            in_guard = True
            i += 1
            continue
        if in_guard and stripped.startswith("#define XOAOP_"):
            in_guard = False
            i += 1
            continue

        # 处理 #if !defined(XOAOP_H) ... #error ... #endif
        if stripped == '#if !defined(XOAOP_H)':
            in_error_check = True
            i += 1
            continue
        if in_error_check:
            if stripped.startswith("#endif"):
                in_error_check = False
            i += 1
            continue

        # 跳过尾部的 #endif // XOAOP_XXX_H
        if re.match(r"^#endif\s*//\s*XOAOP_\w+_H", stripped):
            i += 1
            continue

        # 跳过子模块内部的 #include "xoaop.h"
        if re.match(r'^#include\s+"xoaop\.h"', stripped):
            i += 1
            continue

        result.append(line)
        i += 1

    return "".join(result)


def amalgamate(output_dir, output_name):
    """合并所有子模块到输出文件"""
    base_file = os.path.join(SCRIPT_DIR, "xoaop.h")

    with open(base_file, "r", encoding="utf-8") as f:
        base_lines = f.readlines()

    output_lines = []
    for line in base_lines:
        stripped = line.strip()

        # 检查是否是子模块 include
        matched = re.match(r'^#include\s+"(xoaop_\w+\.h)"', stripped)
        if matched:
            module_name = matched.group(1)
            if module_name in SUB_MODULES:
                output_lines.append(f"// === begin: {module_name} ===\n")
                module_content = read_sub_module(module_name)
                output_lines.append(module_content)
                output_lines.append(f"// === end: {module_name} ===\n")
            else:
                output_lines.append(line)
        else:
            output_lines.append(line)

    output_dir_path = os.path.join(SCRIPT_DIR, output_dir)
    os.makedirs(output_dir_path, exist_ok=True)
    output_path = os.path.join(output_dir_path, output_name)
    with open(output_path, "w", encoding="utf-8") as f:
        f.writelines(output_lines)

    print(f"Amalgamated header written to: {output_path}")
    print(f"Total lines: {len(output_lines)}")


def main():
    output_dir = "build"
    output_name = "xoaop.h"
    if len(sys.argv) > 2 and sys.argv[1] == "--output":
        output_name = sys.argv[2]
    amalgamate(output_dir, output_name)


if __name__ == "__main__":
    main()
