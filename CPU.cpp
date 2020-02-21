//
// Created by Sammy Al Hashemi on 2020-02-02.
//

#include "CPU.h"
#include "Bus.h"

// This register keeps track if an interrupt condition was met or not
#define INTERRUPT_FLAG_REG 0xFF0F
// This register stores all the interrupts that will be handled, once flagged in INTERRUPT_FLAG_REG
#define INTERRUPT_ENABLE_REG 0xFFFF
// This is a special instruction that tells the cpu to look into a separate OPCODE table.
#define PREFIX 0xCB

// masks
#define BYTE_FIRST_BIT_MASK 0b10000000u


// special helper functions
#define HAS_CARRY(n, n1, n2) ((uint16_t)(n < n1) | (uint16_t)((n < n2)))
#define HAS_HALF_CARRY(nn1, nn2) (((nn1 & 0x0FFFu) + (nn2 + 0x0FFFu)) > 0x0FFF)
#define HAS_HALF_CARRY_8(n1, n2) (((n1 & 0x0Fu) + (n2 & 0x0Fu)) > 0x0F)
#define HAS_HALF_CARRY_DECREMENT_8(n) (((n & 0x0Fu) == 0x0Fu))
#define IS_ZERO_8(n) (n == 0)


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
    return bus->READ(addr);
}

void CPU::WRITE(u_int16_t addr, u_int8_t data)
{
    bus->WRITE(addr, data);
}

// Read two-bytes for instructions that load in nn (for example)
u_int16_t CPU::ReadNn()
{
    uint8_t lower = READ(regs.pc++);
    uint8_t upper = READ(regs.pc++);
    // This concatenation seems to work an gets rid of signed warnings
    return lower | (uint16_t)(upper << 8u);
}

// Reads an unsigned memory-instruction
uint8_t CPU::ReadN()
{
    return READ(regs.pc++);
}

// Reads a signed instruction
// Glancing over the OPCODES, it seems more useful for INTERRUPTS
int8_t CPU::ReadI() {
    return (int8_t) READ(regs.pc++);
}

void CPU::SetFlag(CPU::Z80_FLAGS f, bool v) {
    // if we are setting the flag true
    // OR a bitwise shift
    // if we want to set it false
    // AND it with zero
    if (v) {
        regs.af.F |= f;
    } else {
        regs.af.F &= (unsigned)~f;
    }
}

uint8_t CPU::GetFlag(CPU::Z80_FLAGS f) {
    return (regs.f & f) > 0 ? 1 : 0;
}

int CPU::stepCPU() {
    switch (READ(regs.pc++)) {
        /* First Row */
        case 0x00:
            return NOP();
        case 0x01:
            return LD_BC_nn(ReadNn());
        case 0x02:
            return LD_Addr_BC_A();
        case 0x03:
            return INC_BC();
        case 0x04:
            return INC_B();
        case 0x05:
            return DEC_B();
        case 0x06:
            return LD_B_n(ReadN());
        case 0x07:
            return RLC_A();
        case 0x08:
            return LD_Addr_nn_SP(ReadNn());
        case 0x09:
            return ADD_HL_BC();
        case 0x0A:
            return LD_A_Addr_BC();
        case 0x0B:
            return DEC_BC();
        case 0x0C:
            return INC_C();
        case 0x0D:
            return DEC_C();
        case 0x0E:
            return LD_C_n(ReadN());
        case 0x0F:
            return RRC_A();
        /* Second Row */
        case 0x10:
            return STOP();
        case 0x11:
            return LD_DE_nn(ReadNn());
        case 0x12:
            return LD_Addr_DE_A();
        case 0x13:
            return INC_DE();
        case 0x14:
            return INC_D();
        case 0x15:
            return DEC_D();
        case 0x16:
            return LD_D_n(ReadN());
        case 0x17:
            return RL_A();
        case 0x18:
            return JR_i(ReadI());
        case 0x19:
            return ADD_HL_DE();
        case 0x1A:
            return LD_A_Addr_DE();
        case 0x1B:
            return DEC_DE();
        case 0x1C:
            return INC_E();
        case 0x1D:
            return DEC_E();
        case 0x1E:
            return LD_E_n(ReadN());
        case 0x1F:
            return RR_A();
        /* Third Row */
        case 0x20:
          return JR_NZ_i(ReadI());
        case 0x21:
          return LD_HL_nn(ReadNn());
        case 0x22:
          return LDI_Addr_HL_A();
        case 0x23:
          return INC_HL();
        case 0x24:
            return INC_H();
        case 0x25:
            return DEC_H();
        case 0x26:
            return LD_H_n(ReadN());
        case 0x27:
            return DAA();
        case 0x28:
            return JR_Z_i(ReadI());
        case 0x29:
            return ADD_HL_HL();
        case 0x2A:
            return LDI_A_Addr_HL();
        case 0x2B:
            return DEC_HL();
        case 0x2C:
            return INC_L();
        case 0x2D:
            return DEC_L();
        case 0x2E:
            return LD_L_n(ReadN());
        case 0x2F:
            return CPL();
        /* Fourth Row */
        case 0x30:
            return JR_NC_i(ReadI());
        case 0x31:
            return LD_SP_nn(ReadNn());
        case 0x32:
            return LDD_Addr_HL_A();
        case 0x33:
            return INC_SP();
        case 0x34:
            return INC_Addr_HL();
        case 0x35:
            return DEC_Addr_HL();
        case 0x36:
            return LD_Addr_HL_n(ReadN());
        case 0x37:
            return SCF();
        case 0x38:
            return JR_C_i(ReadI());
        case 0x39:
            return ADD_HL_SP();
        case 0x3A:
            return LDD_A_Addr_HL();
        case 0x3B:
            return DEC_SP();
        case 0x3C:
            return INC_A();
        case 0x3D:
            return DEC_A();
        case 0x3E:
            return LD_A_n(ReadN());
        case 0x3F:
            return CCF();
        /* Second OPCODE Table */
        case PREFIX:
            switch (READ(regs.pc++)) {
                default:
                    printf("Unsupported OPCODE 0x%02x at 0x%04x", READ(regs.pc), regs.pc);
                    std::exit(EXIT_FAILURE);
            }
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
CPU::OPCODE CPU::NOP()
{
    return 1;
}

// load 16-bit immediate into BC
CPU::OPCODE CPU::LD_BC_nn(uint16_t nn)
{
    // instruction has 3 bytes to read -> increase pc by three
    // instruction takes 3 m-cycles (1m cycle constitutes four edges, either rising or falling)
    regs.bc.BC = nn;
    return 3;
}

// write to the address pointed to by register BC with A's value
CPU::OPCODE CPU::LD_Addr_BC_A()
{
    WRITE(regs.bc.BC, regs.af.A);
    return 2;
}

// Increment BC
CPU::OPCODE CPU::INC_BC()
{
    regs.bc.BC++;
    return 2;
}

// Increment B
CPU::OPCODE CPU::INC_B()
{
    regs.bc.B++;
    return 1;
}

// Decrement B
CPU::OPCODE CPU::DEC_B()
{
    regs.bc.B--;
    return 1;
}

// Load 8-bit immediate into B
CPU::OPCODE CPU::LD_B_n(uint8_t n)
{
    regs.bc.B = n;
    return 2;
}

// Rotate A left with carry
CPU::OPCODE CPU::RLC_A() {
    // set the carry flag in the F register if there is a resultant carry
    // 0b10110010 -> 0b00000001 -> (& 0x01u) -> 0x01u
    auto c =  (uint8_t)(regs.af.A >> 7u) & 0x01u; // either 0x00 or 0x01
    regs.af.A = (uint8_t)(regs.af.A << 1u) | c;
    SetFlag(C, c);
    return 1;
}

// Write SP to memory at addresses nn, nn + 1
CPU::OPCODE CPU::LD_Addr_nn_SP(uint16_t nn) {
    // store LSB of SP at address nn
    WRITE(nn, regs.sp & 0xFFu);
    // store MSB of SP at address nn + 1
    WRITE(nn + 1, regs.sp >> 8u);
    return 5;
}

/**
 * Adds HL, BC and puts the result in HL.
 * N is unset, H, C are affected by the result
 * In this operation, H is set if there is a carry-over from 11-12.
 * NOTE: watch out for these half-carry. I was reading online that they can vary per operation.
 */
CPU::OPCODE CPU::ADD_HL_BC() {
    auto nn1 = regs.hl.HL;
    auto nn2 = regs.bc.BC;
    regs.hl.HL = nn1 + nn2;
    SetFlag(N, false); // unset the negative-flag
    // Carry and Half-carry flags are affected
    SetFlag(C, HAS_CARRY(regs.hl.HL, nn1, nn2));
    SetFlag(H, HAS_HALF_CARRY(nn1, nn2));
    return 2;
}

// Load register A with the contents at Address stored in BC
CPU::OPCODE CPU::LD_A_Addr_BC() {
    regs.af.A = READ(regs.bc.BC);
    return 2;
}

// Decrement register BC
CPU::OPCODE CPU::DEC_BC() {
    regs.bc.BC--;
    return 2;
}

// Increment register C
CPU::OPCODE CPU::INC_C() {
    uint8_t n = regs.bc.C;
    regs.bc.C++;
    SetFlag(Z, IS_ZERO_8(regs.bc.C));
    SetFlag(N, false);
    SetFlag(H, HAS_HALF_CARRY_8(n, 0x01u));
    return 1;
}

// Decrement register C
CPU::OPCODE CPU::DEC_C() {
    uint8_t n = regs.bc.C;
    regs.bc.C--;
    SetFlag(Z, IS_ZERO_8(regs.bc.C));
    SetFlag(N, true);
    SetFlag(H, HAS_HALF_CARRY_DECREMENT_8(n));
    return 1;
}

// Load 8-bit immediate into C
CPU::OPCODE CPU::LD_C_n(uint8_t n) {
    regs.bc.C = n;
    return 2;
}

// Rotate Right Carry register A
CPU::OPCODE CPU::RRC_A() {
    auto c = (regs.af.A) & 0x01u; // either 0x01 or 0x00
    SetFlag(C, c);
    regs.af.A = (uint8_t)(regs.af.A >> 1u) | (uint8_t)(c << 7u);
    return 1;
}

// TODO: Look into what this really should do
// An opcode summary I was looking at said it should halt the cpu
CPU::OPCODE CPU::STOP() {
    HALT_FLAG = true;
    return 1;
}

// load 16-bit unsigned immediate into registers DE
CPU::OPCODE CPU::LD_DE_nn(uint16_t nn) {
    regs.de.DE = nn;
    return 3;
}

// load the contents of register A into the address pointed to by register DE
CPU::OPCODE CPU::LD_Addr_DE_A() {
    WRITE(regs.de.DE, regs.af.A);
    return 2;
}

// Increment the registers DE
CPU::OPCODE CPU::INC_DE() {
    regs.de.DE++;
    return 2;
}

// Increment register D
// Z is affected, N is unset, H is affected
CPU::OPCODE CPU::INC_D() {
    uint8_t d = regs.de.D;
    regs.de.D++;
    SetFlag(Z, IS_ZERO_8(regs.de.D));
    SetFlag(N, false);
    SetFlag(H, HAS_HALF_CARRY_8(d, 0x01u));
    return 1;
}

// Decrement the register D
// Z is affected, N is set, H is affected
CPU::OPCODE CPU::DEC_D() {
    uint8_t d = regs.de.D;
    regs.de.D--;
    SetFlag(Z, IS_ZERO_8(regs.de.D));
    return 1;
}

CPU::OPCODE CPU::LD_D_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::RL_A() {
    return 0;
}

CPU::OPCODE CPU::JR_i(int8_t n) {
    return 0;
}

CPU::OPCODE CPU::ADD_HL_DE() {
    return 0;
}

CPU::OPCODE CPU::LD_A_Addr_DE() {
    return 0;
}

CPU::OPCODE CPU::DEC_DE() {
    return 0;
}

CPU::OPCODE CPU::INC_E() {
    return 0;
}

CPU::OPCODE CPU::DEC_E() {
    return 0;
}

CPU::OPCODE CPU::LD_E_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::RR_A() {
    return 0;
}

CPU::OPCODE CPU::JR_NZ_i(int8_t n) {
    return 0;
}

CPU::OPCODE CPU::LD_HL_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::LDI_Addr_HL_A() {
    return 0;
}

CPU::OPCODE CPU::INC_HL() {
    return 0;
}

CPU::OPCODE CPU::INC_H() {
    return 0;
}

CPU::OPCODE CPU::DEC_H() {
    return 0;
}

CPU::OPCODE CPU::LD_H_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::DAA() {
    return 0;
}

CPU::OPCODE CPU::JR_Z_i(int8_t n) {
    return 0;
}

CPU::OPCODE CPU::ADD_HL_HL() {
    return 0;
}

CPU::OPCODE CPU::LDI_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::DEC_HL() {
    return 0;
}

CPU::OPCODE CPU::INC_L() {
    return 0;
}

CPU::OPCODE CPU::DEC_L() {
    return 0;
}

CPU::OPCODE CPU::LD_L_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::CPL() {
    return 0;
}

CPU::OPCODE CPU::JR_NC_i(int8_t n) {
    return 0;
}

CPU::OPCODE CPU::LD_SP_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::LDD_Addr_HL_A() {
    return 0;
}

CPU::OPCODE CPU::INC_SP() {
    return 0;
}

CPU::OPCODE CPU::INC_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::DEC_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_Addr_HL_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::SCF() {
    return 0;
}

CPU::OPCODE CPU::JR_C_i(int8_t n) {
    return 0;
}

CPU::OPCODE CPU::ADD_HL_SP() {
    return 0;
}

CPU::OPCODE CPU::LDD_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::DEC_SP() {
    return 0;
}

CPU::OPCODE CPU::INC_A() {
    return 0;
}

CPU::OPCODE CPU::DEC_A() {
    return 0;
}

CPU::OPCODE CPU::LD_A_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::CCF() {
    return 0;
}

CPU::OPCODE CPU::LD_B_B() {
    return 0;
}

CPU::OPCODE CPU::LB_B_C() {
    return 0;
}

CPU::OPCODE CPU::LD_B_D() {
    return 0;
}

CPU::OPCODE CPU::LD_B_E() {
    return 0;
}

CPU::OPCODE CPU::LD_B_H() {
    return 0;
}

CPU::OPCODE CPU::LD_B_L() {
    return 0;
}

CPU::OPCODE CPU::LD_B_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_B_A() {
    return 0;
}

CPU::OPCODE CPU::LD_C_B() {
    return 0;
}

CPU::OPCODE CPU::LD_C_C() {
    return 0;
}

CPU::OPCODE CPU::LD_C_D() {
    return 0;
}

CPU::OPCODE CPU::LD_C_E() {
    return 0;
}

CPU::OPCODE CPU::LD_C_H() {
    return 0;
}

CPU::OPCODE CPU::LD_C_L() {
    return 0;
}

CPU::OPCODE CPU::LD_C_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_C_A() {
    return 0;
}

CPU::OPCODE CPU::LD_D_B() {
    return 0;
}

CPU::OPCODE CPU::LD_D_C() {
    return 0;
}

CPU::OPCODE CPU::LD_D_D() {
    return 0;
}

CPU::OPCODE CPU::LD_D_E() {
    return 0;
}

CPU::OPCODE CPU::LD_D_H() {
    return 0;
}

CPU::OPCODE CPU::LD_D_L() {
    return 0;
}

CPU::OPCODE CPU::LD_D_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_D_A() {
    return 0;
}

CPU::OPCODE CPU::LD_E_B() {
    return 0;
}

CPU::OPCODE CPU::LD_E_C() {
    return 0;
}

CPU::OPCODE CPU::LD_E_D() {
    return 0;
}

CPU::OPCODE CPU::LD_E_E() {
    return 0;
}

CPU::OPCODE CPU::LD_E_H() {
    return 0;
}

CPU::OPCODE CPU::LD_E_L() {
    return 0;
}

CPU::OPCODE CPU::LD_E_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_E_A() {
    return 0;
}

CPU::OPCODE CPU::LD_H_B() {
    return 0;
}

CPU::OPCODE CPU::LD_H_C() {
    return 0;
}

CPU::OPCODE CPU::LD_H_D() {
    return 0;
}

CPU::OPCODE CPU::LD_H_E() {
    return 0;
}

CPU::OPCODE CPU::LD_H_H() {
    return 0;
}

CPU::OPCODE CPU::LD_H_L() {
    return 0;
}

CPU::OPCODE CPU::LD_H_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_H_A() {
    return 0;
}

CPU::OPCODE CPU::LD_L_B() {
    return 0;
}

CPU::OPCODE CPU::LD_L_C() {
    return 0;
}

CPU::OPCODE CPU::LD_L_D() {
    return 0;
}

CPU::OPCODE CPU::LD_L_E() {
    return 0;
}

CPU::OPCODE CPU::LD_L_H() {
    return 0;
}

CPU::OPCODE CPU::LD_L_L() {
    return 0;
}

CPU::OPCODE CPU::LD_L_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_L_A() {
    return 0;
}

CPU::OPCODE CPU::LD_Addr_HL_B() {
    return 0;
}

CPU::OPCODE CPU::LD_Addr_HL_C() {
    return 0;
}

CPU::OPCODE CPU::LD_Addr_HL_D() {
    return 0;
}

CPU::OPCODE CPU::LD_Addr_HL_E() {
    return 0;
}

CPU::OPCODE CPU::LD_Addr_HL_H() {
    return 0;
}

CPU::OPCODE CPU::LD_Addr_HL_L() {
    return 0;
}

CPU::OPCODE CPU::HALT() {
    return 0;
}

CPU::OPCODE CPU::LD_Addr_HL_A() {
    return 0;
}

CPU::OPCODE CPU::LD_A_B() {
    return 0;
}

CPU::OPCODE CPU::LD_A_C() {
    return 0;
}

CPU::OPCODE CPU::LD_A_D() {
    return 0;
}

CPU::OPCODE CPU::LD_A_E() {
    return 0;
}

CPU::OPCODE CPU::LD_A_H() {
    return 0;
}

CPU::OPCODE CPU::LD_A_L() {
    return 0;
}

CPU::OPCODE CPU::LD_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_A_A() {
    return 0;
}

CPU::OPCODE CPU::ADD_A_B() {
    return 0;
}

CPU::OPCODE CPU::ADD_A_C() {
    return 0;
}

CPU::OPCODE CPU::ADD_A_D() {
    return 0;
}

CPU::OPCODE CPU::ADD_A_E() {
    return 0;
}

CPU::OPCODE CPU::ADD_A_H() {
    return 0;
}

CPU::OPCODE CPU::ADD_A_L() {
    return 0;
}

CPU::OPCODE CPU::ADD_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::ADD_A_A() {
    return 0;
}

CPU::OPCODE CPU::ADC_A_B() {
    return 0;
}

CPU::OPCODE CPU::ADC_A_C() {
    return 0;
}

CPU::OPCODE CPU::ADC_A_D() {
    return 0;
}

CPU::OPCODE CPU::ADC_A_E() {
    return 0;
}

CPU::OPCODE CPU::ADC_A_H() {
    return 0;
}

CPU::OPCODE CPU::ADC_A_L() {
    return 0;
}

CPU::OPCODE CPU::ADC_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::ADC_A_A() {
    return 0;
}

CPU::OPCODE CPU::SUB_A_B() {
    return 0;
}

CPU::OPCODE CPU::SUB_A_C() {
    return 0;
}

CPU::OPCODE CPU::SUB_A_D() {
    return 0;
}

CPU::OPCODE CPU::SUB_A_E() {
    return 0;
}

CPU::OPCODE CPU::SUB_A_H() {
    return 0;
}

CPU::OPCODE CPU::SUB_A_L() {
    return 0;
}

CPU::OPCODE CPU::SUB_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::SUB_A_A() {
    return 0;
}

CPU::OPCODE CPU::SBC_A_B() {
    return 0;
}

CPU::OPCODE CPU::SBC_A_C() {
    return 0;
}

CPU::OPCODE CPU::SBC_A_D() {
    return 0;
}

CPU::OPCODE CPU::SBC_A_E() {
    return 0;
}

CPU::OPCODE CPU::SBC_A_H() {
    return 0;
}

CPU::OPCODE CPU::SBC_A_L() {
    return 0;
}

CPU::OPCODE CPU::SBC_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::SBC_A_A() {
    return 0;
}

CPU::OPCODE CPU::AND_A_B() {
    return 0;
}

CPU::OPCODE CPU::AND_A_C() {
    return 0;
}

CPU::OPCODE CPU::AND_A_D() {
    return 0;
}

CPU::OPCODE CPU::AND_A_E() {
    return 0;
}

CPU::OPCODE CPU::AND_A_H() {
    return 0;
}

CPU::OPCODE CPU::AND_A_L() {
    return 0;
}

CPU::OPCODE CPU::AND_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::AND_A_A() {
    return 0;
}

CPU::OPCODE CPU::XOR_A_B() {
    return 0;
}

CPU::OPCODE CPU::XOR_A_C() {
    return 0;
}

CPU::OPCODE CPU::XOR_A_D() {
    return 0;
}

CPU::OPCODE CPU::XOR_A_E() {
    return 0;
}

CPU::OPCODE CPU::XOR_A_H() {
    return 0;
}

CPU::OPCODE CPU::XOR_A_L() {
    return 0;
}

CPU::OPCODE CPU::XOR_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::XOR_A_A() {
    return 0;
}

CPU::OPCODE CPU::OR_A_B() {
    return 0;
}

CPU::OPCODE CPU::OR_A_C() {
    return 0;
}

CPU::OPCODE CPU::OR_A_D() {
    return 0;
}

CPU::OPCODE CPU::OR_A_E() {
    return 0;
}

CPU::OPCODE CPU::OR_A_H() {
    return 0;
}

CPU::OPCODE CPU::OR_A_L() {
    return 0;
}

CPU::OPCODE CPU::OR_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::OR_A_A() {
    return 0;
}

CPU::OPCODE CPU::CP_A_B() {
    return 0;
}

CPU::OPCODE CPU::CP_A_C() {
    return 0;
}

CPU::OPCODE CPU::CP_A_D() {
    return 0;
}

CPU::OPCODE CPU::CP_A_E() {
    return 0;
}

CPU::OPCODE CPU::CP_A_H() {
    return 0;
}

CPU::OPCODE CPU::CP_A_L() {
    return 0;
}

CPU::OPCODE CPU::CP_A_Addr_HL() {
    return 0;
}

CPU::OPCODE CPU::CP_A_A() {
    return 0;
}

CPU::OPCODE CPU::RET_NZ() {
    return 0;
}

CPU::OPCODE CPU::POP_BC() {
    return 0;
}

CPU::OPCODE CPU::JP_NZ_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::JP_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::CALL_NZ_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::PUSH_BC() {
    return 0;
}

CPU::OPCODE CPU::ADD_A_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::RST_00h() {
    return 0;
}

CPU::OPCODE CPU::RET_Z() {
    return 0;
}

CPU::OPCODE CPU::RET() {
    return 0;
}

CPU::OPCODE CPU::JP_Z_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::CALL_Z_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::CALL_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::ADC_A_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::RST_08h() {
    return 0;
}

CPU::OPCODE CPU::RET_NC() {
    return 0;
}

CPU::OPCODE CPU::POP_DE() {
    return 0;
}

CPU::OPCODE CPU::JP_NC_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::CALL_NC_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::PUSH_DE() {
    return 0;
}

CPU::OPCODE CPU::SUB_A_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::RST_10h() {
    return 0;
}

CPU::OPCODE CPU::RET_C() {
    return 0;
}

CPU::OPCODE CPU::RETI() {
    return 0;
}

CPU::OPCODE CPU::JP_C_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::CALL_C_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::SBC_A_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::RST_18h() {
    return 0;
}

CPU::OPCODE CPU::LD_FF00_n_A(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::POP_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_FF00_C_A() {
    return 0;
}

CPU::OPCODE CPU::PUSH_HL() {
    return 0;
}

CPU::OPCODE CPU::AND_A_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::RST_20h() {
    return 0;
}

CPU::OPCODE CPU::ADD_SP_i(int8_t i) {
    return 0;
}

CPU::OPCODE CPU::JP_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_nn_A(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::XOR_A_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::RST_28h() {
    return 0;
}

CPU::OPCODE CPU::LD_A_FF00_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::POP_AF() {
    return 0;
}

CPU::OPCODE CPU::LD_A_FF00_C() {
    return 0;
}

CPU::OPCODE CPU::DI() {
    return 0;
}

CPU::OPCODE CPU::PUSH_AF() {
    return 0;
}

CPU::OPCODE CPU::OR_A_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::RST_30h() {
    return 0;
}

CPU::OPCODE CPU::LD_HL_SP_i(int8_t i) {
    return 0;
}

CPU::OPCODE CPU::LD_SP_HL() {
    return 0;
}

CPU::OPCODE CPU::LD_A_Addr_nn(uint16_t nn) {
    return 0;
}

CPU::OPCODE CPU::EI() {
    return 0;
}

CPU::OPCODE CPU::CP_A_n(uint8_t n) {
    return 0;
}

CPU::OPCODE CPU::RST_38h() {
    return 0;
}

CPU::OPCODE CPU::LD() {
    return 0;
}

CPU::OPCODE CPU::ADD() {
    return 0;
}

CPU::OPCODE CPU::ADC() {
    return 0;
}
