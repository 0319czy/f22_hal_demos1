#!/usr/bin/env python3
"""Check real SPI transaction helpers with injected HAL and a virtual clock."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/sensors/f22_sensor_bus.c").read_text()
helpers = source[source.index("static int SpiTransfer("):source.index("void F22_ModuleBusInit(")]
harness = r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef enum {HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;
#define GPIOB 1
#define GPIO_PIN_12 12
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
static int module_spi, selected, calls, fail_phase, fail_status, edges;
static uint32_t clock_us, address_done;
static uint8_t last_address, last_value;
static void DelayUs(uint32_t n) { clock_us += n; }
static void HAL_GPIO_WritePin(int port, int pin, int value)
{ assert(port == GPIOB && pin == GPIO_PIN_12); selected = !value; edges++; }
static HAL_StatusTypeDef HAL_SPI_Transmit(int *spi, uint8_t *data, uint16_t n, unsigned timeout)
{
    assert(spi == &module_spi && selected && timeout == 10); calls++;
    if (calls == fail_phase) return (HAL_StatusTypeDef)fail_status;
#ifdef FLOW_SPI_DUPLEX_PROBE
    assert(calls == 1 && n == 2);
    last_address = data[0]; last_value = data[1];
#else
    assert(n == 1);
    if (calls == 1) { last_address = *data; address_done = clock_us; }
    else { assert(clock_us-address_done == 50); last_value = *data; }
#endif
    return HAL_OK;
}
static HAL_StatusTypeDef HAL_SPI_TransmitReceive(int *spi, uint8_t *tx, uint8_t *rx,
                                              uint16_t n, unsigned timeout)
{
    assert(spi == &module_spi && selected && timeout == 10); calls++;
    if (calls == fail_phase) return (HAL_StatusTypeDef)fail_status;
#ifdef FLOW_SPI_DUPLEX_PROBE
    if (calls == 1) {
        assert(n == 1); last_address = tx[0]; address_done = clock_us;
        rx[0] = 0xff; return HAL_OK;
    }
#endif
    assert(clock_us-address_done == (n == 1 ? 500U : 50U));
    for (unsigned i=0;i<n;i++) { assert(tx[i] == 0); rx[i]=0xb6; }
    return HAL_OK;
}
'''
harness += helpers
harness += r'''
static void Reset(int phase, int status)
{ selected=calls=edges=0; clock_us=0; fail_phase=phase; fail_status=status; }
int main(void)
{
    uint8_t bytes[17], value=0x5a;
    Reset(0,0); assert(FlowRead(0,0x5f,bytes,1)==0);
    assert(bytes[0]==0xb6 && last_address==0x5f && !selected && edges==2 && clock_us==800);
    Reset(0,0); assert(FlowRead(0,0x16,bytes,12)==0);
    assert(last_address==0x16 && !selected && clock_us==350);
    Reset(0,0); assert(FlowWrite(0,0x3a,&value,1)==0);
    assert(last_address==0xba && last_value==0x5a && !selected);
#ifdef FLOW_SPI_DUPLEX_PROBE
    assert(clock_us==300 && calls==1);
#else
    assert(clock_us==350 && calls==2);
#endif
    for (int phase=1;phase<=2;phase++) for (int status=1;status<=3;status++) {
        Reset(phase,status); assert(FlowRead(0,0x5f,bytes,1)==-status);
        assert(!selected && edges==2 && calls==phase);
        if (phase == 1
#ifndef FLOW_SPI_DUPLEX_PROBE
            || phase == 2
#endif
        ) {
            Reset(phase,status); assert(FlowWrite(0,0x3a,&value,1)==-status);
            assert(!selected && edges==2 && calls==phase);
        }
    }
    Reset(0,0); assert(FlowRead(0,0x16,bytes,sizeof(bytes))<0 && calls==0 && edges==0);
    puts("Real SPI helper: register/burst timing, address bits, HAL status preservation and chip-select cleanup passed (injected HAL; no hardware claim)");
}
'''
with tempfile.TemporaryDirectory() as folder:
    source_path = Path(folder) / "flow_spi.c"
    binary = Path(folder) / "flow_spi"
    source_path.write_text(harness)
    for flags in ([], ["-DFLOW_ID_PROBE=1", "-DFLOW_SPI_DUPLEX_PROBE=1"]):
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                        "-fsanitize=address,undefined", "-g", *flags,
                        str(source_path), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
