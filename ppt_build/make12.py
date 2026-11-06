# -*- coding: utf-8 -*-
from pptx import Presentation
from pptx.util import Inches, Pt, Emu
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.enum.shapes import MSO_SHAPE
import os

F="" if False else "微软雅黑"
DARK  = RGBColor(0x1F,0x24,0x2B); BLUE = RGBColor(0x1F,0x3A,0x6E)
ACC   = RGBColor(0xE8,0x6E,0x2B); GRAY = RGBColor(0x6B,0x72,0x80)
LIGHT = RGBColor(0xF2,0xF5,0xF9); WHITE= RGBColor(0xFF,0xFF,0xFF)
GREEN = RGBColor(0x2E,0x7D,0x54); LINE = RGBColor(0xDD,0xE3,0xEA)

prs = Presentation(); prs.slide_width=Inches(13.333); prs.slide_height=Inches(7.5)
BLANK = prs.slide_layouts[6]; W,H = prs.slide_width, prs.slide_height

def bg(s,c=LIGHT):
    r=s.shapes.add_shape(MSO_SHAPE.RECTANGLE,0,0,W,H); r.fill.solid(); r.fill.fore_color.rgb=c
    r.line.fill.background(); r.shadow.inherit=False; return r

def txt(s,x,y,w,h,text,size=20,bold=False,color=DARK,align=PP_ALIGN.LEFT,space=6,line=1.25):
    tb=s.shapes.add_textbox(x,y,w,h); tf=tb.text_frame; tf.word_wrap=True
    tf.margin_left=tf.margin_right=Inches(0.05); tf.margin_top=tf.margin_bottom=Inches(0.02)
    for i,ln in enumerate(text.split("\n")):
        p=tf.paragraphs[0] if i==0 else tf.add_paragraph()
        p.alignment=align; p.space_after=Pt(space); p.line_spacing=line
        r=p.add_run(); r.text=ln; r.font.name=F; r.font.size=Pt(size)
        r.font.bold=bold; r.font.color.rgb=color
    return tb

def bar(s,x,y,w,h,color=ACC):
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,w,h); r.fill.solid()
    r.fill.fore_color.rgb=color; r.line.fill.background(); r.shadow.inherit=False; return r

def head(s,kick,title,no):
    bg(s); bar(s,0,0,W,Inches(0.10),BLUE)
    txt(s,Inches(0.75),Inches(0.42),Inches(10),Inches(0.4),kick,14,True,ACC)
    txt(s,Inches(0.75),Inches(0.80),Inches(11.5),Inches(0.9),title,34,True,BLUE)
    txt(s,W-Inches(1.3),H-Inches(0.62),Inches(0.8),Inches(0.4),f"{no:02d}",14,True,GRAY,PP_ALIGN.RIGHT)

def card(s,x,y,w,h,title,body,tc=BLUE,ts=19,bs=14):
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,w,h); r.fill.solid(); r.fill.fore_color.rgb=WHITE
    r.line.color.rgb=LINE; r.line.width=Pt(1); r.shadow.inherit=False
    if title:
        txt(s,x+Inches(0.22),y+Inches(0.16),w-Inches(0.44),Inches(0.45),title,ts,True,tc)
        txt(s,x+Inches(0.22),y+Inches(0.68),w-Inches(0.44),h-Inches(0.86),body,bs,False,DARK,space=5,line=1.3)
    else:
        txt(s,x+Inches(0.30),y+Inches(0.22),w-Inches(0.60),h-Inches(0.4),body,bs,True,tc)

def notes(s,text): s.notes_slide.notes_text_frame.text=text

def kicker_num(s, items, y0=Inches(1.85), step=Inches(1.22)):
    for i,(a,b) in enumerate(items):
        y=y0+i*step
        bar(s,Inches(0.75),y,Inches(0.09),Inches(0.95),ACC if i%2==0 else BLUE)
        txt(s,Inches(1.02),y-Inches(0.02),Inches(11.3),Inches(0.45),a,18,True,BLUE)
        txt(s,Inches(1.02),y+Inches(0.44),Inches(11.3),Inches(0.45),b,14,False,GRAY)

N=[]
def add(sec, body): N.append((sec, body))

# ================= 1 封面 =================
s=prs.slides.add_slide(BLANK); bg(s,BLUE); bar(s,0,H-Inches(0.22),W,Inches(0.22),ACC)
txt(s,Inches(0.9),Inches(1.15),Inches(11.5),Inches(0.5),"2026 悬浮飞行挑战赛 · 技术汇报",17,True,RGBColor(0xF5,0xC5,0x9A))
txt(s,Inches(0.9),Inches(1.75),Inches(11.5),Inches(1.0),"一架能“停在半空中”的小飞机",52,True,WHITE)
txt(s,Inches(0.9),Inches(2.95),Inches(11.2),Inches(0.6),"从一堆零件，到能自己稳住高度——我们做了什么、怎么做的、做到了什么程度",20,False,RGBColor(0xD6,0xDF,0xEC))
for i,(k,v) in enumerate([("4 个","螺旋桨"),("160 次/秒","自我修正"),("20–100 cm","目标高度范围"),("5 秒","目标悬停时间")]):
    x=Inches(0.9+i*3.0); txt(s,x,Inches(5.15),Inches(2.7),Inches(0.6),k,30,True,WHITE)
    txt(s,x,Inches(5.78),Inches(2.7),Inches(0.4),v,14,False,RGBColor(0xC9,0xD4,0xE4))
add("25 秒｜开场","大家好，我们做的是一架手掌大小的四轴飞行器。它的任务听起来很简单：飞到离地 20 到 100 厘米之间，然后稳稳停住 5 秒。今天我用大家都能听懂的话，讲清楚三件事：它是什么做的、凭什么能停住、我们怎么一步步调出来的。")

# ================= 2 目录 =================
s=prs.slides.add_slide(BLANK); head(s,"今天讲什么","五个部分，约 12 分钟",2)
items=[("01","比赛要我们做什么","规则用一句话说清，以及为什么它比“飞起来”难"),
       ("02","小飞机是什么做的","零件清单，每个零件一句“人话”解释"),
       ("03","它怎么飞、怎么想","升力、三个传感器、每秒 160 次的闭环"),
       ("04","我们怎么调出来的","黑匣子、真实数据案例、研制循环与三次关键突破"),
       ("05","现在做到什么程度","目标达成情况，以及达成之后在做什么")]
for i,(n,t1,d1) in enumerate(items):
    y=Inches(1.72+i*1.02)
    txt(s,Inches(0.85),y,Inches(1.0),Inches(0.6),n,26,True,ACC)
    txt(s,Inches(1.85),y-Inches(0.04),Inches(4.8),Inches(0.45),t1,20,True,BLUE)
    txt(s,Inches(6.75),y+Inches(0.02),Inches(5.9),Inches(0.45),d1,13,False,GRAY)
add("15 秒｜结构","我分五块讲：为什么难、它是什么做的、原理是什么、我们怎么调出来的、最后做到什么程度。")

# ================= 3 目标 =================
s=prs.slides.add_slide(BLANK); head(s,"第一部分 · 目标","比赛到底要我们做什么？",3)
card(s,Inches(0.75),Inches(1.8),Inches(5.9),Inches(2.3),"规则（大白话版）",
     "让飞行器起飞后，自己停在离地面 20～100 厘米\n之间的某个高度，保持至少 5 秒不碰地、不失控。",ACC,20,16)
card(s,Inches(6.95),Inches(1.8),Inches(5.6),Inches(2.3),"打个比方",
     "就像让一只蜂鸟，停在你膝盖到腰部之间的\n半空中，翅膀不停微调，五秒钟不掉下来。",BLUE,20,16)
txt(s,Inches(0.75),Inches(4.35),Inches(11.8),Inches(0.4),"为什么难？三件事必须同时做到：",17,True,DARK)
for i,(t1,d1) in enumerate([("准","差 20 厘米就掉出合格区间"),("久","不是“飞过去”，而是“待得住”5 秒"),("稳","场地小，一飘就出界")]):
    x=Inches(0.75+i*4.0); bar(s,x,Inches(4.85),Inches(0.09),Inches(1.2),ACC)
    txt(s,x+Inches(0.25),Inches(4.82),Inches(3.4),Inches(0.5),t1,24,True,BLUE)
    txt(s,x+Inches(0.25),Inches(5.38),Inches(3.4),Inches(0.7),d1,14,False,GRAY)
add("55 秒｜目标","规则一句话：在 20 到 100 厘米之间停住 5 秒。难在三个字：准、久、稳——高度要准，时间要够，还不能飘出场地。"
    "请注意，这三个字后面会被反复提到，我们的每一项技术，都是为解决其中一个字服务的。")

# ================= 4 组成 =================
s=prs.slides.add_slide(BLANK); head(s,"第二部分 · 组成","它由哪些零件组成？",4)
parts=[("机架","骨架，把所有零件连在一起"),("4 个电机 + 螺旋桨","提供升力，往下扇风"),
       ("4 个电调","电机的“油门”，调节转速"),("锂电池","动力来源，也最容易被忽视"),
       ("飞控板（大脑）","STM32 芯片，每秒算 160 次"),("3 个传感器","平衡感、高度尺、眼睛"),
       ("遥控器","人给它起飞 / 停止指令")]
for i,(t1,d1) in enumerate(parts):
    col=0 if i<4 else 1; row=i if i<4 else i-4
    x=Inches(0.75+col*3.55); y=Inches(1.85+row*1.18)
    bar(s,x,y+Inches(0.06),Inches(0.07),Inches(0.62),ACC if i%3==0 else BLUE)
    txt(s,x+Inches(0.22),y-Inches(0.02),Inches(3.2),Inches(0.4),t1,16,True,BLUE)
    txt(s,x+Inches(0.22),y+Inches(0.36),Inches(3.15),Inches(0.6),d1,12,False,GRAY)
img="/home/czy/f22_hal_demos1/ppt_build/rc.jpg"
if os.path.exists(img):
    pw=Inches(4.30); pic=s.shapes.add_picture(img,Inches(8.15),Inches(1.85),width=pw)
    pic.crop_top=0.20; pic.crop_bottom=0.20
    pic.height=Emu(int(pw*(2364.0/1773.0)*(1-0.40)))
    txt(s,Inches(8.15),Inches(5.45),Inches(4.7),Inches(0.7),"我们自制的遥控器：只有“起飞 / 停止”两个键",11.5,False,GRAY,line=1.3)
add("55 秒｜组成","零件分三类：动力（电池、电调、电机和桨）、大脑（STM32 芯片）、感官（三个传感器）。右边是我们自己做的遥控器，只有起飞和停止两个键。"
    "特别说明：这架飞机没有摄像头、没有 GPS、没有昂贵器件，全靠几十块钱的传感器自己判断——这也是它有意思的地方。")

# ================= 5 升力 =================
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 原理","它为什么会飞？",5)
card(s,Inches(0.75),Inches(1.85),Inches(5.7),Inches(2.05),"① 往上：推得比重力大",
     "4 个螺旋桨往下扇风，空气反过来把飞机往上顶。\n转速越高顶得越猛；顶得比自身重量大，就上升。",BLUE,19,15)
card(s,Inches(6.85),Inches(1.85),Inches(5.7),Inches(2.05),"② 往侧边：歪一下就横着走",
     "整体往右歪一点，向上的力就变成“斜的”，\n一部分变成往右推——所以歪 = 横向移动。",BLUE,19,15)
card(s,Inches(0.75),Inches(4.15),Inches(11.8),Inches(1.45),"③ 所以“停住”其实很难",
     "想停在原地：4 个电机转速必须几乎相等；想停在某高度：总推力必须正好等于重量。\n差 1%，它就会慢慢往上爬或往下沉——而这一点点差，人是看不出来也反应不过来的。",ACC,19,15)
txt(s,Inches(0.75),Inches(5.85),Inches(11.8),Inches(0.6),"这就是为什么必须有“大脑”不停地微调，而不是靠人手动飞。",16,True,GREEN)
add("45 秒｜升力","四个桨往下扇风，飞机被顶上去。往上飞是加转速，往侧飞是整体歪一下。"
    "重点在第三张卡：想停住，推力必须正好等于重量，差 1% 就会慢慢爬升或下沉——这个差人手根本反应不过来，所以必须交给芯片。")

# ================= 6 感官 =================
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 感官","它靠三个“感官”知道自己的状态",6)
sens=[("平衡感","MPU6050 陀螺仪 / 加速度计","像人耳朵里的“前庭”：\n感觉自己在往哪边歪、转得多快。\n\n→ 用来保持水平"),
      ("高度尺","VL53L1X 激光测距","像一把朝下照的尺子：\n每 40 毫秒量一次离地多高。\n\n→ 用来保持高度"),
      ("眼睛","PMW3901 光流传感器","像盯着地砖看：\n地砖往后跑 = 我在往前飞。\n\n→ 用来保持位置")]
for i,(k,c1,d1) in enumerate(sens):
    x=Inches(0.75+i*4.05)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,Inches(1.80),Inches(3.7),Inches(3.95)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.25),Inches(1.98),Inches(3.2),Inches(0.5),k,22,True,ACC)
    txt(s,x+Inches(0.25),Inches(2.48),Inches(3.2),Inches(0.6),c1,12.5,True,BLUE)
    txt(s,x+Inches(0.25),Inches(3.10),Inches(3.2),Inches(2.4),d1,14,False,DARK,line=1.45)
txt(s,Inches(0.75),Inches(5.95),Inches(11.8),Inches(0.7),
    "三个传感器加起来几十块钱——但让它们“互相配合、互相纠正”，才是真正的工作量。",15,True,GRAY)
add("55 秒｜感官","它只有三个便宜传感器，我用人的器官来类比：MPU6050 是平衡感，VL53L1X 是往下的激光尺，PMW3901 是盯着地面看的眼睛。"
    "这里要强调一句：单个传感器都很简单，难的是让它们互相配合——因为每一个都会出错。")

# ================= 7 大脑 =================
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 大脑","它怎么“思考”？—— 每秒 160 次的循环",7)
steps=[("量","读出：现在多高、歪几度、往哪飘"),("比","和目标高度比较，算出差多少"),("改","调整 4 个电机转速，把误差拉回来")]
for i,(k,d1) in enumerate(steps):
    x=Inches(1.0+i*4.1)
    c=s.shapes.add_shape(MSO_SHAPE.OVAL,x,Inches(1.9),Inches(1.15),Inches(1.15)); c.fill.solid()
    c.fill.fore_color.rgb=BLUE; c.line.fill.background(); c.shadow.inherit=False
    tf=c.text_frame; tf.text=k; tf.paragraphs[0].alignment=PP_ALIGN.CENTER
    run=tf.paragraphs[0].runs[0]; run.font.size=Pt(28); run.font.bold=True; run.font.color.rgb=WHITE; run.font.name=F
    txt(s,x-Inches(0.85),Inches(3.20),Inches(2.85),Inches(0.9),d1,13,False,GRAY)
    if i<2: txt(s,x+Inches(1.30),Inches(2.15),Inches(1.2),Inches(0.6),"→",30,True,ACC)
card(s,Inches(0.9),Inches(4.30),Inches(11.6),Inches(1.55),"为什么要 160 次/秒？",
     "因为它每秒都在歪、都在掉。人手一秒钟只能反应 1～2 次，\n而它 0.006 秒就修正一次——这叫“闭环控制”，是整架飞机的灵魂。",ACC,19,15)
add("45 秒｜大脑","它的思考只有三个字：量、比、改。量状态、和目标比、改电机转速，0.006 秒一次，也就是每秒 160 次。"
    "为什么这么快？因为它每一瞬间都在歪、都在掉，人手一秒只能反应一两次，必须交给芯片。")

# ================= 8 难点 =================
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 难点","听起来简单，为什么这么难？",8)
kick=[("① 传感器会“撒谎”","电机高速旋转带来震动，读数会乱跳；爬升时地面的样子在“变大”，光流会误以为在平移。"),
      ("② 三个方向会互相干扰","想上升就得加推力，加推力机体会歪一点；歪了就往旁边漂；一漂又影响高度——缠在一起。"),
      ("③ 机器本身会“乏力”","同样 80% 的油门，电池满电时能飞到 60 厘米，电池快没电时只能到 15 厘米。")]
kicker_num(s,kick)
add("45 秒｜难点","三个难点：一，震动让传感器“撒谎”；二，上下左右互相干扰，动一个方向会带出两个；三，机器会“累”——同样油门，满电飞 60 厘米，快没电只能飞 15 厘米。"
    "这三条，就是我们后面所有工作的出发点。")

# ================= 9 技术路线 =================
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 方法","我们的技术路线：三层控制 + 一本“黑匣子”",9)
layers=[("第 3 层　位置保持","用光流把飞机拉回起飞点附近","加分项",GRAY),
        ("第 2 层　高度控制","用激光测距把高度稳在目标值","比赛核心",ACC),
        ("第 1 层　姿态控制","让机体保持水平、不翻转","安全底线",GREEN)]
for i,(t1,d1,tag,c) in enumerate(layers):
    y=Inches(1.80+i*1.12)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,Inches(0.75),y,Inches(7.4),Inches(0.92)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,Inches(1.0),y+Inches(0.09),Inches(3.6),Inches(0.4),t1,17,True,BLUE)
    txt(s,Inches(1.0),y+Inches(0.50),Inches(6.9),Inches(0.4),d1,13,False,GRAY)
    txt(s,Inches(8.35),y+Inches(0.26),Inches(1.9),Inches(0.4),tag,13,True,c)
card(s,Inches(0.75),Inches(5.18),Inches(11.6),Inches(1.62),"为什么要分层？",
     "三个方向缠在一起，一起调必然互相打架。分层之后：底层先把“水平”固定住，\n"
     "中层只管“高度”，上层只管“位置”——每次只解决一个问题。",ACC,18,14)
add("50 秒｜方法","我们的路线分三层：最底层保持水平，是安全底线；中间层控制高度，是比赛核心；最上层保持位置，是加分项。"
    "为什么要分层？三个方向缠在一起，一起调必然互相打架；分层之后，每次只解决一个问题。")

# ================= 10 黑匣子 =================
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 工具","关键工具：给飞机装一个“黑匣子”",10)
card(s,Inches(0.75),Inches(1.80),Inches(3.7),Inches(3.55),"怎么存",
     "每 0.1 秒采一组数据，\n先存在内存里；\n落地后一次性写进\n芯片内置 Flash。\n\n容量：两页轮换，\n掉电不丢。",BLUE,19,14)
card(s,Inches(4.80),Inches(1.80),Inches(3.7),Inches(3.55),"记什么",
     "· 高度、垂直速度\n· 油门（占空比）\n· 横滚 / 俯仰角\n· 水平速度\n· 状态与故障码\n· 各轴修正量",BLUE,19,14)
card(s,Inches(8.85),Inches(1.80),Inches(3.7),Inches(3.55),"有什么用",
     "没有数据：只能“猜”为什么掉。\n\n有了数据：能定位到\n“第几秒、哪一环、差多少”。\n\n调试效率提高一个数量级。",ACC,19,14)
txt(s,Inches(0.75),Inches(5.55),Inches(11.8),Inches(0.7),
    "这是整个项目里，我们认为最值得投入的一小时——它把“感觉”变成了“证据”。",15,True,GREEN)
add("45 秒｜黑匣子","这一页我特别想讲：我们给飞机装了个黑匣子。每 0.1 秒记一组数据，落地写进芯片，掉电不丢。"
    "为什么重要？没有数据只能猜为什么掉，有了数据就能定位到第几秒、哪一环、差多少。老实说，这是整个项目里性价比最高的一小时。")

# ================= 11 数据案例 =================
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 案例","一次真实的数据分析：为什么它在 30 厘米处掉下来",11)
rows=[("t = 2.4 s","高度 376 mm","油门 942‰"),
      ("t = 2.6 s","高度 302 mm","油门 977‰"),
      ("t = 3.2 s","高度 110 mm","油门 990‰")]
txt(s,Inches(0.85),Inches(1.80),Inches(4.6),Inches(0.4),"从黑匣子里读出来的三行：",16,True,DARK)
for i,(a,b,c1) in enumerate(rows):
    y=Inches(2.28+i*0.62)
    txt(s,Inches(0.95),y,Inches(1.6),Inches(0.4),a,15,False,GRAY)
    txt(s,Inches(2.55),y,Inches(2.0),Inches(0.4),b,15,True,BLUE)
    txt(s,Inches(4.55),y,Inches(2.0),Inches(0.4),c1,15,True,ACC)
card(s,Inches(7.05),Inches(1.80),Inches(5.5),Inches(2.05),"结论 1：这不是控制问题",
     "油门一路加到 99%，高度仍从 376 掉到 110。\n→ 说明那一刻“推力不够”，不是算法算错。",ACC,18,14)
card(s,Inches(7.05),Inches(4.00),Inches(5.5),Inches(2.05),"结论 2：该换的是动力",
     "同一天更早的一次飞行：685‰ 就能稳住 630 mm。\n→ 电池状态决定了“能飞多高”，这点必须纳入策略。",GREEN,18,14)
txt(s,Inches(0.85),Inches(4.62),Inches(5.9),Inches(1.6),
    "这两条结论直接改变了我们的方向：\n从“继续调参数”转向“先把动力余量做够”。\n\n这一步转变，是最后达标的关键。",15,True,DARK,line=1.4)
add("50 秒｜案例","举个真实例子。这是黑匣子里的三行数据：两秒四时高度 376 毫米、油门 94%；到三秒二，油门加到 99%，高度只剩 110。"
    "结论很清楚：不是算法算错，是那一刻推力真的不够。同一天更早，68% 的油门就能稳住 63 厘米——差别在电池。"
    "所以我们把方向从“调参数”转向“先把动力余量做够”。")

# ================= 12 过程 =================
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 过程","研制过程：一个不停转的圈",12)
loop=[("改参数 / 改代码",0.75),("烧录进板子",3.65),("真机试飞",6.55),("读数据找原因",9.45)]
for t1,x in loop:
    x=Inches(x)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,Inches(1.95),Inches(2.6),Inches(1.0)); r.fill.solid()
    r.fill.fore_color.rgb=BLUE; r.line.fill.background(); r.shadow.inherit=False
    tf=r.text_frame; tf.text=t1; p=tf.paragraphs[0]; p.alignment=PP_ALIGN.CENTER
    run=p.runs[0]; run.font.size=Pt(16); run.font.bold=True; run.font.color.rgb=WHITE; run.font.name=F
    if x<Inches(9.4): txt(s,x+Inches(2.65),Inches(2.1),Inches(0.6),Inches(0.5),"→",26,True,ACC)
txt(s,Inches(0.75),Inches(3.20),Inches(11.8),Inches(0.5),"一个圈大约 5 分钟；同一套流程我们转了上百圈。",16,True,GREEN)
stats=[("上百次","真机试飞与数据回读"),("数十处","发现并修复的具体问题"),("多轮","控制结构迭代（开环 → 串级）")]
for i,(k,v) in enumerate(stats):
    x=Inches(0.75+i*4.05)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,Inches(3.95),Inches(3.7),Inches(1.4)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.25),Inches(4.10),Inches(3.2),Inches(0.6),k,26,True,ACC)
    txt(s,x+Inches(0.25),Inches(4.70),Inches(3.2),Inches(0.55),v,13,False,GRAY)
card(s,Inches(0.75),Inches(5.60),Inches(11.6),Inches(1.3),"这套流程的价值",
     "它把“靠灵感改代码”变成了“按证据改代码”：每次失败都能定位到一个具体原因，而不是南辕北辙地试。",BLUE,17,13)
add("45 秒｜过程","过程就是个圈：改参数、烧进板子、真机试飞、读数据找原因，再改。一个圈五分钟，我们转了上百圈，修掉数十个具体问题。"
    "这套流程最大的价值，是把“靠灵感改代码”变成“按证据改代码”。")

# ================= 13 坑 =================
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 踩坑","几个具体踩过的坑（都已修复）",13)
pits=[("油门砍过头","到高度后程序把油门降太多，反而掉下来","改成“不低于悬停值”"),
      ("机体自转","一边飞一边转，光流把旋转当成平移","偏航单独加大积分，转量降到 1/5"),
      ("姿态抖动","增益调太猛，机身出现每秒 5 次的抖动","把偏航与姿态的增益拆开"),
      ("下沉漩涡","下降太快会掉进自己的气流，加满油也拉不起","禁止主动下降，改为缓慢降落")]
for i,(t1,d1,fix) in enumerate(pits):
    y=Inches(1.80+i*1.22)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,Inches(0.75),y,Inches(11.6),Inches(1.02)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,Inches(1.0),y+Inches(0.08),Inches(3.2),Inches(0.4),t1,16,True,ACC)
    txt(s,Inches(1.0),y+Inches(0.50),Inches(7.0),Inches(0.4),d1,12.5,False,GRAY)
    txt(s,Inches(8.2),y+Inches(0.28),Inches(4.0),Inches(0.4),fix,12.5,True,GREEN)
add("45 秒｜踩坑","四个典型坑：油门砍过头、机体自转、姿态抖动、下降太快掉进自己的气流。右边是我们的修复办法。"
    "这些都不是书本上的知识，是一次次飞出来的经验。")

# ================= 14 三次突破 =================
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 突破","三个关键突破",14)
brk=[("① 高度环：从“开关式”到“串级”",
      "原来是“到了就冻结油门”，结果冲高又砸下。改成“高度差 → 期望速度 → 油门”的串级后，动作变连续，冲击消失。"),
     ("② 偏航：把两种增益拆开",
      "为压住自转把积分调大，结果姿态出现每秒 5 次的抖动。拆成“偏航用大积分、姿态用原值”后，自转从 30°/s 降到 5°/s，姿态也稳了。"),
     ("③ 传感器门限：只用可信的数据",
      "光流在“机体自转”和“快速升降”时会给出假数据。把这两种情况下的光流关掉，位置修正才真正开始起作用。")]
kicker_num(s,brk,y0=Inches(1.82),step=Inches(1.62))
txt(s,Inches(0.75),Inches(6.55),Inches(11.8),Inches(0.5),
    "共同点：都不是“调数字”，而是先想清楚“物理上发生了什么”。",15,True,GREEN)
add("55 秒｜突破","三个关键突破：第一，高度环从开关式改成串级，动作连续了；"
    "第二，把偏航和姿态的增益拆开，自转从 30 度每秒降到 5 度，姿态也不再抖；"
    "第三，只在光流可信的时候用它——自转和快速升降时关掉。共同点是：都不是调数字，而是先想清楚物理上发生了什么。")

# ================= 15 现状 =================
s=prs.slides.add_slide(BLANK); head(s,"第五部分 · 现状","目标已经达成",15)
rows=[("能稳定悬停的高度","40 ~ 80 厘米（落在 20–100 厘米合规区间内）",GREEN),
      ("所需油门","约 80%，留有余量",GREEN),
      ("连续在 20 厘米以上","≥ 5 秒　✔ 达标",GREEN),
      ("横向漂移","已解决：能稳定留在起飞点附近",GREEN)]
for i,(k,v,c) in enumerate(rows):
    y=Inches(1.85+i*1.0)
    txt(s,Inches(0.9),y,Inches(4.6),Inches(0.5),k,17,True,BLUE)
    txt(s,Inches(5.6),y,Inches(7.0),Inches(0.5),v,16,True,c)
    bar(s,Inches(0.9),y+Inches(0.58),Inches(11.4),Inches(0.02),LINE)
txt(s,Inches(0.9),Inches(6.05),Inches(3.0),Inches(0.4),"目标完成度",15,True,DARK)
bx,bw=Inches(2.9),Inches(8.2)
bar(s,bx,Inches(6.12),bw,Inches(0.42),GREEN)
txt(s,bx+int(bw/2)-Inches(0.5),Inches(6.18),Inches(1.0),Inches(0.32),"100%",13,True,WHITE,PP_ALIGN.CENTER)
add("35 秒｜现状","现在可以明确地说：目标已经达成——它能在 20 到 100 厘米的合规区间内稳定停住，连续超过 5 秒，油门还有余量；"
    "之前最让人头疼的横向漂移也已经解决，能稳定留在起飞点附近。")

# ================= 16 之后 =================
s=prs.slides.add_slide(BLANK); head(s,"第五部分 · 之后","达成之后，我们还在做什么",16)
nxt=[("① 把参数固化成“一套”","任何一块板子烧进去都是同样的表现，现场不翻车"),
     ("② 保持动力余量","换满电 / 高倍率电池，给现场留出 20% 的推力余量"),
     ("③ 写操作手册","开机顺序、按键时机、异常处置，三步说清"),
     ("④ 反复复现验证","连续多轮达标才算稳，而不是“偶尔一次”")]
kicker_num(s,nxt)
txt(s,Inches(0.75),Inches(6.6),Inches(11.8),Inches(0.5),
    "目标不是“飞成功一次”，而是“每次都能成功”。",15,True,GREEN)
add("30 秒｜之后","达成之后我们没有停：把参数固化成一套、保持动力余量、写操作手册、反复复现验证。"
    "因为真正要的不是“成功一次”，而是“每次都能成功”。")

# ================= 17 总结 =================
s=prs.slides.add_slide(BLANK); bg(s,BLUE); bar(s,0,0,W,Inches(0.14),ACC)
txt(s,Inches(0.9),Inches(0.85),Inches(11.5),Inches(0.5),"总结",18,True,RGBColor(0xF5,0xC5,0x9A))
txt(s,Inches(0.9),Inches(1.35),Inches(11.5),Inches(1.0),"让一架小飞机“停住 5 秒”，",38,True,WHITE)
txt(s,Inches(0.9),Inches(2.10),Inches(11.5),Inches(1.0),"我们做到了。",38,True,WHITE)
take=[("学会了用数据说话","黑匣子让每次失败都有明确原因"),
      ("学会了分层思考","姿态、高度、位置分开解决，不混在一起"),
      ("把指标变成了确定性","从“偶尔飞成”到“每次都能复现”")]
for i,(k,v) in enumerate(take):
    y=Inches(3.55+i*1.02)
    txt(s,Inches(0.95),y,Inches(11.0),Inches(0.45),"· "+k,21,True,RGBColor(0xF5,0xC5,0x9A))
    txt(s,Inches(1.25),y+Inches(0.44),Inches(10.6),Inches(0.4),v,14,False,RGBColor(0xD6,0xDF,0xEC))
txt(s,Inches(0.95),Inches(6.65),Inches(11.0),Inches(0.5),"谢谢，欢迎提问。",18,True,WHITE)
add("25 秒｜总结","最后总结一句：让它“飞起来”容易，让它“停住 5 秒”很难——但我们做到了。"
    "这一路学到三件事：用数据说话、分层解决问题、把指标变成确定性。谢谢大家。")

# ================= 18 备用 Q&A（1）=================
s=prs.slides.add_slide(BLANK); head(s,"备用页 · 不占时","可能被问到的问题（1/2）",18)
qa1=[("为什么不用 GPS？","室内/桌面场地只有一两米，GPS 误差就 2~5 米，根本用不上；而且体积、成本、启动时间都不合适。"),
     ("为什么不用摄像头做视觉定位？","要算力、要光照、延迟大；我们只需要“有没有在动”，光流芯片更省、更快。"),
     ("为什么不做固定翼？","题目要求“停住 5 秒”，固定翼必须一直往前飞，原理上就做不到悬停。"),
     ("5 秒是怎么判定达标的？","用激光测距每 40 毫秒记录一次，统计“连续高于 20 厘米”的帧数；现场再用秒表复核。")]
for i,(q,a) in enumerate(qa1):
    col=i%2; row=i//2
    x=Inches(0.75+col*6.1); y=Inches(1.72+row*2.28)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,Inches(5.75),Inches(2.02)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.22),y+Inches(0.14),Inches(5.3),Inches(0.45),q,16,True,ACC)
    txt(s,x+Inches(0.22),y+Inches(0.62),Inches(5.3),Inches(1.3),a,12.5,False,DARK,line=1.35)
add("0 秒｜备用","备用页：如果被问到“为什么不用 GPS / 摄像头 / 固定翼”“5 秒怎么判定”，直接翻到这页。")

# ================= 19 备用 Q&A（2）=================
s=prs.slides.add_slide(BLANK); head(s,"备用页 · 不占时","可能被问到的问题（2/2）",19)
qa2=[("成本大概多少？","核心器件都是低成本模块（传感器、单片机在几十元级），整机控制在数百元以内。"),
     ("最大的技术难点是什么？","传感器在“动态中”不可靠：自转时会把旋转读成平移，爬升时会把地面缩放读成前进。"),
     ("有没有更准的高度传感器？","板载有气压计，但在一两米量程内分辨率不够，所以主用激光，气压计只作辅助。"),
     ("现场万一失败怎么兜底？","参数固化 + 备用电池 + 开机检查清单 + 三次机会的节奏安排，先保底再冲成绩。")]
for i,(q,a) in enumerate(qa2):
    col=i%2; row=i//2
    x=Inches(0.75+col*6.1); y=Inches(1.72+row*2.28)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,Inches(5.75),Inches(2.02)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.22),y+Inches(0.14),Inches(5.3),Inches(0.45),q,16,True,ACC)
    txt(s,x+Inches(0.22),y+Inches(0.62),Inches(5.3),Inches(1.3),a,12.5,False,DARK,line=1.35)
card(s,Inches(0.75),Inches(6.15),Inches(11.85),Inches(0.9),None,"安全兜底：倾斜超限即停机、超时即停机、失联即停机、电机输出有上限。",GREEN,1,14)
add("0 秒｜备用","备用页：如果被问到成本、难点、备用方案，翻到这页。最后一句强调安全机制，评委通常很在意这一点。")

for i,sl in enumerate(prs.slides):
    if i < len(N):
        sec, body = N[i]
        sl.notes_slide.notes_text_frame.text = f"【{sec}】{body}"

out="/home/czy/f22_hal_demos1/小飞机技术汇报.pptx"
prs.save(out)
tot=sum(int(sec.split(" ")[0]) for sec,_ in N)
print("saved:",out,os.path.getsize(out),"bytes, slides:",len(prs.slides),"| 总时长:",tot,"秒 =",round(tot/60,1),"分钟")
