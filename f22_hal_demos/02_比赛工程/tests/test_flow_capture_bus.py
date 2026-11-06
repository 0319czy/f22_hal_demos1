#!/usr/bin/env python3
"""Exercise the actual boot-only bus-clear helper with electrical pin doubles."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/hal/Src/i2c_bus_recovery.c").read_text()
source = "\n".join(line for line in source.splitlines() if not line.startswith("#include"))
harness = r'''
#include <assert.h>
#include <stdio.h>
#define GPIOB 1
#define GPIO_PIN_10 1024
#define GPIO_PIN_11 2048
#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
#define GPIO_MODE_OUTPUT_OD 1
#define GPIO_NOPULL 0
#define GPIO_SPEED_FREQ_LOW 0
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_I2C2_CLK_ENABLE() ((void)0)
#define __HAL_RCC_I2C2_FORCE_RESET() ((void)0)
#define __HAL_RCC_I2C2_RELEASE_RESET() ((void)0)
typedef struct {unsigned Pin,Mode,Pull,Speed;} GPIO_InitTypeDef;
static unsigned requested=3072,stuck_sda,stuck_scl,edges;
static void HAL_Delay(unsigned ms) {assert(ms==1);}
static void HAL_GPIO_Init(int port,GPIO_InitTypeDef *g)
{assert(port==GPIOB && g->Pin==3072 && g->Mode==GPIO_MODE_OUTPUT_OD);}
static void HAL_GPIO_WritePin(int port,unsigned pin,unsigned high)
{
 assert(port==GPIOB);
 if (high) {
   if ((pin & GPIO_PIN_10) && !(requested & GPIO_PIN_10)) ++edges;
   requested |= pin;
 } else requested &= ~pin;
}
static unsigned HAL_GPIO_ReadPin(int port,unsigned pin)
{
 assert(port==GPIOB);
 if (pin==GPIO_PIN_10 && stuck_scl) return 0;
 if (pin==GPIO_PIN_11 && stuck_sda && edges<9) return 0;
 return !!(requested & pin);
}
'''
harness += source
harness += r'''
int main(void) {
 assert(I2C2_ClearBusAtBoot()==0 && edges==0);
 stuck_sda=1; assert(I2C2_ClearBusAtBoot()==9 && edges==9);
 edges=0;stuck_scl=1;assert(I2C2_ClearBusAtBoot()==0x100 && edges==0);
 puts("Actual boot bus clear: idle untouched, powered slave released after nine clocks, held SCL reported (GPIO doubles)");
}
'''
with tempfile.TemporaryDirectory() as temp:
    c = Path(temp)/"bus.c"
    c.write_text(harness)
    exe = Path(temp)/"bus"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    "-fsanitize=undefined,address", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
