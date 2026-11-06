# -*- coding: utf-8 -*-
"""7 分钟精简版：13 页正文 + 2 页备用；全程 GBK 安全字符。"""
from pptx import Presentation
from pptx.util import Inches, Pt, Emu
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN
from pptx.enum.shapes import MSO_SHAPE
from pptx.oxml.ns import qn
from lxml import etree
import os

F="微软雅黑"
DARK=RGBColor(0x1F,0x24,0x2B); BLUE=RGBColor(0x1F,0x3A,0x6E); ACC=RGBColor(0xE8,0x6E,0x2B)
GRAY=RGBColor(0x6B,0x72,0x80); LIGHT=RGBColor(0xF2,0xF5,0xF9); WHITE=RGBColor(0xFF,0xFF,0xFF)
GREEN=RGBColor(0x2E,0x7D,0x54); LINE=RGBColor(0xDD,0xE3,0xEA)
IMG="/home/czy/f22_hal_demos1/ppt_build/"

prs=Presentation(); prs.slide_width=Inches(13.333); prs.slide_height=Inches(7.5)
BLANK=prs.slide_layouts[6]; W,H=prs.slide_width,prs.slide_height

def _f(run,size,bold,color):
    run.font.name=F; run.font.size=Pt(size); run.font.bold=bold; run.font.color.rgb=color
    rPr=run._r.get_or_add_rPr()
    for tag in ('a:latin','a:ea','a:cs'):
        el=rPr.find(qn(tag))
        if el is None: el=etree.SubElement(rPr,qn(tag))
        el.set('typeface',F)

def bg(s,c=LIGHT):
    r=s.shapes.add_shape(MSO_SHAPE.RECTANGLE,0,0,W,H); r.fill.solid(); r.fill.fore_color.rgb=c
    r.line.fill.background(); r.shadow.inherit=False

def txt(s,x,y,w,h,text,size=20,bold=False,color=DARK,align=PP_ALIGN.LEFT,space=6,line=1.25):
    tb=s.shapes.add_textbox(x,y,w,h); tf=tb.text_frame; tf.word_wrap=True
    tf.margin_left=tf.margin_right=Inches(0.05); tf.margin_top=tf.margin_bottom=Inches(0.02)
    for i,ln in enumerate(text.split("\n")):
        p=tf.paragraphs[0] if i==0 else tf.add_paragraph()
        p.alignment=align; p.space_after=Pt(space); p.line_spacing=line
        r=p.add_run(); r.text=ln; _f(r,size,bold,color)

def bar(s,x,y,w,h,color=ACC):
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,w,h); r.fill.solid()
    r.fill.fore_color.rgb=color; r.line.fill.background(); r.shadow.inherit=False

def head(s,kick,title,no):
    bg(s); bar(s,0,0,W,Inches(0.10),BLUE)
    txt(s,Inches(0.75),Inches(0.42),Inches(10),Inches(0.4),kick,14,True,ACC)
    txt(s,Inches(0.75),Inches(0.80),Inches(11.6),Inches(0.9),title,34,True,BLUE)
    txt(s,W-Inches(1.3),H-Inches(0.62),Inches(0.8),Inches(0.4),f"{no:02d}",14,True,GRAY,PP_ALIGN.RIGHT)

def card(s,x,y,w,h,title,body,tc=BLUE,ts=19,bs=14):
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,w,h); r.fill.solid(); r.fill.fore_color.rgb=WHITE
    r.line.color.rgb=LINE; r.line.width=Pt(1); r.shadow.inherit=False
    if title: txt(s,x+Inches(0.22),y+Inches(0.14),w-Inches(0.44),Inches(0.42),title,ts,True,tc)
    yy=y+Inches(0.62) if title else y+Inches(0.20)
    txt(s,x+Inches(0.22),yy,w-Inches(0.44),h-(yy-y)-Inches(0.12),body,bs,False,DARK,space=4,line=1.28)

def photo(s,name,x,y,w,ratio_crop=0.0,landscape=False):
    pic=s.shapes.add_picture(IMG+name,x,y,width=w)
    ar=(1773.0/2364.0) if not landscape else (1773.0/2364.0)
    pic.height=Emu(int(w*ar*(1-ratio_crop)))
    if ratio_crop>0:
        pic.crop_top=ratio_crop/2; pic.crop_bottom=ratio_crop/2
    return pic

N=[]
def add(sec,body): N.append((sec,body))

# 01 封面
s=prs.slides.add_slide(BLANK); bg(s,BLUE); bar(s,0,H-Inches(0.22),W,Inches(0.22),ACC)
txt(s,Inches(0.9),Inches(1.15),Inches(11.5),Inches(0.5),"具身智能实验室  ·  新成员课题汇报",17,True,RGBColor(0xF5,0xC5,0x9A))
txt(s,Inches(0.9),Inches(1.80),Inches(11.5),Inches(1.0),"一架能“停在半空中”的小飞机",50,True,WHITE)
txt(s,Inches(0.9),Inches(2.95),Inches(11.4),Inches(0.8),"从一堆零件，到能自己稳住高度",20,False,RGBColor(0xD6,0xDF,0xEC))
for i,(k,v) in enumerate([("4 个","螺旋桨"),("每秒 160 次","自我修正"),("20-100 厘米","目标高度"),("5 秒","目标悬停")]):
    x=Inches(0.9+i*3.0); txt(s,x,Inches(4.65),Inches(2.9),Inches(0.6),k,24,True,WHITE)
    txt(s,x,Inches(5.25),Inches(2.9),Inches(0.4),v,14,False,RGBColor(0xC9,0xD4,0xE4))
txt(s,Inches(0.9),Inches(6.20),Inches(11.4),Inches(0.5),"汇报人：实验室新成员　｜　约 7 分钟",14,False,RGBColor(0xB8,0xC6,0xDA))
add("15 秒｜开场","各位老师、师兄师姐好。我是实验室新成员，这是我进组后的第一个项目：让一架手掌大小的四轴飞行器，在离地 20 到 100 厘米之间停住 5 秒。")

# 02 题目与现场
s=prs.slides.add_slide(BLANK); head(s,"第一部分 · 题目","题目是什么，现场是什么样",2)
card(s,Inches(0.75),Inches(1.78),Inches(7.0),Inches(1.60),"题目",
     "起飞后自己停在离地 20 到 100 厘米之间的某个高度，保持至少 5 秒不碰地、不失控。",ACC,19,15)
card(s,Inches(0.75),Inches(3.50),Inches(7.0),Inches(1.10),"通俗地说",
     "像让一只蜂鸟停在膝盖到腰之间的半空，五秒不掉下来。",BLUE,19,15)
txt(s,Inches(0.75),Inches(4.80),Inches(7.0),Inches(0.4),"难在三处：",15,True,DARK)
for i,(k,v) in enumerate([("准","差 20 厘米就出界"),("久","要待得住"),("稳","一飘就出界")]):
    x=Inches(0.80+i*2.35)
    txt(s,x,Inches(5.22),Inches(2.2),Inches(0.4),k,20,True,BLUE)
    txt(s,x,Inches(5.60),Inches(2.2),Inches(0.5),v,11.5,False,GRAY)
pw=Inches(4.75)
pic=s.shapes.add_picture(IMG+"q_venue.jpg",Inches(7.95),Inches(1.72),width=pw)
pic.height=Emu(int(pw*(1773.0/2364.0)*0.82)); pic.crop_top=0.09; pic.crop_bottom=0.09
txt(s,Inches(7.95),Inches(4.62),Inches(4.9),Inches(0.4),"比赛现场：地面是定位靶垫，周围用瓶子围出边界",10.5,False,GRAY)
pw2=Inches(2.30)
pic=s.shapes.add_picture(IMG+"q_lineup.jpg",Inches(7.95),Inches(5.05),width=pw2)
pic.height=Emu(int(pw2*(2364.0/1773.0)*0.62)); pic.crop_top=0.19; pic.crop_bottom=0.19
txt(s,Inches(10.45),Inches(5.12),Inches(2.5),Inches(0.8),"同一场地，各队机器依次上场",10.5,False,GRAY)
add("45 秒｜题目","题目一句话：20 到 100 厘米之间停住 5 秒。右边是现场：地上是定位靶垫，周围用瓶子围边界；下面这张是同一场地各队机器依次上场。难在准、久、稳。")

# 03 组成
s=prs.slides.add_slide(BLANK); head(s,"第二部分 · 组成","它由哪些零件组成",3)
parts=[("机架","骨架"),("4 个电机与螺旋桨","提供升力"),("4 个电调","调节转速"),
       ("锂电池","动力来源"),("飞控板（大脑）","STM32，每秒算 160 次"),("3 个传感器","平衡、高度、位置"),
       ("遥控器","起飞与停止")]
for i,(t1,d1) in enumerate(parts):
    col=0 if i<4 else 1; row=i if i<4 else i-4
    x=Inches(0.70+col*2.75); y=Inches(1.86+row*1.14)
    bar(s,x,y+Inches(0.06),Inches(0.07),Inches(0.58),ACC if i%3==0 else BLUE)
    txt(s,x+Inches(0.20),y-Inches(0.02),Inches(2.6),Inches(0.4),t1,14.5,True,BLUE)
    txt(s,x+Inches(0.20),y+Inches(0.34),Inches(2.55),Inches(0.5),d1,11,False,GRAY)
p1=Inches(2.55)
pic=s.shapes.add_picture(IMG+"q_air.jpg",Inches(6.35),Inches(1.80),width=p1)
pic.height=Emu(int(p1*(2364.0/1773.0)))
p2=Inches(2.30)
pic=s.shapes.add_picture(IMG+"rc.jpg",Inches(9.25),Inches(1.80),width=p2)
pic.height=Emu(int(p2*(2364.0/1773.0)*0.72)); pic.crop_top=0.14; pic.crop_bottom=0.14
txt(s,Inches(6.35),Inches(5.38),Inches(2.7),Inches(0.7),"整机：四轴加保护罩",11,False,GRAY)
txt(s,Inches(9.25),Inches(4.58),Inches(2.6),Inches(0.7),"自制遥控器：两个键",11,False,GRAY)
add("40 秒｜组成","零件分三类：动力、大脑、感官。中间是这台机器，加了保护罩；右边是自制遥控器。没有摄像头，也没有 GPS。")

# 04 升力
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 原理","它为什么会飞",4)
card(s,Inches(0.75),Inches(1.85),Inches(5.7),Inches(1.95),"往上：推得比重力大",
     "螺旋桨往下扇风，空气把飞机顶上去；转速越高顶得越猛。",BLUE,19,15)
card(s,Inches(6.85),Inches(1.85),Inches(5.7),Inches(1.95),"往侧边：歪一下就横着走",
     "整体歪一点，向上的力变斜，一部分变成横向推力。",BLUE,19,15)
card(s,Inches(0.75),Inches(4.00),Inches(11.8),Inches(1.55),"所以“停住”很难",
     "想停住：4 个电机转速几乎相等，总推力正好等于重量。差 1%，它就慢慢爬升或下沉，\n而这一点点差，人看不出来也反应不过来。",ACC,19,15)
txt(s,Inches(0.75),Inches(5.75),Inches(11.8),Inches(0.5),"所以必须让芯片替我们盯着，这就是闭环控制。",16,True,GREEN)
add("35 秒｜升力","往下扇风把飞机顶上去；歪一下就横着走。关键是停住时推力必须正好等于重量，差 1% 就会慢慢爬升或下沉，人手反应不过来。")

# 05 感官
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 感官","它靠三个传感器知道自己的状态",5)
for i,(k,c1,d1) in enumerate([("平衡感","MPU6050","像人的“前庭”，感知歪和转\n用来保持水平"),
                              ("高度尺","VL53L1X 激光","每 40 毫秒量一次离地高度\n用来保持高度"),
                              ("眼睛","PMW3901 光流","盯着地砖看，判断有没有在动\n用来保持位置")]):
    x=Inches(0.75+i*4.05)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,Inches(1.80),Inches(3.7),Inches(3.30)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.25),Inches(1.98),Inches(3.2),Inches(0.45),k,21,True,ACC)
    txt(s,x+Inches(0.25),Inches(2.45),Inches(3.2),Inches(0.45),c1,13,True,BLUE)
    txt(s,x+Inches(0.25),Inches(2.98),Inches(3.2),Inches(1.9),d1,13.5,False,DARK,line=1.45)
txt(s,Inches(0.75),Inches(5.35),Inches(11.8),Inches(0.6),
    "单个都简单，难的是让三个互相配合、互相纠正。",15,True,GRAY)
add("35 秒｜感官","三个便宜传感器：MPU6050 是平衡感，VL53L1X 是往下的激光尺，PMW3901 是盯着地面的眼睛。单个都简单，难的是配合。")

# 06 大脑
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 大脑","它怎么思考：每秒 160 次的循环",6)
for i,(k,d1) in enumerate([("量","多高、歪几度、往哪飘"),("比","和目标比较"),("改","调 4 个电机转速")]):
    x=Inches(1.0+i*4.1)
    c=s.shapes.add_shape(MSO_SHAPE.OVAL,x,Inches(1.95),Inches(1.15),Inches(1.15)); c.fill.solid()
    c.fill.fore_color.rgb=BLUE; c.line.fill.background(); c.shadow.inherit=False
    tf=c.text_frame; tf.text=k; tf.paragraphs[0].alignment=PP_ALIGN.CENTER
    _f(tf.paragraphs[0].runs[0],28,True,WHITE)
    txt(s,x-Inches(0.85),Inches(3.25),Inches(2.85),Inches(0.8),d1,13,False,GRAY)
    if i<2: txt(s,x+Inches(1.30),Inches(2.20),Inches(1.2),Inches(0.6),"→",30,True,ACC)
card(s,Inches(0.9),Inches(4.35),Inches(11.6),Inches(1.40),"为什么这么快",
     "因为它每秒都在歪、都在掉；人手一秒只能反应 1 到 2 次，它 0.006 秒修正一次。",ACC,19,15)
add("30 秒｜大脑","它的思考就三个字：量、比、改，每秒 160 次。因为它一直在歪、在掉，人手来不及。")

# 07 难点
s=prs.slides.add_slide(BLANK); head(s,"第三部分 · 难点","为什么这么难",7)
for i,(a,b) in enumerate([("传感器会“撒谎”","震动让读数乱跳；爬升时地面在变大，光流会误判成在平移。"),
                          ("三个方向互相干扰","加推力机体就歪，歪了就横漂，横漂又影响高度。"),
                          ("机器会“乏力”","同样 80% 油门，满电能飞 60 厘米，快没电只能 15 厘米。")]):
    y=Inches(1.90+i*1.55)
    bar(s,Inches(0.75),y,Inches(0.09),Inches(1.20),ACC)
    txt(s,Inches(1.0),y-Inches(0.02),Inches(11.3),Inches(0.45),a,19,True,BLUE)
    txt(s,Inches(1.0),y+Inches(0.44),Inches(11.3),Inches(0.7),b,14,False,DARK,line=1.35)
add("25 秒｜难点","三个难点：震动让传感器撒谎；三个方向互相干扰；机器会累，同样油门满电能飞 60 厘米，快没电只能 15 厘米。")

# 08 条件（实验室）
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 条件","做成这件事，靠的是哪些现成条件",8)
sup=[("① 硬件与场地","套件、传感器、烧录器和试飞场地都是现成的，改完当天能飞。"),
     ("② 建议的顺序","得到的建议是“先看数据，再改代码”，之后一直按这个顺序做。"),
     ("③ 已有工程规范","烧录流程、看门狗、安全停机、故障码，省了不少返工。"),
     ("④ 组内讨论","飞坏了就一起看记录，把原因说清楚再改。")]
for i,(a,b) in enumerate(sup):
    y=Inches(1.74+i*1.14)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,Inches(0.70),y,Inches(7.55),Inches(1.00)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,Inches(0.92),y+Inches(0.08),Inches(3.4),Inches(0.4),a,15.5,True,GREEN)
    txt(s,Inches(0.92),y+Inches(0.46),Inches(7.1),Inches(0.5),b,11.5,False,DARK)
p1=Inches(3.30)
pic=s.shapes.add_picture(IMG+"q_team.jpg",Inches(8.55),Inches(1.74),width=p1)
pic.height=Emu(int(p1*(2364.0/1773.0)*0.86)); pic.crop_top=0.07; pic.crop_bottom=0.07
txt(s,Inches(8.55),Inches(5.87),Inches(3.6),Inches(0.5),"赛前合影",10.5,False,GRAY)
txt(s,Inches(0.70),Inches(6.35),Inches(7.6),Inches(0.5),
    "套件和流程都是现成的，我负责调参、改代码和试飞记录。",13.5,True,GRAY)
add("35 秒｜条件","条件是现成的：套件、场地、烧录流程；最省时间的是那句“先看数据再改代码”；还有看门狗、安全停机这些规范。我负责调参、改代码和试飞记录。")

# 09 方法：三层 + 黑匣子
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 方法","两层关键：分层控制，加一本“黑匣子”",9)
for i,(t1,d1,tag,c) in enumerate([("位置保持","用光流拉回起飞点","加分项",GRAY),
                                 ("高度控制","用激光把高度稳住","核心",ACC),
                                 ("姿态控制","让机体保持水平","底线",GREEN)]):
    y=Inches(1.76+i*1.06)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,Inches(0.75),y,Inches(6.5),Inches(0.88)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,Inches(1.0),y+Inches(0.08),Inches(3.0),Inches(0.4),t1,16.5,True,BLUE)
    txt(s,Inches(1.0),y+Inches(0.47),Inches(5.4),Inches(0.4),d1,12.5,False,GRAY)
    txt(s,Inches(5.95),y+Inches(0.24),Inches(1.2),Inches(0.4),tag,12,True,c)
card(s,Inches(7.55),Inches(1.76),Inches(5.05),Inches(3.32),"黑匣子：把感觉变成证据",
     "每 0.1 秒采一组数据，先在内存，落地写进\n芯片内置 Flash，掉电不丢。\n\n记：高度、油门、姿态角、水平速度、\n故障码、各轴修正量。\n\n没数据只能猜；有数据能定位到\n第几秒、哪一环、差多少。",ACC,16.5,11.5)
txt(s,Inches(0.75),Inches(5.10),Inches(6.5),Inches(0.9),
    "分层是组会上定下来的：\n先稳住姿态，再管高度，最后追位置。",14,True,DARK,line=1.4)
add("35 秒｜方法","方法两条：一是分层，姿态、高度、位置分开治，一次只解决一个问题；二是黑匣子，把每次飞行的数据存下来，从猜变成能定位。")

# 10 数据案例
s=prs.slides.add_slide(BLANK); head(s,"第四部分 · 证据","一次真实的数据：为什么它在 30 厘米处掉下来",10)
txt(s,Inches(0.85),Inches(1.80),Inches(4.6),Inches(0.4),"黑匣子里的三行：",16,True,DARK)
for i,(a,b,c1) in enumerate([("第 2.4 秒","高度 376 毫米","油门 94%"),
                             ("第 2.6 秒","高度 302 毫米","油门 98%"),
                             ("第 3.2 秒","高度 110 毫米","油门 99%")]):
    y=Inches(2.28+i*0.62)
    txt(s,Inches(0.95),y,Inches(1.6),Inches(0.4),a,15,False,GRAY)
    txt(s,Inches(2.55),y,Inches(2.0),Inches(0.4),b,15,True,BLUE)
    txt(s,Inches(4.55),y,Inches(2.0),Inches(0.4),c1,15,True,ACC)
card(s,Inches(7.05),Inches(1.78),Inches(5.5),Inches(1.95),"结论一：不是控制问题",
     "油门加到 99%，高度仍从 376 掉到 110，说明推力不够。",ACC,18,14)
card(s,Inches(7.05),Inches(3.85),Inches(5.5),Inches(1.95),"结论二：调整动力",
     "更早一次飞行，68% 油门就能稳住 630 毫米，差别在电池。",GREEN,18,14)
txt(s,Inches(0.85),Inches(4.55),Inches(5.9),Inches(1.4),
    "方向由此改变：从继续调参数，\n转向先把动力余量做够。",15,True,DARK,line=1.4)
add("35 秒｜证据","举个真实例子：2.4 秒时高度 376 毫米、油门 94%；到 3.2 秒油门 99%，高度只剩 110。说明推力不够，不是算法错；更早一次 68% 就能稳住 630 毫米，差别在电池。方向由此改成先把动力余量做够。")

# 11 结果
s=prs.slides.add_slide(BLANK); head(s,"第五部分 · 结果","目标达成情况",11)
for i,(k,v) in enumerate([("稳定悬停高度","40 到 80 厘米，在 20 到 100 厘米合规区间内"),
                          ("所需油门","约 80%，留有余量"),
                          ("连续在 20 厘米以上","5 秒以上　√ 达标"),
                          ("横向漂移","已解决，能留在起飞点附近")]):
    y=Inches(1.95+i*1.0)
    txt(s,Inches(0.9),y,Inches(4.2),Inches(0.5),k,17,True,BLUE)
    txt(s,Inches(5.2),y,Inches(7.5),Inches(0.5),v,16,True,GREEN)
    bar(s,Inches(0.9),y+Inches(0.58),Inches(11.4),Inches(0.02),LINE)
txt(s,Inches(0.9),Inches(6.15),Inches(3.0),Inches(0.4),"完成度",15,True,DARK)
bx,bw=Inches(2.5),Inches(8.6); bar(s,bx,Inches(6.20),bw,Inches(0.42),GREEN)
txt(s,bx+int(bw/2)-Inches(0.5),Inches(6.26),Inches(1.0),Inches(0.32),"100%",13,True,WHITE,PP_ALIGN.CENTER)
add("30 秒｜结果","结果：能在 20 到 100 厘米区间稳定停住，连续 5 秒以上，油门还有余量；横向漂移也解决了。")

# 12 变化
s=prs.slides.add_slide(BLANK); head(s,"第五部分 · 变化","这一段时间，我自己的变化",12)
for i,(k,v,c) in enumerate([("刚进组","只会按下按键，看不出问题在哪。",GRAY),
                            ("第一件事","先读数据，再改代码。",BLUE),
                            ("第二件事","把问题拆开，一次只改一个变量。",BLUE),
                            ("第三件事","硬件和代码一起看：推力不够时调参数没用。",BLUE),
                            ("现在","能独立走完改代码、烧录、试飞、读数据的流程。",GREEN)]):
    y=Inches(1.78+i*0.96)
    txt(s,Inches(0.80),y,Inches(2.2),Inches(0.45),k,15,True,c)
    txt(s,Inches(3.05),y,Inches(6.3),Inches(0.6),v,13,False,DARK)
    bar(s,Inches(0.80),y+Inches(0.68),Inches(8.6),Inches(0.012),LINE)
p1=Inches(2.75)
pic=s.shapes.add_picture(IMG+"q_frame.jpg",Inches(9.85),Inches(1.78),width=p1)
pic.height=Emu(int(p1*(2364.0/1773.0)))
txt(s,Inches(9.85),Inches(5.58),Inches(2.9),Inches(0.5),"拆装过的机架与飞控板",10.5,False,GRAY)
txt(s,Inches(0.80),Inches(6.72),Inches(8.6),Inches(0.4),
    "收获主要是这套流程，不只是做成了一架飞机。",14,True,GREEN)
add("30 秒｜变化","说说我自己：刚进组只会按键，现在学会了先看数据、把问题拆开、软硬一起看，能独立走完整个流程。收获主要是这套流程。")

# 13 总结
s=prs.slides.add_slide(BLANK); bg(s,BLUE); bar(s,0,0,W,Inches(0.14),ACC)
txt(s,Inches(0.9),Inches(0.85),Inches(11.5),Inches(0.5),"总结",18,True,RGBColor(0xF5,0xC5,0x9A))
txt(s,Inches(0.9),Inches(1.40),Inches(11.5),Inches(0.9),"让一架小飞机“停住 5 秒”，我们做到了。",34,True,WHITE)
for i,(k,v) in enumerate([("平台与流程","套件、场地、规范都是现成的"),
                          ("方法","先看数据、再拆问题、软硬一起考虑"),
                          ("结果","从偶尔飞成，到每次都能复现")]):
    y=Inches(3.05+i*1.05)
    txt(s,Inches(0.95),y,Inches(11.0),Inches(0.45),"· "+k,19,True,RGBColor(0xF5,0xC5,0x9A))
    txt(s,Inches(1.25),y+Inches(0.44),Inches(10.6),Inches(0.4),v,13.5,False,RGBColor(0xD6,0xDF,0xEC))
txt(s,Inches(0.95),Inches(6.55),Inches(11.0),Inches(0.5),"谢谢各位，请多指正。",17,True,WHITE)
add("15 秒｜总结","总结三点：平台与流程是现成的，方法上学会了先看数据再拆问题，结果上从偶尔飞成做到每次都能复现。谢谢。")

# 14-15 备用
s=prs.slides.add_slide(BLANK); head(s,"备用页 · 不占时","可能被问到的问题（1/2）",14)
for i,(q,a) in enumerate([("为什么不用 GPS？","场地只有一两米，GPS 误差就有 2 到 5 米。"),
                          ("为什么不用摄像头？","要算力、要光照、延迟大，光流更省。"),
                          ("为什么不做固定翼？","要求停住 5 秒，固定翼必须一直往前飞。"),
                          ("5 秒怎么判定？","激光每 40 毫秒记录，统计连续高于 20 厘米的时长。")]):
    col=i%2; row=i//2
    x=Inches(0.75+col*6.1); y=Inches(1.72+row*2.28)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,Inches(5.75),Inches(2.02)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.22),y+Inches(0.14),Inches(5.3),Inches(0.45),q,16,True,ACC)
    txt(s,x+Inches(0.22),y+Inches(0.62),Inches(5.3),Inches(1.3),a,12.5,False,DARK,line=1.35)
add("0 秒｜备用","备用页：被问到 GPS、摄像头、固定翼、判定方式时用。")

s=prs.slides.add_slide(BLANK); head(s,"备用页 · 不占时","可能被问到的问题（2/2）",15)
for i,(q,a) in enumerate([("成本大概多少？","核心是低成本模块，整机数百元以内。"),
                          ("最大技术难点？","传感器在动态中不可靠：自转和爬升会给假数据。"),
                          ("有更准的高度传感器吗？","板载气压计在一两米量程内分辨率不够，主用激光。"),
                          ("现场失败怎么兜底？","参数固化、备用电池、开机清单、三次机会安排。")]):
    col=i%2; row=i//2
    x=Inches(0.75+col*6.1); y=Inches(1.72+row*2.28)
    r=s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE,x,y,Inches(5.75),Inches(2.02)); r.fill.solid()
    r.fill.fore_color.rgb=WHITE; r.line.color.rgb=LINE; r.shadow.inherit=False
    txt(s,x+Inches(0.22),y+Inches(0.14),Inches(5.3),Inches(0.45),q,16,True,ACC)
    txt(s,x+Inches(0.22),y+Inches(0.62),Inches(5.3),Inches(1.3),a,12.5,False,DARK,line=1.35)
card(s,Inches(0.75),Inches(6.15),Inches(11.85),Inches(0.9),None,
     "安全兜底：倾斜超限即停机、超时即停机、失联即停机、电机输出有上限。",GREEN,1,14)
add("0 秒｜备用","备用页：成本、难点、兜底方案；最后强调安全机制。")

for i,sl in enumerate(prs.slides):
    if i<len(N):
        sec,body=N[i]
        sl.notes_slide.notes_text_frame.text=f"【{sec}】{body}"

out="/home/czy/f22_hal_demos1/小飞机课题汇报_7分钟版.pptx"
prs.save(out)
tot=sum(int(sec.split(" ")[0]) for sec,_ in N)
print("saved:",out,os.path.getsize(out),"| slides:",len(prs.slides),"| 正文:",tot,"秒 =",round(tot/60,1),"分钟")
