"""按 USB VID:PID 定位板载 CH340，避免枚举号变化导致上传失败。

背景
    CH340 反复掉线重插后，内核会重新分配设备号：先是 ttyUSB0，掉一次
    可能就变成 ttyUSB1。写死 upload_port 迟早失效；而让 PlatformIO 自动
    探测又可能挑中本机的 /dev/ttyS* 传统串口并报 "Not a tty"。
    这里按 CH340 的 USB ID 精确匹配，两个问题一起避开。

注意
    不能用 realpath("/dev/ttyUSB0") 来找 USB 厂商：/dev 下的串口是字符
    设备节点而不是符号链接，realpath 会原样返回自己。必须从
    /sys/class/tty/ttyUSB*/device 出发往上找 idVendor。
"""

import glob
import os

Import("env")  # noqa: F821  (PlatformIO 注入的 env)

# CH340 系列：1a86:7523（CH340K/CH340E 等）、1a86:7522（CH340）。
CH340_VENDORS = ("1a86",)


def usb_ids(node):
    """返回该 tty 对应 USB 设备的 (idVendor, idProduct)，找不到返回 (None, None)。"""
    path = os.path.realpath("/sys/class/tty/%s/device" % node)
    # 目录层级：.../1-13:1.0/ttyUSB0/tty/ttyUSB0 -> 往上找带 idVendor 的那层
    for _ in range(8):
        vendor_file = os.path.join(path, "idVendor")
        if os.path.exists(vendor_file):
            try:
                with open(vendor_file) as handle:
                    vendor = handle.read().strip()
                with open(os.path.join(path, "idProduct")) as handle:
                    product = handle.read().strip()
                return vendor, product
            except OSError:
                return None, None
        parent = os.path.dirname(path)
        if parent == path:
            break
        path = parent
    return None, None


def find_ch340():
    """优先返回 VID 匹配 CH340 的节点；都不匹配时退回第一个 /dev/ttyUSB*。"""
    nodes = sorted(glob.glob("/dev/ttyUSB*"))
    for node in nodes:
        vendor, _product = usb_ids(os.path.basename(node))
        if vendor in CH340_VENDORS:
            return node, vendor
    if nodes:
        return nodes[0], None
    return None, None


port, vendor = find_ch340()

if port:
    env.Replace(UPLOAD_PORT=port)  # noqa: F821
    if vendor:
        print("CH340 detected on %s (USB VID %s)" % (port, vendor))
    else:
        print("no CH340 VID match; falling back to %s" % port)
else:
    print("WARNING: no /dev/ttyUSB* present; upload will fail")
