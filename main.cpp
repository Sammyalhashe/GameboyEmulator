#include <iostream>

#include <cstdint>
#include "CPU.h"
#include "Bus.h"

#define HAS_HALF_CARRY_8(n1, n2) (((n1 & 0x0Fu) + (n2 & 0x0Fu)) > 0x0F)

#define TESTING false

void tester(CPU cpu);

int main() {
    CPU cpu;
    Bus bus;
#if TESTING == true
    tester(cpu);
#endif
    bus.run();
    return 0;
}

void tester(CPU cpu) {
    cpu.regs.af.AF = 0x0fe0;
    printf("0x%02x 0x%02x 0x%04x\n", cpu.regs.af.A, cpu.regs.af.F, cpu.regs.af.AF);
    cpu.regs.af.A = 0xbb;
    printf("0x%02x 0x%02x 0x%04x\n", cpu.regs.af.A, cpu.regs.af.F, cpu.regs.af.AF);
    cpu.regs.af.F = 0xd0;
    printf("0x%02x 0x%02x 0x%04x\n", cpu.regs.af.A, cpu.regs.af.F, cpu.regs.af.AF);
    printf("Before Shift: 0x%02x 0x%02x 0x%04x\n", cpu.regs.af.A, cpu.regs.af.F, cpu.regs.af.AF);
    cpu.regs.af.A = cpu.regs.af.A << 1u;
    printf("A shifted: 0x%02x 0x%02x 0x%04x\n", cpu.regs.af.A, cpu.regs.af.F, cpu.regs.af.AF);
    auto Z = CPU::Z80_FLAGS::Z;
    cpu.regs.af.F &= (unsigned)~Z;
    printf("0x%02x 0x%02x 0x%04x\n", cpu.regs.af.A, cpu.regs.af.F, cpu.regs.af.AF);
    uint8_t t1 = 0x11;
    uint8_t t2 = 0xff;
    uint16_t test = t1 | (u_int16_t)(t2 << 8u); // test concatenation of two bytes -> seems to work
    printf("0x%04x\n", test);
    // test mask
    uint8_t byte_last_number = 0b01000000;
    uint8_t res = ((uint8_t)(0xbb) & byte_last_number);
    printf(res ? "true\n" : "false\n");
    // test set F
    uint16_t AF = 0xbb80;
    uint c = (uint16_t)(AF >> 15u) & 0x01u;
    uint16_t res2 = (AF & 0xFF00u) << 1u;
    printf("res2: 0x%04x, c: 0x%04x\n", res2, c << 8);

    uint8_t A = 0b10110010;
    uint carry = (uint8_t)(A >> 7u) & 0x01u;
    printf("A: 0x%02x ,new carry: 0x%02x, %d\n", A, carry, carry);
    A = (uint8_t)(A << 1u) | (uint8_t)carry;
    printf("new A: 0x%02x\n", A);


    cpu.regs.af.AF = 0x009E;
    printf("F: 0x%02x ,AF: 0x%04x\n", cpu.regs.af.F, cpu.regs.af.AF);
    // test the macro I defined
    int asdf = HAS_HALF_CARRY_8(cpu.regs.af.AF, 0x02u);
    cpu.regs.af.F += 0x02u;
    printf("new F: 0x%02x ,new AF: 0x%04x, HC? %d\n", cpu.regs.af.F, cpu.regs.af.AF, asdf);


    cpu.regs.af.AF = 0x009E;
    printf("F: 0x%02x ,AF: 0x%04x\n", cpu.regs.af.F, cpu.regs.af.AF);
    // test the macro I defined
    asdf = HAS_HALF_CARRY_8(cpu.regs.af.AF, -0x02u);
    cpu.regs.af.F -= 0x02u;
    printf("new F: 0x%02x ,new AF: 0x%04x, HC? %d\n", cpu.regs.af.F, cpu.regs.af.AF, asdf);
}
