//
// Created by Sammy Al Hashemi on 2020-02-02.
//

#include "CPU.h"
#include "Bus.h"

// This register keeps track if an interrupt condition was met or not
#define INTERRUPT_FLAG_REG 0xFF0F
// This register stores all the interrupts that will be handled, once flagged in INTERRUPT_FLAG_REG
#define INTERRUPT_ENABLE_REG 0xFFFF

CPU::CPU()
{
    using a = CPU;
    lookup =
            {
                    {}
            };
}

CPU::~CPU()=default;

u_int8_t CPU::READ(u_int16_t addr, bool read_only)
{
    // check for range validity occurs within bus implementation
    return bus->READ(addr, read_only);
}

void CPU::WRITE(u_int16_t addr, u_int8_t data)
{
    bus->WRITE(addr, data);
}

void CPU::SetFlag(CPU::Z80_FLAGS f, bool v) {
    // if we are setting the flag true
    // OR a bitwise shift
    // if we want to set it false
    // AND it with zero
    if (v) {
        regs.f |= f;
        regs.af.F |= f;
    } else {
        regs.f &= (unsigned)~f;
        regs.af.F &= (unsigned)~f;
    }
}

uint8_t CPU::GetFlag(CPU::Z80_FLAGS f) {
    return (regs.f & f) > 0 ? 1 : 0;
}

int CPU::stepCPU() {
    switch (READ(regs.pc)) {
        case 0x00:
            return NOP();
        default:
            printf("Unsupported OPCODE 0x%02x at 0x%04x", READ(regs.pc), regs.pc);
            std::exit(EXIT_FAILURE);
    }
}

void CPU::handleCycles(int c) {
    // TODO: create a method to handle cycles
    cycles += c;
    if (cycles >= 256) {
        cycles -= 256;
        // TODO: Flag
    }
}

void CPU::handleInterrupts() {
    if (interrupts_enabled) {
        if (READ(INTERRUPT_FLAG_REG) && READ(INTERRUPT_ENABLE_REG)) {
            // TODO: handle different interrupts here ie. VBLANK
        }
    }
}

/*
 * TODO: Complete OPCODES
 */

// No-op
CPU::OPCODE CPU::NOP() {
    regs.pc++;
    return 1;
}

