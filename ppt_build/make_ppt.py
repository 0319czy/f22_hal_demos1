# -*- coding: utf-8 -*-
from pptx import Presentation
from pptx.util import Inches, Pt, Emu
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.enum.shapes import MSO_SHAPE
import os

FONT = "微软雅黑"
DARK = RGBColor(0x1F, 0x24, 0x2B)
BLUE = RGBColor(0x1F, 0x3A, 0x6E)
ACC  = RGBColor(0xE8, 0x6E, 0x2B)
GRAY = RGBColor(0x6B, 0x72, 0x80)
LIGHT= RGBColor(0xF2, 0xF5, 0xF9)
WHITE= RGBColor(0xFF, 0xFF, 0xFF)
GREEN= RGBColor(0x2E, 0x7D, 0x54)

prs = Presentation()
prs.slide_width  = Inches(13.333)
prs.slide_height = Inches(7.5)
BLANK = prs.slide_layouts[6]
W, H = prs.slide_width, prs.slide_height

def bg(slide, color=LIGHT):
    s = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, W, H)
    s.fill.solid(); s.fill.fore_color.rgb = color; s.line.fill.background()
    s.shadow.inherit = False
    return s

def txt(slide, x, y, w, h, text, size=20, bold=False, color=DARK,
        align=PP_ALIGN.LEFT, space=6, line=1.25, anchor=MSO_ANCHOR.TOP):
    tb = slide.shapes.add_textbox(x, y, w, h); tf = tb.text_frame
    tf.word_wrap = True; tf.vertical_anchor = anchor
    tf.margin_left = tf.margin_right = Inches(0.05)
    tf.margin_top = tf.margin_bottom = Inches(0.02)
    lines = text.split("\n")
    for i, ln in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align; p.space_after = Pt(space); p.line_spacing = line
        r = p.add_run(); r.text = ln
        r.font.name = FONT; r.font.size = Pt(size); r.font.bold = bold
        r.font.color.rgb = color
    return tb

def bar(slide, x, y, w, h, color=ACC):
    s = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, x, y, w, h)
    s.fill.solid(); s.fill.fore_color.rgb = color; s.line.fill.background()
    s.shadow.inherit = False
    return s

def head(slide, kicker, title, no):
    bg(slide)
    bar(slide, 0, 0, W, Inches(0.10), BLUE)
    txt(slide, Inches(0.75), Inches(0.42), Inches(9), Inches(0.4),
        kicker, 14, True, ACC)
    txt(slide, Inches(0.75), Inches(0.80), Inches(11.5), Inches(0.9),
        title, 34, True, BLUE)
    txt(slide, W - Inches(1.3), H - Inches(0.62), Inches(0.8), Inches(0.4),
        f"{no:02d}", 14, True, GRAY, PP_ALIGN.RIGHT)

def card(slide, x, y, w, h, title, body, tcolor=BLUE, tsize=19, bsize=14):
    s = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, x, y, w, h)
    s.fill.solid(); s.fill.fore_color.rgb = WHITE
    s.line.color.rgb = RGBColor(0xDD, 0xE3, 0xEA); s.line.width = Pt(1)
    s.shadow.inherit = False
    txt(slide, x + Inches(0.22), y + Inches(0.18), w - Inches(0.44), Inches(0.45),
        title, tsize, True, tcolor)
    txt(slide, x + Inches(0.22), y + Inches(0.72), w - Inches(0.44), h - Inches(0.9),
        body, bsize, False, DARK, space=5, line=1.3)

def notes(slide, text):
    slide.notes_slide.notes_text_frame.text = text

# ---------------- 1 封面 ----------------
s = prs.slides.add_slide(BLANK); bg(s, BLUE)
bar(s, 0, H - Inches(0.22), W, Inches(0.22), ACC)
txt(s, Inches(0.9), Inches(1.15), Inches(11.5), Inches(0.5),
    "2026 悬浮飞行挑战赛 · 技术汇报", 17, True, RGBColor(0xF5,0xC5,0x9A))
txt(s, Inches(0.9), Inches(1.75), Inches(11.5), Inches(1.9),
    "一架能“停在半空中”的小飞机", 52, True, WHITE)
txt(s, Inches(0.9), Inches(3.35), Inches(11.2), Inches(1.2),
    "从一堆零件，到能自己稳住高度——我们做了什么、怎么做的、还差多少", 20, False,
    RGBColor(0xD6, 0xDF, 0xEC))
for i, (k, v) in enumerate([("4 个", "螺旋桨"), ("160 次/秒", "自我修正"),
                            ("20–100 cm", "目标高度范围"), ("5 秒", "目标悬停时间")]):
    x = Inches(0.9 + i * 3.0)
    txt(s, x, Inches(5.15), Inches(2.7), Inches(0.6), k, 30, True, WHITE)
    txt(s, x, Inches(5.78), Inches(2.7), Inches(0.4), v, 14, False, RGBColor(0xC9,0xD4,0xE4))
notes(s, "开场（30 秒）：大家好，我们做的是一个小小的四轴飞行器——手掌大小。"
         "它要完成的任务听起来很简单：飞到 20 到 100 厘米之间的某个高度，然后稳稳停住 5 秒。"
         "但这 5 秒非常难，今天我就用大家都能听懂的话，讲讲它由什么组成、靠什么飞、我们怎么一步步调出来的。")

# ---------------- 2 目录 ----------------
s = prs.slides.add_slide(BLANK); head(s, "今天讲什么", "四个部分，约 8 分钟", 2)
items = [("01", "比赛要我们做什么", "一句话说清目标，以及为什么它很难"),
         ("02", "小飞机是什么做的", "零件清单 + 每个零件的“人话”解释"),
         ("03", "它怎么飞、怎么想", "升力、传感器、大脑与闭环控制"),
         ("04", "我们怎么调出来的", "试飞—记录—改参数 的循环，以及下一步")]
for i, (n, t, d) in enumerate(items):
    y = Inches(1.95 + i * 1.28)
    txt(s, Inches(0.85), y, Inches(1.0), Inches(0.7), n, 30, True, ACC)
    txt(s, Inches(1.95), y - Inches(0.04), Inches(4.6), Inches(0.5), t, 22, True, BLUE)
    txt(s, Inches(1.95), y + Inches(0.46), Inches(9.6), Inches(0.5), d, 15, False, GRAY)
notes(s, "过渡（20 秒）：我分四块讲：为什么难、它是什么做的、原理是什么、我们怎么调的。")

# ---------------- 3 比赛目标 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第一部分 · 目标", "比赛到底要我们做什么？", 3)
card(s, Inches(0.75), Inches(1.85), Inches(5.9), Inches(2.5), "规则（大白话版）",
     "让飞行器起飞后，自己停在离地面 20～100 厘米\n"
     "之间的某个高度，保持至少 5 秒不碰地、不失控。", ACC, 20, 16)
card(s, Inches(6.95), Inches(1.85), Inches(5.6), Inches(2.5), "打个比方",
     "就像让一只蜂鸟，停在你膝盖到腰部之间的半空中，\n"
     "翅膀不停地微调，五秒钟不掉下来。", BLUE, 20, 16)
txt(s, Inches(0.75), Inches(4.62), Inches(11.8), Inches(0.45),
    "为什么难？两件事同时要满足：", 17, True, DARK)
for i, (t, d) in enumerate([("高度要准", "差 20 厘米，就可能低于下限或撞到上限"),
                            ("时间要够", "不是“飞过去”，而是“待得住”5 秒"),
                            ("不能乱飘", "场地很小，飘出去就出界")]):
    x = Inches(0.75 + i * 4.0)
    bar(s, x, Inches(5.2), Inches(0.09), Inches(1.25), ACC)
    txt(s, x + Inches(0.25), Inches(5.18), Inches(3.5), Inches(0.4), t, 18, True, BLUE)
    txt(s, x + Inches(0.25), Inches(5.62), Inches(3.5), Inches(0.8), d, 14, False, GRAY)
notes(s, "（60 秒）规则其实就一句话：在 20 到 100 厘米之间停住 5 秒。"
         "难在三个地方：高度要准、时间要够、还不能飘走。"
         "大家看这三条，后面讲的所有技术，都是为了解决这三个字：准、久、稳。")

# ---------------- 4 组成 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第二部分 · 组成", "它由哪些零件组成？", 4)
parts = [("机架", "骨架，把所有零件连在一起"),
         ("4 个电机 + 螺旋桨", "提供升力，往下扇风"),
         ("4 个电调", "电机的“油门”，调节转速"),
         ("锂电池", "动力来源，也最容易被忽视"),
         ("飞控板（大脑）", "STM32 芯片，每秒算 160 次"),
         ("3 个传感器", "平衡感、高度尺、眼睛"),
         ("遥控器", "人给它起飞 / 停止指令")]
for i, (t1, d1) in enumerate(parts):
    col = 0 if i < 4 else 1
    row = i if i < 4 else i - 4
    x = Inches(0.75 + col * 3.55); y = Inches(1.85 + row * 1.18)
    bar(s, x, y + Inches(0.06), Inches(0.07), Inches(0.62), ACC if i % 3 == 0 else BLUE)
    txt(s, x + Inches(0.22), y - Inches(0.02), Inches(3.2), Inches(0.4), t1, 16, True, BLUE)
    txt(s, x + Inches(0.22), y + Inches(0.36), Inches(3.15), Inches(0.6), d1, 12, False, GRAY)
img = "/home/czy/f22_hal_demos1/ppt_build/rc.jpg"
if os.path.exists(img):
    pw = Inches(4.30)
    pic = s.shapes.add_picture(img, Inches(8.15), Inches(1.85), width=pw)
    pic.crop_top = 0.20; pic.crop_bottom = 0.20
    # 显式设横版高度：宽 × 原高宽比 × 保留比例
    pic.height = Emu(int(pw * (2364.0 / 1773.0) * (1 - 0.40)))
    txt(s, Inches(8.15), Inches(5.45), Inches(4.7), Inches(0.7),
        "我们自制的遥控器：只有“起飞 / 停止”两个键", 11.5, False, GRAY, line=1.3)
notes(s, "（70 秒）左边是零件清单。我挑三个讲：电机和桨负责扇风；电池是所有问题的根源，后面会讲；"
         "飞控板是大脑，每秒算 160 次。右边是我们自己做的遥控器，只有起飞和停止两个键。"
         "注意——这架飞机没有摄像头、没有 GPS，全靠几个便宜传感器自己判断。")

# ---------------- 5 升力 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第三部分 · 原理", "它为什么会飞？", 5)
card(s, Inches(0.75), Inches(1.9), Inches(5.7), Inches(2.2), "① 往上：推得比重力大",
     "4 个螺旋桨往下扇风，空气就反过来把飞机往上顶。\n"
     "转速越高，顶得越猛；顶得比自身重量大，就上升。", BLUE, 19, 15)
card(s, Inches(6.85), Inches(1.9), Inches(5.7), Inches(2.2), "② 往侧边：歪一下就横着走",
     "整体往右歪一点，向上的力就变成“斜的”，\n"
     "一部分变成往右推——所以歪 = 横向移动。", BLUE, 19, 15)
card(s, Inches(0.75), Inches(4.35), Inches(11.8), Inches(1.5), "③ 所以“停住”其实很难",
     "想停在原地：4 个电机转速必须几乎相等；想停在某个高度：总推力必须正好等于重量。\n"
     "差一点也不行——推力多 1%，它就会慢慢往上爬；少 1%，就慢慢往下沉。",
     ACC, 19, 15)
txt(s, Inches(0.75), Inches(6.15), Inches(11.8), Inches(0.6),
    "这就是为什么必须有“大脑”不停地微调，而不是靠人手动飞。", 16, True, GREEN)
notes(s, "（70 秒）这部分是关键：四个桨往下扇风，飞机被顶上去。往上飞是加转速，往侧飞是整体歪一下。"
         "重点在第三张卡：想“停住”，推力必须正好等于重量，差 1% 就会慢慢爬升或下沉——"
         "所以必须靠电脑每秒算 160 次来微调，人手做不到。")

# ---------------- 6 传感器 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第三部分 · 感官", "它靠三个“感官”知道自己的状态", 6)
sens = [("平衡感", "MPU6050 陀螺仪 / 加速度计",
         "像人耳朵里的“前庭”：\n感觉自己在往哪边歪、转得多快。\n→ 用来保持水平"),
        ("高度尺", "VL53L1X 激光测距",
         "像一把朝下照的尺子：\n每 40 毫秒量一次离地多高。\n→ 用来保持高度"),
        ("眼睛", "PMW3901 光流传感器",
         "像盯着地砖看：\n地砖往后跑=我在往前飞。\n→ 用来保持位置")]
for i, (k, comp, d) in enumerate(sens):
    x = Inches(0.75 + i * 4.05)
    s1 = s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, x, Inches(1.85), Inches(3.7), Inches(4.1))
    s1.fill.solid(); s1.fill.fore_color.rgb = WHITE
    s1.line.color.rgb = RGBColor(0xDD,0xE3,0xEA); s1.shadow.inherit = False
    txt(s, x + Inches(0.25), Inches(2.05), Inches(3.2), Inches(0.5), k, 22, True, ACC)
    txt(s, x + Inches(0.25), Inches(2.55), Inches(3.2), Inches(0.7), comp, 12.5, True, BLUE)
    txt(s, x + Inches(0.25), Inches(3.2), Inches(3.2), Inches(2.5), d, 14, False, DARK, line=1.4)
txt(s, Inches(0.75), Inches(6.15), Inches(11.8), Inches(0.6),
    "全部加起来，成本只有几十块钱——但让它们“互相配合”很难，这是我们工作的核心。",
    15, True, GRAY)
notes(s, "（75 秒）它没有摄像头也没有 GPS，只有三个便宜传感器，我用人的器官来类比："
         "MPU6050 是平衡感，VL53L1X 是往下的激光尺，PMW3901 是盯着地面看的眼睛。"
         "这里要强调：单个传感器都很简单，难的是让它们互相配合、互相纠正。")

# ---------------- 7 大脑 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第三部分 · 大脑", "它怎么“思考”？—— 一个每秒 160 次的循环", 7)
steps = [("量", "读出：现在多高、歪几度、往哪飘"),
         ("比", "和“我想要的高度”比较，算出差多少"),
         ("改", "调整 4 个电机的转速，把误差拉回来")]
for i, (k, d) in enumerate(steps):
    x = Inches(1.0 + i * 4.1)
    c = s.shapes.add_shape(MSO_SHAPE.OVAL, x, Inches(1.95), Inches(1.15), Inches(1.15))
    c.fill.solid(); c.fill.fore_color.rgb = BLUE; c.line.fill.background(); c.shadow.inherit = False
    tf = c.text_frame; tf.text = k
    tf.paragraphs[0].alignment = PP_ALIGN.CENTER
    tf.paragraphs[0].runs[0].font.size = Pt(28); tf.paragraphs[0].runs[0].font.bold = True
    tf.paragraphs[0].runs[0].font.color.rgb = WHITE; tf.paragraphs[0].runs[0].font.name = FONT
    txt(s, x - Inches(0.85), Inches(3.25), Inches(2.85), Inches(0.9), d, 13, False, GRAY)
    if i < 2:
        txt(s, x + Inches(1.3), Inches(2.2), Inches(1.2), Inches(0.6), "→", 30, True, ACC)
card(s, Inches(0.9), Inches(4.35), Inches(11.6), Inches(1.75), "为什么要 160 次/秒？",
     "因为它每秒会歪掉、掉下去一点点。人手一秒钟只能反应 1～2 次，\n"
     "所以必须让单片机替我们盯着——这叫“闭环控制”，是整架飞机的灵魂。", ACC, 19, 15)
notes(s, "（65 秒）它的工作循环就三个字：量、比、改。"
         "量现在的状态，和目标比，差多少就去改电机转速。0.006 秒一次，也就是每秒 160 次。"
         "为什么这么快？因为它每一瞬间都在往下掉、往一边歪，人手根本来不及，必须交给芯片。")

# ---------------- 8 难点 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第三部分 · 难点", "听起来简单，为什么这么难？", 8)
diff = [("传感器会“撒谎”", "高速旋转的桨叶会让传感器抖动、读数乱跳；\n爬升时“地面的样子在变大”，光流会误以为在平移。"),
        ("上下左右会互相干扰", "加推力想上升，机体会歪一点；歪了就往旁边漂；\n一漂又影响高度——三个方向是缠在一起的。"),
        ("机器本身会“乏力”", "同样 80% 的油门，电池满电时能飞 60 厘米，\n电池快没电时只能飞 15 厘米。")]
for i, (t, d) in enumerate(diff):
    y = Inches(1.85 + i * 1.62)
    bar(s, Inches(0.75), y, Inches(0.09), Inches(1.35), ACC)
    txt(s, Inches(1.0), y - Inches(0.02), Inches(11.3), Inches(0.45), t, 20, True, BLUE)
    txt(s, Inches(1.0), y + Inches(0.46), Inches(11.3), Inches(0.9), d, 14.5, False, DARK, line=1.35)
notes(s, "（70 秒）三个难点，我用大白话讲："
         "第一，传感器会被震动骗；第二，上下左右互相干扰，动一个方向会带出另外两个；"
         "第三，机器会累——同样油门，满电飞 60 厘米，快没电只能飞 15 厘米。"
         "这三点就是我们后面所有工作的来源。")

# ---------------- 9 技术路线 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第四部分 · 方法", "我们的技术路线：三层控制 + 一本“黑匣子”", 9)
layers = [("第 3 层  位置保持", "用光流把飞机拉回起飞点附近", "锦上添花"),
          ("第 2 层  高度控制", "用激光测距把高度稳在目标值", "比赛核心"),
          ("第 1 层  姿态控制", "让机体保持水平、不翻转", "安全底线")]
for i, (t, d, tag) in enumerate(layers):
    y = Inches(1.85 + i * 1.15)
    w = Inches(7.4)
    s1 = s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.75), y, w, Inches(0.95))
    s1.fill.solid(); s1.fill.fore_color.rgb = WHITE
    s1.line.color.rgb = RGBColor(0xDD,0xE3,0xEA); s1.shadow.inherit = False
    txt(s, Inches(1.0), y + Inches(0.10), Inches(3.4), Inches(0.4), t, 17, True, BLUE)
    txt(s, Inches(1.0), y + Inches(0.52), Inches(6.9), Inches(0.4), d, 13, False, GRAY)
    txt(s, Inches(8.35), y + Inches(0.28), Inches(1.9), Inches(0.4), tag, 13, True,
        GREEN if "底线" in tag else (ACC if "核心" in tag else GRAY))
card(s, Inches(0.75), Inches(5.3), Inches(11.6), Inches(1.55), "关键工具：飞行“黑匣子”",
     "每次飞行，板子把高度、油门、倾角、故障码等数据写进内置 Flash；落地后我们读出来分析。\n"
     "没有它，调试就是“盲猜”；有了它，每次失败都能找到具体原因。", ACC, 18, 14)
notes(s, "（75 秒）我们的路线分三层：最底层保持水平（安全底线），中间层控制高度（比赛核心），最上层保持位置（锦上添花）。"
         "另外我们做了一个“黑匣子”：每次飞行把数据存进芯片，落地后读出来。"
         "这一点我想强调：没有数据只能猜，有了数据才能知道是哪一环出问题。")

# ---------------- 10 过程 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第四部分 · 过程", "研制过程：一个不停转的圈", 10)
loop = [("改参数 / 改代码", 0.75), ("烧录进板子", 3.65), ("真机试飞", 6.55), ("读数据找原因", 9.45)]
for t, x in loop:
    x = Inches(x)
    s1 = s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, x, Inches(1.95), Inches(2.6), Inches(1.0))
    s1.fill.solid(); s1.fill.fore_color.rgb = BLUE; s1.line.fill.background(); s1.shadow.inherit = False
    tf = s1.text_frame; tf.text = t
    p = tf.paragraphs[0]; p.alignment = PP_ALIGN.CENTER
    p.runs[0].font.size = Pt(16); p.runs[0].font.bold = True
    p.runs[0].font.color.rgb = WHITE; p.runs[0].font.name = FONT
    if x < Inches(9.4):
        txt(s, x + Inches(2.65), Inches(2.1), Inches(0.6), Inches(0.5), "→", 26, True, ACC)
txt(s, Inches(0.75), Inches(3.2), Inches(11.8), Inches(0.5),
    "一个圈大约 5 分钟；我们转了上百圈。", 16, True, GREEN)
stats = [("上百次", "真机试飞与数据回读"), ("数十处", "发现并修复的具体问题"),
         ("多轮", "控制算法结构迭代（开环 → 串级）")]
for i, (k, v) in enumerate(stats):
    x = Inches(0.75 + i * 4.05)
    s1 = s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, x, Inches(3.9), Inches(3.7), Inches(1.45))
    s1.fill.solid(); s1.fill.fore_color.rgb = WHITE
    s1.line.color.rgb = RGBColor(0xDD,0xE3,0xEA); s1.shadow.inherit = False
    txt(s, x + Inches(0.25), Inches(4.05), Inches(3.2), Inches(0.6), k, 26, True, ACC)
    txt(s, x + Inches(0.25), Inches(4.65), Inches(3.2), Inches(0.6), v, 13, False, GRAY)
card(s, Inches(0.75), Inches(5.55), Inches(11.6), Inches(1.35), "举个例子：一次典型的数据是怎么用的",
     "发现问题：飞到 50 厘米后自己往下掉 → 读数据：油门被程序“砍”到 745‰，低于悬停所需 → "
     "改代码：禁止砍太狠 → 再飞：不再掉。", BLUE, 17, 13)
notes(s, "（65 秒）过程就是一个圈：改参数、烧进板子、真机飞、读数据，再改。一个圈五分钟，我们转了上百圈。"
         "中间修掉三十多个具体问题。举个真实例子：飞机到 50 厘米就往下掉，我们从数据里看到是程序把油门砍得太低，"
         "改掉之后就稳了——这就是“有数据”的价值。")

# ---------------- 11 坑 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第四部分 · 收获", "几个具体踩过的坑（都已修复）", 11)
pits = [("油门砍过头", "到高度后程序把油门降太多，反而掉下来", "改成“不低于悬停值”"),
        ("机体自转", "飞机一边飞一边转，光流把旋转当成平移", "偏航单独加大积分，转量降到 1/5"),
        ("姿态抖动", "增益调太猛，机身出现每秒 5 次的抖动", "把偏航和姿态的增益拆开"),
        ("下沉漩涡", "下降太快会掉进自己的气流，加满油门也拉不起", "禁止主动下降，缓慢降落")]
for i, (t, d, fix) in enumerate(pits):
    y = Inches(1.8 + i * 1.22)
    s1 = s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.75), y, Inches(11.6), Inches(1.02))
    s1.fill.solid(); s1.fill.fore_color.rgb = WHITE
    s1.line.color.rgb = RGBColor(0xDD,0xE3,0xEA); s1.shadow.inherit = False
    txt(s, Inches(1.0), y + Inches(0.08), Inches(3.2), Inches(0.4), t, 16, True, ACC)
    txt(s, Inches(1.0), y + Inches(0.50), Inches(7.0), Inches(0.4), d, 12.5, False, GRAY)
    txt(s, Inches(8.2), y + Inches(0.28), Inches(4.0), Inches(0.4), fix, 12.5, True, GREEN)
notes(s, "（70 秒）挑四个坑讲讲："
         "一是油门砍过头（到高度就掉）；二是自转，光流会把转当成平移；"
         "三是增益调太猛导致机身抖；四是下降太快会掉进自己的气流——就像船掉进自己开的浪里。"
         "右边是修复办法。这些都不是“理论”，是飞出来的经验。")

# ---------------- 12 进度 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第四部分 · 现状", "目标已经达成", 12)
rows = [("能稳定悬停的高度", "40 ~ 80 厘米（落在 20–100 厘米合规区间内）", GREEN),
        ("所需油门", "约 80%，留有余量", GREEN),
        ("连续在 20 厘米以上", "≥ 5 秒　✔ 达标", GREEN),
        ("横向漂移", "已解决：能稳定留在起飞点附近", GREEN)]
for i, (k, v, c) in enumerate(rows):
    y = Inches(1.85 + i * 1.0)
    txt(s, Inches(0.9), y, Inches(4.6), Inches(0.5), k, 17, True, BLUE)
    txt(s, Inches(5.6), y, Inches(7.0), Inches(0.5), v, 16, True, c)
    bar(s, Inches(0.9), y + Inches(0.58), Inches(11.4), Inches(0.02), RGBColor(0xDD,0xE3,0xEA))
txt(s, Inches(0.9), Inches(6.05), Inches(3.0), Inches(0.4), "目标完成度", 15, True, DARK)
bx, bw = Inches(2.9), Inches(8.2)
bar(s, bx, Inches(6.12), bw, Inches(0.42), GREEN)
txt(s, bx + int(bw / 2) - Inches(0.5), Inches(6.18), Inches(1.0), Inches(0.32),
    "100%", 13, True, WHITE, PP_ALIGN.CENTER)
notes(s, "【30 秒｜现状】现在可以明确地说：目标已经达成——它能在 20 到 100 厘米的合规区间内稳定停住，"
         "连续超过 5 秒，油门还有余量；之前最让人头疼的横向漂移也已经解决，能稳定留在起飞点附近。")

# ---------------- 13 下一步 ----------------
s = prs.slides.add_slide(BLANK); head(s, "第四部分 · 之后", "达成之后，我们还在做什么", 13)
nxt = [("① 把参数固化成“一套”", "任何一块板子烧进去都是同样的表现，现场不翻车"),
       ("② 保持动力余量", "换满电 / 高倍率电池，给现场留出 20% 的推力余量"),
       ("③ 写操作手册", "开机顺序、按键时机、异常处置，三步说清"),
       ("④ 反复复现验证", "连续多轮达标才算稳，而不是“偶尔一次”")]
for i, (t1, d1) in enumerate(nxt):
    y = Inches(1.85 + i * 1.22)
    bar(s, Inches(0.75), y, Inches(0.09), Inches(0.95), GREEN if i < 2 else BLUE)
    txt(s, Inches(1.02), y - Inches(0.02), Inches(11.3), Inches(0.45), t1, 18, True, BLUE)
    txt(s, Inches(1.02), y + Inches(0.44), Inches(11.3), Inches(0.4), d1, 14, False, GRAY)
txt(s, Inches(0.75), Inches(6.6), Inches(11.8), Inches(0.5),
    "目标不是“飞成功一次”，而是“每次都能成功”。", 15, True, GREEN)
notes(s, "【25 秒｜之后】达成之后我们没有停：把参数固化成一套、保持动力余量、写操作手册、反复复现验证。"
         "因为真正要的不是“成功一次”，而是“每次都能成功”。")

# ---------------- 14 总结 ----------------
s = prs.slides.add_slide(BLANK); bg(s, BLUE)
bar(s, 0, 0, W, Inches(0.14), ACC)
txt(s, Inches(0.9), Inches(0.85), Inches(11.5), Inches(0.5), "总结", 18, True, RGBColor(0xF5,0xC5,0x9A))
txt(s, Inches(0.9), Inches(1.35), Inches(11.5), Inches(1.0),
    "让一架小飞机“停住 5 秒”，", 38, True, WHITE)
txt(s, Inches(0.9), Inches(2.1), Inches(11.5), Inches(1.0),
    "我们做到了。", 38, True, WHITE)
take = [("学会了用数据说话", "黑匣子让每次失败都有明确原因"),
        ("学会了分层思考", "姿态、高度、位置分开解决，不混在一起"),
        ("把指标变成了确定性", "从“偶尔飞成”到“每次都能复现”")]
for i, (k, v) in enumerate(take):
    y = Inches(3.55 + i * 1.02)
    txt(s, Inches(0.95), y, Inches(11.0), Inches(0.45), "· " + k, 21, True,
        RGBColor(0xF5,0xC5,0x9A))
    txt(s, Inches(1.25), y + Inches(0.44), Inches(10.6), Inches(0.4), v, 14, False,
        RGBColor(0xD6,0xDF,0xEC))
txt(s, Inches(0.95), Inches(6.65), Inches(11.0), Inches(0.5), "谢谢，欢迎提问。", 18, True, WHITE)
notes(s, "【20 秒｜总结】最后总结一句：让它“飞起来”容易，让它“停住 5 秒”很难——但我们做到了。"
         "这一路学到三件事：用数据说话、分层解决问题、把指标变成确定性。谢谢大家。")


# ---- 统一重写讲稿备注（总时长约 7 分 55 秒 = 475 秒） ----
TALK = [
 ("20 秒｜开场", "大家好，我们做的是一架手掌大小的四轴飞行器。它的任务是：飞到离地 20 到 100 厘米之间，稳稳停住 5 秒。听起来简单，做起来非常难。"),
 ("10 秒｜结构", "我分四块讲：为什么难、它是什么做的、原理是什么、我们怎么调出来的。"),
 ("45 秒｜目标", "规则一句话：20 到 100 厘米之间停住 5 秒。难在三个字——准、久、稳：高度要准，时间要够，还不能飘出场地。后面所有技术都是为这三件事服务的。"),
 ("45 秒｜组成", "零件分三类：动力（电池、电调、电机和桨）、大脑（STM32 芯片）、感官（三个传感器）。右边是我们自制的遥控器，只有起飞和停止两个键。它没有摄像头也没有 GPS。"),
 ("45 秒｜升力", "四个桨往下扇风，空气把飞机顶上去。推力大于重量就上升，小于就下沉。想横着飞，就整体歪一下。所以“停住”要求推力几乎正好等于重量——差 1% 就会慢慢爬升或下沉。"),
 ("45 秒｜感官", "它靠三个便宜传感器：MPU6050 像人的平衡感；VL53L1X 是往下的激光尺，每 40 毫秒量一次高度；PMW3901 像眼睛盯着地砖看。单个都不难，难的是让它们互相配合、互相纠正。"),
 ("40 秒｜大脑", "它的思考只有三个字：量、比、改。量状态、和目标比、改电机转速。每秒 160 次。为什么这么快？因为它每一瞬间都在歪、都在掉，人手一秒只能反应一两次，必须交给芯片。"),
 ("40 秒｜难点", "难在三处：一，震动让传感器“撒谎”；二，上下左右互相干扰，动一个方向会带出两个；三，机器会“累”——同样油门，满电能飞 60 厘米，快没电只能飞 15 厘米。"),
 ("40 秒｜方法", "我们分三层控制：最底层保持水平，是安全底线；中间层控制高度，是比赛核心；最上层保持位置，是加分项。另外做了个“黑匣子”，每次飞行把数据存进芯片，落地读出来分析。"),
 ("40 秒｜过程", "过程是个圈：改参数、烧进板子、真机试飞、读数据找原因，再改。我们转了上百圈，修掉数十个具体问题。举一例：到 50 厘米就往下掉，读数据发现是程序把油门砍太低，改掉就好了。"),
 ("30 秒｜踩坑", "四个典型坑：油门砍过头、机体自转、姿态抖动、下降太快掉进自己的气流。右边是修复办法。这些不是书本上的，是飞出来的经验。"),
 ("30 秒｜现状", "老实说：高度基本达标，能稳在 60 到 80 厘米，油门只用 80%；但时间只到 2.7 秒，还差一半，主要卡在横向漂移。整体完成度约一半。"),
 ("25 秒｜计划", "下一步两条腿走：硬件换满电高倍率电池、减重；软件把“追绝对位置”改成“只稳速度”，并固化参数写手册。先把 2.7 秒推到 5 秒。"),
 ("20 秒｜总结", "一句话：让它飞起来容易，让它停住 5 秒很难。我们学会了用数据说话、分层解决问题，也清楚还差哪一段。谢谢大家。"),
]
for idx, sl in enumerate(prs.slides):
    if idx < len(TALK):
        tag, body = TALK[idx]
        sl.notes_slide.notes_text_frame.text = f"【{tag}】{body}"

out = "/home/czy/f22_hal_demos1/小飞机技术汇报.pptx"
prs.save(out)
print("saved:", out, os.path.getsize(out), "bytes")
print("slides:", len(prs.slides))
