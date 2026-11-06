#!/usr/bin/env python3
"""Build a single teaching handout while retaining links to lesson source assets."""
from pathlib import Path
import os,re
root=Path(__file__).resolve().parents[1]
sections=[
    ('授课安排','docs/DEMO_COURSE.md'),
    ('比赛规则与现有硬件','docs/COMPETITION.md'),
    ('硬件与手册梳理','docs/HARDWARE_OVERVIEW.md'),
    ('第1课：VOFA与IMU','src/demo/demo_vofa/README.md'),
    ('第1课完整通道说明','docs/VOFA.md'),
    ('第2课：遥控','src/demo/demo02_remote/README.md'),
    ('遥控协议','docs/RADIO_PROTOCOL.md'),
    ('遥控器程序与烧录','remote/tle100/README.md'),
    ('第3课：电机','src/demo/demo03_motor/README.md'),
    ('第4课：悬停逻辑','src/demo/demo04_hover_logic/README.md'),
    ('第5课：ToF','src/demo/demo05_tof/README.md'),
    ('第6课：光流','src/demo/demo06_flow/README.md'),
    ('光流与ToF接线','docs/MODULE_WIRING.md'),
    ('第7课：整机联调','src/demo/demo07_integration/README.md'),
    ('实验记录表','docs/LAB_RECORD.md'),
    ('验证与限制','docs/LESSON_VALIDATION.md'),
    ('当前板实测与限制','docs/BOARD_CHECK_2026-10-09.md'),
    ('资料来源','docs/references/README.md')]
parts=['# UAV-F22 队内教学总讲义\n\n七课文档集中在这一个文件。配套源码、CSV、手册、遥控HEX均在本文件所在的教学目录中。比赛工程位于旁边的`02_比赛工程`。\n\n操作时先进入本目录，再执行文中的命令：\n\n```bash\ncd "/home/czy/f22_hal_demos1/f22_hal_demos/01_教学demo"\n```\n\n本文件由`scripts/build_handbook.py`从各课文档生成；修改原文档后重新运行此脚本。\n']
for title,name in sections:
    source=root/name
    def relink(match):
        target=match.group(1).strip('<>')
        if '://' in target or target.startswith('#'):return match.group(0)
        path,sep,anchor=target.partition('#')
        result=os.path.relpath((source.parent/path).resolve(),root)
        if sep:result+='#'+anchor
        if ' ' in result:result='<'+result+'>'
        return ']('+result+')'
    text=re.sub(r'\]\(([^)]+)\)',relink,source.read_text())
    text=re.sub(r'^(#{1,5}) ',r'#\1 ',text,flags=re.M)
    parts.append('\n---\n\n## '+title+'\n\n'+text.strip()+'\n')
(root/'教学总讲义.md').write_text('\n'.join(parts))
print('已生成：教学总讲义.md，包含',len(sections),'份文档')
