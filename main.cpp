#include <iostream>

#include <cstdint>
#include "CPU.h"

int main() {
    CPU cpu;
    cpu.regs.af.AF = 0x0fe0;
    printf("0x%02x 0x%02x 0x%04x\n", cpu.regs.af.A, cpu.regs.af.F, cpu.regs.af.AF);
    cpu.regs.af.A = 0xbb;
    printf("0x%02x 0x%02x 0x%04x\n", cpu.regs.af.A, cpu.regs.af.F, cpu.regs.af.AF);
    cpu.regs.af.F = 0xd0;
    printf("0x%02x 0x%02x 0x%04x\n", cpu.regs.af.A, cpu.regs.af.F, cpu.regs.af.AF);
    auto Z = CPU::Z80_FLAGS::Z;
    cpu.regs.af.F &= (unsigned)~Z;
    printf("0x%02x 0x%02x 0x%04x\n", cpu.regs.af.A, cpu.regs.af.F, cpu.regs.af.AF);
    return 0;
}
