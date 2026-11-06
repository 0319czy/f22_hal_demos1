# -*- coding: utf-8 -*-
"""具身智能实验室版：突出实验室带领作用 + 个人成长；全程使用国标安全字符。"""
from pptx import Presentation
from pptx.util import Inches, Pt, Emu
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN
from pptx.enum.shapes import MSO_SHAPE
from pptx.oxml.ns import qn
from lxml import etree
import os

F  = "微软雅黑"
DARK=RGBColor(0x1F,0x24,0x2B); BLUE=RGBColor(0x1F,0x3A,0x6E); ACC=RGBColor(0xE8,0x6E,0x2B)
GRAY=RGBColor(0x6B,0x72,0x80);  LIGHT=RGBColor(0xF2,0xF5,0xF9); WHITE=RGBColor(0xFF,0xFF,0xFF)
GREEN=RGBColor(0x2E,0x7D,0x54); LINE=RGBColor(0xDD,0xE3,0xEA)

prs=Presentation(); prs.slide_width=Inches(13.333); prs.slide_height=Inches(7.5)
BLANK=prs.slide_layouts[6]; W,H=prs.slide_width,prs.slide_height

def _set_font(run, size, bold, color):
    """中英文都显式指定字体，避免跨平台回退导致缺字/乱码。"""
    run.font.name=F; run.font.size=Pt(size); run.font.bold=bold; run.font.color.rgb=color
    rPr=run._r.get_or_add_rPr()
    for tag in ('a:latin','a:ea','a:cs'):
        el=rPr.find(qn(tag))
        if el is None:
            el=etree.SubElement(rPr, qn(tag))
        el.set('typeface', F)

def bg(s,c=LIGHT):
    r=s.shapes.add_shape(MSO_SHAPE.RECTANGLE,0,0,W,H); r.fill.solid(); r.fill.fore_color.rgb=c
    r.line.fill.background(); r.shadow.inherit=False; return r

def txt(s,x,y,w,h,text,size=20,bold=False,color=DARK,align=PP_ALIGN.LEFT,space=6,line=1.25):
    tb=s.shapes.add_textbox(x,y,w,h); tf=tb.text_frame; tf.word_wrap=True
    tf.margin_left=tf.margin_right=Inches(0.05); tf.margin_top=tf.margin_bottom=Inches(0.02)
    for i,ln in enumerate(text.split("\n")):
        p=tf.paragraphs[0] if i==0 else tf.add_paragraph()
        p.alignment=align; p.space_after=Pt(space); p.line_spacing=line
        r=p.add_run(); r.text=ln; _set_font(r,size,bold,color)
    return tb

def bar(s,x,y,w,h,color=ACC):
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,w,h); r.fill.solid()
    r.fill.fore_color.rgb=color; r.line.fill.background(); r.shadow.inherit=False; return r

def head(s,kick,title,no):
    bg(s); bar(s,0,0,W,Inches(0.10),BLUE)
    txt(s,Inches(0.75),Inches(0.42),Inches(10),Inches(0.4),kick,14,True,ACC)
    txt(s,Inches(0.75),Inches(0.80),Inches(11.6),Inches(0.9),title,34,True,BLUE)
    txt(s,W-Inches(1.3),H-Inches(0.62),Inches(0.8),Inches(0.4),f"{no:02d}",14,True,GRAY,PP_ALIGN.RIGHT)

def card(s,x,y,w,h,title,body,tc=BLUE,ts=19,bs=14):
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,w,h); r.fill.solid(); r.fill.fore_color.rgb=WHITE
    r.line.color.rgb=LINE; r.line.width=Pt(1); r.shadow.inherit=False
    if title: txt(s,x+Inches(0.22),y+Inches(0.16),w-Inches(0.44),Inches(0.45),title,ts,True,tc)
    yy=y+Inches(0.68) if title else y+Inches(0.20)
    txt(s,x+Inches(0.22),yy,w-Inches(0.44),h-(yy-y)-Inches(0.14),body,bs,False,DARK,space=5,line=1.3)

N=[]
def add(sec,body): N.append((sec,body))

# ---------------- 01 封面 ----------------
s=prs.slides.add_slide(BLANK); bg(s,BLUE); bar(s,0,H-Inches(0.22),W,Inches(0.22),ACC)
txt(s,Inches(0.9),Inches(1.05),Inches(11.5),Inches(0.5),"具身智能实验室  ·  新成员课题汇报",17,True,RGBColor(0xF5,0xC5,0x9A))
txt(s,Inches(0.9),Inches(1.62),Inches(11.5),Inches(1.0),"一架能“停在半空中”的小飞机",50,True,WHITE)
txt(s,Inches(0.9),Inches(2.75),Inches(11.4),Inches(0.9),
    "在实验室的平台与指导下，从一堆零件做到能自己稳住高度",20,False,RGBColor(0xD6,0xDF,0xEC))
for i,(k,v) in enumerate([("4 个","螺旋桨"),("每秒 160 次","自我修正"),("20-100 厘米","目标高度"),("5 秒","目标悬停")]):
    x=Inches(0.9+i*3.0); txt(s,x,Inches(4.55),Inches(2.9),Inches(0.6),k,24,True,WHITE)
    txt(s,x,Inches(5.15),Inches(2.8),Inches(0.4),v,14,False,RGBColor(0xC9,0xD4,0xE4))
txt(s,Inches(0.9),Inches(6.05),Inches(11.4),Inches(0.5),"汇报人：实验室新成员　｜　时长约 12 分钟",14,False,RGBColor(0xB8,0xC6,0xDA))
add("25 秒｜开场","各位老师、各位师兄师姐好。我是实验室的新成员，这个课题是我进组后的第一个完整项目：让一架手掌大小的四轴飞行器，在离地 20 到 100 厘米之间稳稳停住 5 秒。今天既汇报技术，也汇报我在实验室这段时间的成长。")

# ---------------- 02 目录 ----------------
s=prs.slides.add_slide(BLANK); head(s,"汇报结构","五个部分，约 12 分钟",2)
items=[("01","课题与目标","实验室给我的题目，以及它为什么难"),
       ("02","小飞机是什么做的","零件清单，每个零件一句通俗解释"),
       ("03","它怎么飞、怎么想","升力、三个传感器、每秒 160 次的闭环"),
       ("04","在实验室怎么做的","平台与方法：黑匣子、数据案例、关键突破"),
       ("05","结果与我的成长","目标达成情况，以及我个人学到了什么")]
for i,(n,t1,d1) in enumerate(items):
    y=Inches(1.72+i*1.02)
    txt(s,Inches(0.85),y,Inches(1.0),Inches(0.6),n,26,True,ACC)
    txt(s,Inches(1.85),y-Inches(0.04),Inches(4.9),Inches(0.45),t1,20,True,BLUE)
    txt(s,Inches(6.85),y+Inches(0.02),Inches(5.8),Inches(0.45),d1,13,False,GRAY)
add("10 秒｜结构","分五块：课题与目标、它是什么做的、原理是什么、我们在实验室怎么做出来的、最后结果和我个人的成长。")

# ---------------- 03 目标 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第一部分 · 目标","题目是什么，现场是什么样",3)
card(s,Inches(0.75),Inches(1.78),Inches(7.0),Inches(1.62),"题目",
     "起飞后自己停在离地 20 到 100 厘米之间的某个高度，保持至少 5 秒不碰地、不失控。",ACC,19,15)
card(s,Inches(0.75),Inches(3.50),Inches(7.0),Inches(1.12),"通俗地说",
     "像让一只蜂鸟停在膝盖到腰之间的半空，五秒不掉下来。",BLUE,19,15)
txt(s,Inches(0.75),Inches(4.96),Inches(7.0),Inches(0.4),"难在三处：",15,True,DARK)
for i,(k,v) in enumerate([("准","差 20 厘米就出界"),("久","要待得住，不是飞过去"),("稳","场地小，一飘就出界")]):
    x=Inches(0.80+i*2.35)
    txt(s,x,Inches(5.42),Inches(2.2),Inches(0.4),k,20,True,BLUE)
    txt(s,x,Inches(5.80),Inches(2.2),Inches(0.5),v,11.5,False,GRAY)
pw=Inches(4.75)
pic=s.shapes.add_picture("/home/czy/f22_hal_demos1/ppt_build/q_venue.jpg",Inches(7.95),Inches(1.72),width=pw)
pic.height=Emu(int(pw*(1773.0/2364.0)*0.82)); pic.crop_top=0.09; pic.crop_bottom=0.09
txt(s,Inches(7.95),Inches(4.62),Inches(4.9),Inches(0.4),"比赛现场：地面是定位靶垫，周围用瓶子围出边界",10.5,False,GRAY)
pw2=Inches(2.30)
pic=s.shapes.add_picture("/home/czy/f22_hal_demos1/ppt_build/q_lineup.jpg",Inches(7.95),Inches(5.05),width=pw2)
pic.height=Emu(int(pw2*(2364.0/1773.0)*0.62)); pic.crop_top=0.19; pic.crop_bottom=0.19
txt(s,Inches(10.45),Inches(5.12),Inches(2.5),Inches(0.8),
    "同一场地，各队机器依次上场（不是我们的）",10.5,False,GRAY,line=1.35)
add("45 秒｜目标","题目一句话：在 20 到 100 厘米之间停住 5 秒。右边是比赛现场，地上是定位靶垫，四周用水瓶围出边界。难在三个字：准、久、稳。")

# ---------------- 04 组成 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第二部分 · 组成","它由哪些零件组成？",4)
parts=[("机架","骨架，把所有零件连在一起"),("4 个电机与螺旋桨","提供升力，往下扇风"),
       ("4 个电调","电机的“油门”，调节转速"),("锂电池","动力来源，也最容易被忽视"),
       ("飞控板（大脑）","STM32 芯片，每秒算 160 次"),("3 个传感器","平衡感、高度尺、眼睛"),
       ("遥控器","人给它起飞与停止指令")]
for i,(t1,d1) in enumerate(parts):
    col=0 if i<4 else 1; row=i if i<4 else i-4
    x=Inches(0.70+col*2.75); y=Inches(1.82+row*1.16)
    bar(s,x,y+Inches(0.06),Inches(0.07),Inches(0.60),ACC if i%3==0 else BLUE)
    txt(s,x+Inches(0.20),y-Inches(0.02),Inches(2.6),Inches(0.4),t1,14.5,True,BLUE)
    txt(s,x+Inches(0.20),y+Inches(0.34),Inches(2.55),Inches(0.6),d1,11,False,GRAY)
p1=Inches(2.55)
pic=s.shapes.add_picture("/home/czy/f22_hal_demos1/ppt_build/q_air.jpg",Inches(6.35),Inches(1.78),width=p1)
pic.height=Emu(int(p1*(2364.0/1773.0)))
p2=Inches(2.30)
pic=s.shapes.add_picture("/home/czy/f22_hal_demos1/ppt_build/rc.jpg",Inches(9.25),Inches(1.78),width=p2)
pic.height=Emu(int(p2*(2364.0/1773.0)*0.72)); pic.crop_top=0.14; pic.crop_bottom=0.14
txt(s,Inches(6.35),Inches(5.35),Inches(2.7),Inches(0.8),"整机：四轴加保护罩，中间是飞控板",11,False,GRAY,line=1.3)
txt(s,Inches(9.25),Inches(4.55),Inches(2.6),Inches(0.8),"自制遥控器：起飞、停止两个键",11,False,GRAY,line=1.3)
add("45 秒｜组成","零件分三类：动力、大脑、感官。中间是我们自己这台机器，加了保护罩；右边是自制遥控器，只有起飞和停止两个键。没有摄像头也没有 GPS。")

# ---------------- 05 升力 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 原理","它为什么会飞？",5)
card(s,Inches(0.75),Inches(1.85),Inches(5.7),Inches(2.05),"① 往上：推得比重力大",
     "4 个螺旋桨往下扇风，空气反过来把飞机往上顶。\n转速越高顶得越猛，顶得比自身重量大就上升。",BLUE,19,15)
card(s,Inches(6.85),Inches(1.85),Inches(5.7),Inches(2.05),"② 往侧边：歪一下就横着走",
     "整体往右歪一点，向上的力就变成“斜的”，\n一部分变成往右推，所以歪就是横向移动。",BLUE,19,15)
card(s,Inches(0.75),Inches(4.15),Inches(11.8),Inches(1.45),"③ 所以“停住”其实很难",
     "想停在原地：4 个电机转速必须几乎相等；想停在某个高度：总推力必须正好等于重量。\n差 1%，它就会慢慢爬升或下沉，而这一点点差，人看不出来也反应不过来。",ACC,19,15)
txt(s,Inches(0.75),Inches(5.85),Inches(11.8),Inches(0.6),"所以必须让芯片替我们盯着，这就是“闭环控制”。",16,True,GREEN)
add("45 秒｜升力","四个桨往下扇风，飞机被顶上去。往上飞是加转速，往侧飞是整体歪一下。重点在第三张卡：想停住，推力必须正好等于重量，差 1% 就会慢慢爬升或下沉。")

# ---------------- 06 感官 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 感官","它靠三个“感官”知道自己的状态",6)
sens=[("平衡感","MPU6050 陀螺仪与加速度计","像人耳朵里的“前庭”：\n感觉自己在往哪边歪、转得多快。\n\n用来保持水平"),
      ("高度尺","VL53L1X 激光测距","像一把朝下照的尺子：\n每 40 毫秒量一次离地多高。\n\n用来保持高度"),
      ("眼睛","PMW3901 光流传感器","像盯着地砖看：\n地砖往后跑，说明我在往前飞。\n\n用来保持位置")]
for i,(k,c1,d1) in enumerate(sens):
    x=Inches(0.75+i*4.05)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,Inches(1.80),Inches(3.7),Inches(3.95)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.25),Inches(1.98),Inches(3.2),Inches(0.5),k,22,True,ACC)
    txt(s,x+Inches(0.25),Inches(2.48),Inches(3.2),Inches(0.6),c1,12.5,True,BLUE)
    txt(s,x+Inches(0.25),Inches(3.10),Inches(3.2),Inches(2.4),d1,14,False,DARK,line=1.45)
txt(s,Inches(0.75),Inches(5.95),Inches(11.8),Inches(0.7),
    "三个传感器加起来几十元，但让它们“互相配合、互相纠正”，才是真正的工作量。",15,True,GRAY)
add("40 秒｜感官","它只有三个便宜传感器，我用人的器官来类比：MPU6050 是平衡感，VL53L1X 是往下的激光尺，PMW3901 是盯着地面看的眼睛。单个都简单，难的是让它们互相配合。")

# ---------------- 07 大脑 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 大脑","它怎么“思考”？每秒 160 次的循环",7)
for i,(k,d1) in enumerate([("量","读出：多高、歪几度、往哪飘"),("比","和目标高度比较，算出差多少"),("改","调整 4 个电机转速，把误差拉回")]):
    x=Inches(1.0+i*4.1)
    c=s.shapes.add_shape(MSO_SHAPE.OVAL,x,Inches(1.9),Inches(1.15),Inches(1.15)); c.fill.solid()
    c.fill.fore_color.rgb=BLUE; c.line.fill.background(); c.shadow.inherit=False
    tf=c.text_frame; tf.text=k; tf.paragraphs[0].alignment=PP_ALIGN.CENTER
    _set_font(tf.paragraphs[0].runs[0],28,True,WHITE)
    txt(s,x-Inches(0.85),Inches(3.20),Inches(2.85),Inches(0.9),d1,13,False,GRAY)
    if i<2: txt(s,x+Inches(1.30),Inches(2.15),Inches(1.2),Inches(0.6),"→",30,True,ACC)
card(s,Inches(0.9),Inches(4.30),Inches(11.6),Inches(1.55),"为什么要每秒 160 次？",
     "因为它每秒都在歪、都在掉。人手一秒钟只能反应 1 到 2 次，\n而它 0.006 秒就修正一次，这叫“闭环控制”，是整架飞机的灵魂。",ACC,19,15)
add("35 秒｜大脑","它的思考只有三个字：量、比、改。0.006 秒一次，也就是每秒 160 次。为什么这么快？因为它每一瞬间都在歪、都在掉，人手来不及。")

# ---------------- 08 难点 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 难点","听起来简单，为什么这么难？",8)
for i,(a,b) in enumerate([("① 传感器会“撒谎”","电机高速旋转带来震动，读数会乱跳；爬升时地面的样子在变大，光流会误以为在平移。"),
                          ("② 三个方向会互相干扰","想上升就得加推力，加推力机体会歪一点；歪了就往旁边漂；一漂又影响高度，缠在一起。"),
                          ("③ 机器本身会“乏力”","同样 80% 的油门，电池满电时能飞到 60 厘米，电池快没电时只能到 15 厘米。")]):
    y=Inches(1.85+i*1.62)
    bar(s,Inches(0.75),y,Inches(0.09),Inches(1.35),ACC)
    txt(s,Inches(1.0),y-Inches(0.02),Inches(11.3),Inches(0.45),a,20,True,BLUE)
    txt(s,Inches(1.0),y+Inches(0.46),Inches(11.3),Inches(0.9),b,14.5,False,DARK,line=1.35)
add("35 秒｜难点","三个难点：震动让传感器撒谎；上下左右互相干扰；机器会累，同样油门满电能飞 60 厘米，快没电只能飞 15 厘米。这三条是我们后面所有工作的出发点。")

# ---------------- 09 实验室 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 条件","做成这件事，靠的是哪些现成条件",9)
sup=[("① 硬件与场地","四轴套件、传感器模块、烧录器和试飞场地都是现成的，改完当天就能飞。"),
     ("② 建议的顺序","第一次拿数据去问，得到的建议是“先看数据，再改代码”，之后一直按这个顺序做。"),
     ("③ 已有工程规范","烧录流程、看门狗、安全停机、故障码，这些规范省了不少返工。"),
     ("④ 组内讨论","飞坏了就一起看记录，把原因说清楚再改，不靠猜。")]
for i,(a,b) in enumerate(sup):
    y=Inches(1.72+i*1.12)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,Inches(0.70),y,Inches(7.55),Inches(0.98)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,Inches(0.92),y+Inches(0.08),Inches(3.4),Inches(0.4),a,15.5,True,GREEN)
    txt(s,Inches(0.92),y+Inches(0.46),Inches(7.1),Inches(0.5),b,11.5,False,DARK)
p1=Inches(3.30)
pic=s.shapes.add_picture("/home/czy/f22_hal_demos1/ppt_build/q_team.jpg",Inches(8.55),Inches(1.72),width=p1)
pic.height=Emu(int(p1*(2364.0/1773.0)*0.86)); pic.crop_top=0.07; pic.crop_bottom=0.07
txt(s,Inches(8.55),Inches(5.85),Inches(3.6),Inches(0.6),"赛前合影（佩戴参赛证）",10.5,False,GRAY)
txt(s,Inches(0.70),Inches(6.30),Inches(7.6),Inches(0.5),
    "套件和流程都是现成的，我负责的是调参、改代码和试飞记录。",13.5,True,GRAY)
add("45 秒｜条件","这一页说清条件：套件、场地、烧录流程都是现成的；真正省时间的是那句“先看数据再改代码”；"
    "还有看门狗、安全停机这些规范，以及飞坏了大家一起看记录的习惯。我负责的是调参、改代码和试飞记录。")

# ---------------- 10 技术路线 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 方法","技术路线：三层控制，一次只解决一个问题",10)
for i,(t1,d1,tag,c) in enumerate([("第 3 层　位置保持","用光流把飞机拉回起飞点附近","加分项",GRAY),
                                 ("第 2 层　高度控制","用激光测距把高度稳在目标值","比赛核心",ACC),
                                 ("第 1 层　姿态控制","让机体保持水平、不翻转","安全底线",GREEN)]):
    y=Inches(1.80+i*1.12)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,Inches(0.75),y,Inches(7.4),Inches(0.92)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,Inches(1.0),y+Inches(0.09),Inches(3.6),Inches(0.4),t1,17,True,BLUE)
    txt(s,Inches(1.0),y+Inches(0.50),Inches(6.9),Inches(0.4),d1,13,False,GRAY)
    txt(s,Inches(8.35),y+Inches(0.26),Inches(1.9),Inches(0.4),tag,13,True,c)
card(s,Inches(0.75),Inches(5.18),Inches(11.6),Inches(1.62),"分层这个思路是组会上定下来的",
     "三个方向缠在一起，一起调必然互相打架。分层之后，底层先把“水平”固定住，\n中层只管“高度”，上层只管“位置”，每次只解决一个问题。",ACC,18,14)
add("40 秒｜方法","路线分三层：底层保持水平是安全底线，中层控制高度是核心，上层保持位置是加分项。三个方向缠在一起会互相打架，分层之后一次只解决一个问题。"
    "这个分层是组会上定下来的，按这个拆我才真正调得动。")

# ---------------- 11 黑匣子 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 工具","关键工具：给飞机装一个“黑匣子”",11)
card(s,Inches(0.75),Inches(1.80),Inches(3.7),Inches(3.55),"怎么存",
     "每 0.1 秒采一组数据，\n先存在内存里；\n落地后一次性写进\n芯片内置 Flash。\n\n容量：两页轮换，\n掉电不丢。",BLUE,19,14)
card(s,Inches(4.80),Inches(1.80),Inches(3.7),Inches(3.55),"记什么",
     "高度、垂直速度\n油门占比\n横滚与俯仰角\n水平速度\n状态与故障码\n各轴修正量",BLUE,19,14)
card(s,Inches(8.85),Inches(1.80),Inches(3.7),Inches(3.55),"有什么用",
     "没有数据：只能猜为什么掉。\n\n有了数据：能定位到\n第几秒、哪一环、差多少。\n\n调试效率提高一个数量级。",ACC,19,14)
txt(s,Inches(0.75),Inches(5.55),Inches(11.8),Inches(0.7),
    "这是我们在实验室做的最值的一件事，它把“感觉”变成了“证据”。",15,True,GREEN)
add("35 秒｜黑匣子","我们给飞机装了个黑匣子：每 0.1 秒记一组数据，落地写进芯片，掉电不丢。没有数据只能猜，有了数据能定位到第几秒、哪一环、差多少。这是我们做的最值的一件事。")

# ---------------- 12 数据案例 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 案例","一次真实的数据分析：为什么它在 30 厘米处掉下来",12)
txt(s,Inches(0.85),Inches(1.80),Inches(4.6),Inches(0.4),"从黑匣子里读出来的三行：",16,True,DARK)
for i,(a,b,c1) in enumerate([("第 2.4 秒","高度 376 毫米","油门 94%"),
                             ("第 2.6 秒","高度 302 毫米","油门 98%"),
                             ("第 3.2 秒","高度 110 毫米","油门 99%")]):
    y=Inches(2.28+i*0.62)
    txt(s,Inches(0.95),y,Inches(1.6),Inches(0.4),a,15,False,GRAY)
    txt(s,Inches(2.55),y,Inches(2.0),Inches(0.4),b,15,True,BLUE)
    txt(s,Inches(4.55),y,Inches(2.0),Inches(0.4),c1,15,True,ACC)
card(s,Inches(7.05),Inches(1.80),Inches(5.5),Inches(2.05),"结论一：这不是控制问题",
     "油门一路加到 99%，高度仍从 376 掉到 110。\n说明那一刻推力不够，不是算法算错。",ACC,18,14)
card(s,Inches(7.05),Inches(4.00),Inches(5.5),Inches(2.05),"结论二：该调整的是动力",
     "同一天更早的一次飞行，68% 油门就能稳住 630 毫米。\n说明电池状态决定了能飞多高。",GREEN,18,14)
txt(s,Inches(0.85),Inches(4.62),Inches(5.9),Inches(1.6),
    "这两条结论直接改变了方向：\n从“继续调参数”转向“先把动力余量做够”。\n\n这一步转变，是最后达标的关键。",15,True,DARK,line=1.4)
add("40 秒｜案例","举个真实例子：第 2.4 秒高度 376 毫米、油门 94%；到第 3.2 秒，油门加到 99%，高度只剩 110。"
    "结论是推力不够，不是算法错。同一天更早，68% 油门就能稳住 630 毫米，差别在电池。所以我们把方向从调参数转向先把动力余量做够，这是最后达标的关键。")

# ---------------- 13 循环 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 过程","研制过程：一个不停转的圈",13)
for t1,x in [("改参数或代码",0.75),("烧录进板子",3.65),("真机试飞",6.55),("读数据找原因",9.45)]:
    x=Inches(x)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,Inches(1.95),Inches(2.6),Inches(1.0)); r.fill.solid()
    r.fill.fore_color.rgb=BLUE; r.line.fill.background(); r.shadow.inherit=False
    tf=r.text_frame; tf.text=t1; p=tf.paragraphs[0]; p.alignment=PP_ALIGN.CENTER
    _set_font(p.runs[0],16,True,WHITE)
    if x<Inches(9.4): txt(s,x+Inches(2.65),Inches(2.1),Inches(0.6),Inches(0.5),"→",26,True,ACC)
txt(s,Inches(0.75),Inches(3.20),Inches(11.8),Inches(0.5),"一个圈大约 5 分钟，同一套流程我们转了上百圈。",16,True,GREEN)
for i,(k,v) in enumerate([("上百次","真机试飞与数据回读"),("数十处","发现并修复的具体问题"),("多轮","控制结构迭代")]):
    x=Inches(0.75+i*4.05)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,Inches(3.95),Inches(3.7),Inches(1.4)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.25),Inches(4.10),Inches(3.2),Inches(0.6),k,26,True,ACC)
    txt(s,x+Inches(0.25),Inches(4.70),Inches(3.2),Inches(0.55),v,13,False,GRAY)
card(s,Inches(0.75),Inches(5.60),Inches(11.6),Inches(1.3),"这套流程的价值",
     "它把“靠灵感改代码”变成“按证据改代码”：每次失败都能定位到一个具体原因。",BLUE,17,13)
add("35 秒｜过程","过程就是个圈：改参数、烧录、试飞、读数据，再改。一个圈五分钟，我们转了上百圈。它把靠灵感改代码变成了按证据改代码。")

# ---------------- 14 踩坑 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 踩坑","几个具体踩过的坑（都已修复）",14)
for i,(t1,d1,fix) in enumerate([("油门砍过头","到高度后程序把油门降太多，反而掉下来","改成不低于悬停值"),
                                ("机体自转","一边飞一边转，光流把旋转当成平移","偏航单独加大积分"),
                                ("姿态抖动","增益调太猛，机身出现每秒 5 次的抖动","偏航与姿态增益拆开"),
                                ("下沉漩涡","下降太快会掉进自己的气流，加满油也拉不起","禁止主动下降，缓慢降落")]):
    y=Inches(1.80+i*1.22)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,Inches(0.75),y,Inches(11.6),Inches(1.02)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,Inches(1.0),y+Inches(0.08),Inches(3.2),Inches(0.4),t1,16,True,ACC)
    txt(s,Inches(1.0),y+Inches(0.50),Inches(7.0),Inches(0.4),d1,12.5,False,GRAY)
    txt(s,Inches(8.2),y+Inches(0.28),Inches(4.0),Inches(0.4),fix,12.5,True,GREEN)
add("30 秒｜踩坑","四个典型坑：油门砍过头、机体自转、姿态抖动、下降太快掉进自己的气流。右边是修复办法。这些不是书本知识，是飞出来的经验。")

# ---------------- 15 突破 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 突破","三个关键突破",15)
for i,(a,b) in enumerate([("① 高度环：从“开关式”到“串级”","原来是到了就冻结油门，结果冲高又砸下。改成“高度差、期望速度、油门”的串级后动作变连续，冲击消失。"),
                          ("② 偏航：把两种增益拆开","为压住自转把积分调大，结果姿态每秒抖 5 次。拆成“偏航用大积分、姿态用原值”后，自转从 30 度每秒降到 5 度每秒。"),
                          ("③ 传感器门限：只用可信的数据","光流在机体自转和快速升降时会给出假数据。把这两种情况下的光流关掉，位置修正才真正起作用。")]):
    y=Inches(1.82+i*1.62)
    bar(s,Inches(0.75),y,Inches(0.09),Inches(1.35),ACC if i%2==0 else BLUE)
    txt(s,Inches(1.0),y-Inches(0.02),Inches(11.3),Inches(0.45),a,19,True,BLUE)
    txt(s,Inches(1.0),y+Inches(0.46),Inches(11.3),Inches(0.9),b,13.5,False,DARK,line=1.35)
txt(s,Inches(0.75),Inches(6.55),Inches(11.8),Inches(0.5),
    "共同点：都不是调数字，而是先想清楚“物理上发生了什么”。",15,True,GREEN)
add("35 秒｜突破","三个关键突破：高度环从开关式改成串级；偏航和姿态的增益拆开，自转从 30 度降到 5 度；只在光流可信时用它。共同点是先想清楚物理上发生了什么，而不是试数字。")

# ---------------- 16 现状 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第五部分 · 结果","目标已经达成",16)
for i,(k,v) in enumerate([("能稳定悬停的高度","40 到 80 厘米，落在 20 到 100 厘米的合规区间内"),
                          ("所需油门","约 80%，留有余量"),
                          ("连续在 20 厘米以上","5 秒以上　√ 达标"),
                          ("横向漂移","已解决，能稳定留在起飞点附近")]):
    y=Inches(1.85+i*1.0)
    txt(s,Inches(0.9),y,Inches(4.6),Inches(0.5),k,17,True,BLUE)
    txt(s,Inches(5.6),y,Inches(7.2),Inches(0.5),v,16,True,GREEN)
    bar(s,Inches(0.9),y+Inches(0.58),Inches(11.4),Inches(0.02),LINE)
txt(s,Inches(0.9),Inches(6.05),Inches(3.0),Inches(0.4),"目标完成度",15,True,DARK)
bx,bw=Inches(2.9),Inches(8.2); bar(s,bx,Inches(6.12),bw,Inches(0.42),GREEN)
txt(s,bx+int(bw/2)-Inches(0.5),Inches(6.18),Inches(1.0),Inches(0.32),"100%",13,True,WHITE,PP_ALIGN.CENTER)
add("35 秒｜结果","结果可以明确地说：目标达成。能在 20 到 100 厘米区间内稳定停住，连续 5 秒以上，油门还有余量；横向漂移也已解决。")

# ---------------- 17 个人成长 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第五部分 · 变化","这一段时间，我自己的变化",17)
grow=[("刚进组","只会按下按键，看不出问题在哪。","GRAY"),
      ("第一件事","先读数据，再改代码。","BLUE"),
      ("第二件事","把问题拆开，一次只改一个变量。","BLUE"),
      ("第三件事","硬件和代码一起看：推力不够时，调参数没用。","BLUE"),
      ("现在","能独立走完“改代码、烧录、试飞、读数据”的流程。","GREEN")]
for i,(k,v,cname) in enumerate(grow):
    y=Inches(1.75+i*0.98)
    c=GRAY if cname=="GRAY" else (GREEN if cname=="GREEN" else BLUE)
    txt(s,Inches(0.80),y,Inches(2.2),Inches(0.45),k,15,True,c)
    txt(s,Inches(3.05),y,Inches(6.4),Inches(0.6),v,13,False,DARK)
    bar(s,Inches(0.80),y+Inches(0.70),Inches(8.7),Inches(0.012),LINE)
p1=Inches(2.75)
pic=s.shapes.add_picture("/home/czy/f22_hal_demos1/ppt_build/q_frame.jpg",Inches(9.85),Inches(1.75),width=p1)
pic.height=Emu(int(p1*(2364.0/1773.0)))
txt(s,Inches(9.85),Inches(5.55),Inches(2.9),Inches(0.6),"拆装过的机架与飞控板",10.5,False,GRAY)
txt(s,Inches(0.80),Inches(6.75),Inches(8.7),Inches(0.4),
    "收获主要是这套流程，不只是做成了一架飞机。",14,True,GREEN)
add("40 秒｜变化","说说我自己：刚进组只会按键，看不出问题在哪。现在学会了三件事：先看数据再改代码、把问题拆开一次只改一个变量、硬件和代码一起看。"
    "现在能独立走完整个流程。收获主要是这套流程，不只是做成了一架飞机。")

# ---------------- 18 之后 ----------------
s=prs.slides.add_slide(BLANK); head(s,"第五部分 · 之后","接下来，把经验留下",18)
for i,(a,b) in enumerate([("① 参数固化成一套","任何一块板子烧进去都一样，现场不翻车"),
                          ("② 操作手册与清单","开机顺序、按键时机、异常处置，三步说清"),
                          ("③ 把方法写成文档","黑匣子怎么用、怎么读数据，留给下一位新成员"),
                          ("④ 继续复现验证","连续多轮达标才算稳，而不是偶尔一次")]):
    y=Inches(1.85+i*1.22)
    bar(s,Inches(0.75),y,Inches(0.09),Inches(0.95),GREEN if i<3 else BLUE)
    txt(s,Inches(1.02),y-Inches(0.02),Inches(11.3),Inches(0.45),a,18,True,BLUE)
    txt(s,Inches(1.02),y+Inches(0.44),Inches(11.3),Inches(0.4),b,14,False,GRAY)
txt(s,Inches(0.75),Inches(6.6),Inches(11.8),Inches(0.5),
    "目标是“每次都能成功”，而且下一个人能直接接着做。",15,True,GREEN)
add("20 秒｜之后","接下来把经验留下：参数固化、写操作手册、把方法写成文档留给下一位新成员，并继续复现验证。目标是每次都能成功，下一个人能直接接着做。")

# ---------------- 19 总结 ----------------
s=prs.slides.add_slide(BLANK); bg(s,BLUE); bar(s,0,0,W,Inches(0.14),ACC)
txt(s,Inches(0.9),Inches(0.85),Inches(11.5),Inches(0.5),"总结",18,True,RGBColor(0xF5,0xC5,0x9A))
txt(s,Inches(0.9),Inches(1.35),Inches(11.5),Inches(0.9),"让一架小飞机“停住 5 秒”，",36,True,WHITE)
txt(s,Inches(0.9),Inches(2.05),Inches(11.5),Inches(0.9),"我们做到了。",36,True,WHITE)
for i,(k,v) in enumerate([("平台与流程","套件、场地、烧录与安全规范都是现成的，省掉大量返工"),
                          ("方法","先看数据、再拆问题、软硬件一起考虑"),
                          ("结果","从“偶尔飞成”到“每次都能复现”")]):
    y=Inches(3.40+i*1.05)
    txt(s,Inches(0.95),y,Inches(11.0),Inches(0.45),"· "+k,20,True,RGBColor(0xF5,0xC5,0x9A))
    txt(s,Inches(1.25),y+Inches(0.44),Inches(10.6),Inches(0.4),v,13.5,False,RGBColor(0xD6,0xDF,0xEC))
txt(s,Inches(0.95),Inches(6.70),Inches(11.0),Inches(0.5),"谢谢各位，请多指正。",17,True,WHITE)
add("18 秒｜总结","总结三点：平台与流程是现成的，方法上学会了先看数据再拆问题，结果上从偶尔飞成做到每次都能复现。谢谢各位。")

# ---------------- 20 备用 Q&A 1 ----------------
s=prs.slides.add_slide(BLANK); head(s,"备用页 · 不占时","可能被问到的问题（1/2）",20)
for i,(q,a) in enumerate([("为什么不用 GPS？","场地只有一两米，GPS 误差就有 2 到 5 米，用不上。"),
                          ("为什么不用摄像头定位？","要算力、要光照、延迟大，光流芯片更省更快。"),
                          ("为什么不做固定翼？","要求停住 5 秒，固定翼必须一直往前飞。"),
                          ("5 秒怎么判定？","激光每 40 毫秒记录一次，统计连续高于 20 厘米的时长，再用秒表复核。")]):
    col=i%2; row=i//2
    x=Inches(0.75+col*6.1); y=Inches(1.72+row*2.28)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,Inches(5.75),Inches(2.02)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.22),y+Inches(0.14),Inches(5.3),Inches(0.45),q,16,True,ACC)
    txt(s,x+Inches(0.22),y+Inches(0.62),Inches(5.3),Inches(1.3),a,12.5,False,DARK,line=1.35)
add("0 秒｜备用","备用页：被问到 GPS、摄像头、固定翼、判定方式时翻到这一页。")

# ---------------- 21 备用 Q&A 2 ----------------
s=prs.slides.add_slide(BLANK); head(s,"备用页 · 不占时","可能被问到的问题（2/2）",21)
for i,(q,a) in enumerate([("成本大概多少？","核心器件都是低成本模块，整机控制在数百元以内。"),
                          ("最大技术难点？","传感器在动态中不可靠：自转时把旋转读成平移，爬升时把缩放读成前进。"),
                          ("有更准的高度传感器吗？","板载有气压计，但一两米量程内分辨率不够，主用激光。"),
                          ("现场失败怎么兜底？","参数固化、备用电池、开机清单、三次机会的节奏安排。")]):
    col=i%2; row=i//2
    x=Inches(0.75+col*6.1); y=Inches(1.72+row*2.28)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,Inches(5.75),Inches(2.02)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.22),y+Inches(0.14),Inches(5.3),Inches(0.45),q,16,True,ACC)
    txt(s,x+Inches(0.22),y+Inches(0.62),Inches(5.3),Inches(1.3),a,12.5,False,DARK,line=1.35)
card(s,Inches(0.75),Inches(6.15),Inches(11.85),Inches(0.9),None,
     "安全兜底：倾斜超限即停机、超时即停机、失联即停机、电机输出有上限。",GREEN,1,14)
add("0 秒｜备用","备用页：被问到成本、难点、兜底方案时翻到这里；最后一句强调安全机制。")

for i,sl in enumerate(prs.slides):
    if i < len(N):
        sec,body = N[i]
        sl.notes_slide.notes_text_frame.text = f"【{sec}】{body}"

out="/home/czy/f22_hal_demos1/小飞机课题汇报_实验室版.pptx"
prs.save(out)
tot=sum(int(sec.split(" ")[0]) for sec,_ in N)
print("saved:",out,os.path.getsize(out),"bytes | slides:",len(prs.slides),"| 正文时长:",tot,"秒 =",round(tot/60,1),"分钟")
