//
// Created by Sammy Al Hashemi on 2020-02-02.
//

#include "CPU.h"
#include "Bus.h"
#include <_types/_uint16_t.h>
#include <_types/_uint8_t.h>
#include <cstdint>

// This register keeps track if an interrupt condition was met or not
#define INTERRUPT_FLAG_REG 0xFF0F
// This register stores all the interrupts that will be handled, once flagged in INTERRUPT_FLAG_REG
#define INTERRUPT_ENABLE_REG 0xFFFF
// This is a special instruction that tells the cpu to look into a separate OPCODE table.
#define PREFIX 0xCB


// masks
#define BYTE_MSB_MASK 0b10000000u
#define BYTE_LSB_MASK 0b00000001u


// special helper macros
// NOTE: params are (result, firstVal, secondVal)
#define HAS_CARRY(nn, nn1, nn2) ((uint16_t)(((nn) < (nn1))) | (uint16_t)(((nn) < (nn2))))
// NOTE: params are (result, firstVal, secondVal, carry flag)
#define HAS_CARRY_c(nn, nn1, nn2, c) ((uint16_t)(((nn - c) < (nn1))) | (uint16_t)(((nn - c) < (nn2))))
// NOTE: params are (result, firstVal, secondVal)
#define HAS_CARRY_8(n, n1, n2) ((uint8_t)(((n) < (n1))) | (uint8_t)(((n) < (n2))))
// NOTE: params are (result, firstVal, secondVal, carry flag)
#define HAS_CARRY_8_c(n, n1, n2, c) ((uint8_t)(((n - c) < (n1))) | (uint8_t)(((n - c) < (n2))))
// NOTE: params are (firstVal, secondVal)
#define HAS_HALF_CARRY(nn1, nn2) (((((nn1) & 0x0FFFu) + ((nn2) + 0x0FFFu)) > 0x0FFF))
// NOTE: params are (firstVal, secondVal)
#define HAS_HALF_CARRY_8(n1, n2) ((((n1) & 0x0Fu) + ((n2) & 0x0Fu)) > 0x0F)
// NOTE: params are (firstVal, secondVal, carry flag)
#define HAS_HALF_CARRY_8c(n1, n2, c) ((((n1) & 0x0Fu) + ((n2) & 0x0Fu) + c) > 0x0F)
// NOTE: I have to check again whether this is what they mean with the Half carry condition
#define HAS_HALF_CARRY_DECREMENT_8(n) ((((n) & 0x0Fu) == 0x0Fu))
// params are (firstVal, secondVal) where firstVal - secondVal
#define HAS_BORROW_8(n1, n2) (n2 > n1)
#define HAS_BORROW_8c(n1, n2, c) ((n2 > n1) | ((n2 + c) > n1))
// params are (firstVal, secondVal) where firstVal - secondVal
#define HAS_HALF_BORROW_8(n1, n2) ((n2 & 0x0Fu) > (n1 & 0x0Fu))
// NOTE: n1 - n2 - c = n1 - (n2 + c)
// params are (firstVal, secondVal, carry flag)
#define HAS_HALF_BORROW_8c(n1, n2, c) (((n2 & 0x0Fu) + c) > (n1 & 0x0Fu))
#define IS_ZERO_8(n) ((n) == 0)


CPU::CPU() {}

CPU::~CPU()=default;

uint8_t CPU::READ(u_int16_t addr, bool read_only)
{
    // check for range validity occurs within bus implementation
    return bus->READ(addr);
}

void CPU::WRITE(u_int16_t addr, u_int8_t data)
{
    bus->WRITE(addr, data);
}

uint16_t CPU::popFromStack() {
    uint8_t n1 = READ(regs.sp++);
    uint8_t n2 = READ(regs.sp++);
    return (n2 << 8) | n1;
}

void CPU::pushToStack(uint16_t ADDR) {
    // push MSB first
    WRITE(regs.sp--, ADDR >> 8);
    // then push LSB
    WRITE(regs.sp--, ADDR & 0x00FFu);
}

/**
 * Pop register from the stack.
 * Roughly equivalent to the following operations:
 * ld LOW(r16), [sp] ; C, E or L
 * inc sp
 * ld HIGH(r16), [sp] ; B, D or H
 * inc sp
 * No flags are affected
 */
int CPU::POP_REG(uint16_t& REG) {
    uint8_t low = READ(regs.sp) ;
    regs.sp++;
    uint8_t high = READ(regs.sp);
    regs.sp++;
    REG = (high << 8) | low;
    return 3;
}

/**
 * Jump to address nn if condition CC is met.
 * No flags affected
 * TODO: w/wo interrupts: 4/3 cycles
 */
int CPU::JP_CC_n16(Z80_FLAGS FLAG, bool CC, uint16_t nn) {
    if (CC) {
        if (GetFlag(FLAG)) {
            regs.pc = nn;
        }
    } else {
        if (!GetFlag(FLAG)) {
            regs.pc = nn;
        }
    }
    return 3;
}

/**
 * CALL pushes the address of the instruction AFTER CALL on the stack
 * This is so RET can pop it later
 * It then executes an implicit JP nn
 * no flags affected
 * TODO: w/wo interrupts 6/3 cycles
 */
int CPU::CALL_CC_n16(Z80_FLAGS FLAG, bool CC, uint16_t nn) {
    if (CC) {
        if (GetFlag(FLAG)) {
            pushToStack(regs.pc);
            regs.pc = nn;
        }
    } else {
        if (!GetFlag(FLAG)) {
            pushToStack(regs.pc);
            regs.pc = nn;
        }
    }
    return 3;
}


/**
 * Return from suboutine if condition CC for Z80_FLAG FLAG is met
 * Basically POP PC
 * TODO: w/wo interrupts: 5/2
 */
int CPU::RET_CC(Z80_FLAGS FLAG, bool CC) {
    if (CC) {
        if (GetFlag(FLAG)) {
            POP_REG(regs.pc);
        }
    } else {
        if (!GetFlag(FLAG)) {
            POP_REG(regs.pc);
        }
    }
    return 2;
}


/**
 * Push register REG onto the stack
 * This is roughly equivalent to the following instructions:
 * dec sp
 * ld [sp], HIGH(REG) ; B, D, H
 * dec sp
 * ld [sp], LOW(REG) ; C, E, L
 * NOTE: recall that the stack starts at a high memory and grows downwards
 * no flags affected
 * 4 cycles
 */
int CPU::PUSH_REG(uint16_t REG) {
    // load the MSB onto the stack
    WRITE(--regs.sp, REG >> 8u);
    // load the LSB onto the stack
    WRITE(--regs.sp, REG & 0x00FFu);
    return 4;
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

// Z affected, N unset, H affected
void CPU::INCREMENT_8_BIT_REG(uint8_t& reg) {
    // grab the original value
    auto r = reg;
    // Increment the value
    reg++;
    SetFlag(Z, IS_ZERO_8(reg));
    SetFlag(N, false);
    SetFlag(H, HAS_HALF_CARRY_8(r, 0x01u));
}

// Z affected, N set, H affected
void CPU::DECREMENT_8_BIT_REG(uint8_t& reg) {
    reg--;
    SetFlag(Z, IS_ZERO_8(reg));
    SetFlag(N, true);
    SetFlag(H, HAS_HALF_CARRY_DECREMENT_8(reg));
}

/**
 * Adds HL, BC and puts the result in HL.
 * N is unset, H, C are affected by the result
 * In this operation, H is set if there is a carry-over from 11-12.
 * NOTE: watch out for these half-carry. I was reading online that they can
 * vary per operation.
 */
int CPU::ADD_HL_REG(uint16_t REG) {
    auto nn1 = regs.hl.HL;
    auto nn2 = REG;
    regs.hl.HL = nn1 + nn2;
    SetFlag(N, false); // unset the negative-flag
    // Carry and Half-carry flags are affected
    SetFlag(C, HAS_CARRY(regs.hl.HL, nn1, nn2));
    SetFlag(H, HAS_HALF_CARRY(nn1, nn2));
    return 2;
}

int CPU::ADD_A_REG(uint8_t REG) {
    auto n1 = regs.af.A;
    auto n2 = REG;
    regs.af.A = n1 + n2;
    // set Z flag if the result is zero
    SetFlag(Z, IS_ZERO_8(regs.af.A));
    // unset N flag
    SetFlag(N, false);
    // set H if overflow from bit 3
    SetFlag(H, HAS_HALF_CARRY_8(n1, n2));
    // set C if overflow from bit 7
    SetFlag(C, HAS_CARRY_8(regs.af.A, n1, n2));
    return 1;
}

/**
 * Add the value of n8 to A
 * Flags are the same as ADD_A_REG (see above)
 * 2 cycles
 */
int CPU::ADD_A_n8(uint8_t n) {
    auto n1 = regs.af.A;
    auto n2 = n;
    regs.af.A = n1 + n2;
    // set Z flag if the result is zero
    SetFlag(Z, IS_ZERO_8(regs.af.A));
    // unset N flag
    SetFlag(N, false);
    // set H if overflow from bit 3
    SetFlag(H, HAS_HALF_CARRY_8(n1, n2));
    // set C if overflow from bit 7
    SetFlag(C, HAS_CARRY_8(regs.af.A, n1, n2));
    return 2;
}

int CPU::ADD_A_Addr_REG16(uint16_t REG) {
    // returns 2
    return ADD_A_REG(READ(REG)) + 1;
}

int CPU::ADC_A_REG(uint8_t REG) {
    auto c = GetFlag(C);
    auto n1 = regs.af.A;
    auto n2 = REG;
    regs.af.A = n1 + n2 + c;
    // set Z flag if the result is zero
    SetFlag(Z, IS_ZERO_8(regs.af.A));
    // unset N flag
    SetFlag(N, false);
    // set H if overflow from bit 3
    SetFlag(H, HAS_HALF_CARRY_8c(n1, n2, c));
    // set C if overflow from bit 7
    SetFlag(C, HAS_CARRY_8_c(regs.af.A, n1, n2, c));
    return 1;
}

int CPU::ADC_A_n8(uint8_t n) {
    auto c = GetFlag(C);
    auto n1 = regs.af.A;
    auto n2 = n;
    regs.af.A = n1 + n2 + c;
    // set Z flag if the result is zero
    SetFlag(Z, IS_ZERO_8(regs.af.A));
    // unset N flag
    SetFlag(N, false);
    // set H if overflow from bit 3
    SetFlag(H, HAS_HALF_CARRY_8c(n1, n2, c));
    // set C if overflow from bit 7
    SetFlag(C, HAS_CARRY_8_c(regs.af.A, n1, n2, c));
    return 2;
}

int CPU::ADC_A_Addr_REG16(uint16_t REG) {
    // returns 2
    return ADC_A_REG(READ(REG)) + 1;
}

/*
 * Subtracts the value in REG from A
 * and stores the result in A.
 * Affects Z, N is set, H if there is a borrow from bit 4
 * C if there is a borrow (website tells me set if REG > A)
 */
int CPU::SUB_A_REG(uint8_t REG) {
    auto n1 = regs.af.A;
    auto n2 = REG;
    regs.af.A = n1 - n2;
    // Set if A becomes 0
    SetFlag(Z, IS_ZERO_8(regs.af.A));
    // N is set
    SetFlag(N, true);
    // set H if there is a borrow from bit 4
    SetFlag(H, HAS_HALF_BORROW_8(n1, n2));
    // set C is there is a borrow (REG > A)
    // See https://rednex.github.io/rgbds/gbz80.7.html#SUB_A,r8
    SetFlag(C, HAS_BORROW_8(n1, n2));
    return 1;
}


/**
 * Substract the value in REG from A, but DO NOT store the result.
 * This is useful for comparisons.
 * Has the same flags as SUB:
 * Z affected, N set, H if borrow from bit 4, C if borrow (REG > A)
 */
int CPU::CP_A_REG(uint8_t REG) {
    auto n1 = regs.af.A;
    auto n2 = REG;
    auto res = n1 - n2;
    // Set if A becomes 0
    SetFlag(Z, IS_ZERO_8(res));
    // N is set
    SetFlag(N, true);
    // set H if there is a borrow from bit 4
    SetFlag(H, HAS_HALF_BORROW_8(n1, n2));
    // set C is there is a borrow (REG > A)
    // See https://rednex.github.io/rgbds/gbz80.7.html#SUB_A,r8
    SetFlag(C, HAS_BORROW_8(n1, n2));
    return 1;
}

/*
 * Subtracts the value REG points to from A
 * and stores the result in A.
 * Affects Z, N is set, H if there is a borrow from bit 4
 * C if there is a borrow (website tells me set if REG > A)
 */
int CPU::SUB_A_Addr_REG16(uint16_t REG) {
    // returns 2
    return SUB_A_REG(READ(REG)) + 1;
}

/**
 * Substract the value REG points to from A, but DO NOT store the result.
 * This is useful for comparisons.
 * Has the same flags as SUB:
 * Z affected, N set, H if borrow from bit 4, C if borrow (REG > A)
 */
int CPU::CP_A_Addr_REG16(uint16_t REG) {
    // returns 2
    return CP_A_REG(READ(REG)) + 1;
}

/*
 * Subtract the value in REG and the carry flag from A
 * Affects Z, N is set, set H if borrow from bit 4
 * Set C if borrow (site says REG + carry > A)
 * See https://rednex.github.io/rgbds/gbz80.7.html#SBC_A,r8
 */
int CPU::SBC_A_REG(uint8_t REG) {
    auto n1 = regs.af.A;
    auto n2 = REG;
    auto c = GetFlag(C);
    regs.af.A = n1 - n2 - c;
    // Set Z if A is zero
    SetFlag(Z, regs.af.A);
    // Set N
    SetFlag(N, true);
    // Set if borrow from bit 4
    SetFlag(H, HAS_HALF_BORROW_8c(n1, n2, c));
    // Set if borrow (REG + c > A)
    SetFlag(C, HAS_BORROW_8c(n1, n2, c));
    return 1;
}

int CPU::SBC_A_Addr_REG16(uint16_t REG) {
    // returns 2
    return SBC_A_REG(READ(REG)) + 1;
}

/**
 * Bitwise AND between A and the value in REG
 * Store the result back in A
 * Z affected, N unset, H set, C unset
 */
int CPU::AND_A_REG(uint8_t REG) {
    regs.af.A &= REG;
    SetFlag(Z, regs.af.A);
    SetFlag(N, false);
    SetFlag(H, true);
    SetFlag(C, false);
    return 1;
}

/**
 * Bitwise AND between A and the value REG points to
 * Store the result back in A
 * Z affected, N unset, H set, C unset
 */
int CPU::AND_A_Addr_REG16(uint16_t REG) {
    // returns 2
    return AND_A_REG(READ(REG)) + 1;
}

/**
 * Bitwise XOR between A and REG.
 * The result is stored in A.
 * Z affected, rest are unset
 */
int CPU::XOR_A_REG(uint8_t REG) {
    regs.af.A ^= REG;
    // Set Z if result is zero
    SetFlag(Z, IS_ZERO_8(regs.af.A));
    // rest are unset
    SetFlag(N, false);
    SetFlag(H, false);
    SetFlag(C, false);
    return 1;
}

/**
 * Bitwise XOR between A and the value pointed to by REG.
 * Flags are the same as XOR_A_REG.
 */
int CPU::XOR_A_Addr_REG16(uint16_t REG) {
    // returns 2
    return XOR_A_REG(READ(REG)) + 1;
}


/**
 * Bitwise OR between A and REG.
 * The result is stored in A.
 * Z affected, rest are unset
 */
int CPU::OR_A_REG(uint8_t REG) {
    regs.af.A ^= REG;
    // Set Z if result is zero
    SetFlag(Z, IS_ZERO_8(regs.af.A));
    // rest are unset
    SetFlag(N, false);
    SetFlag(H, false);
    SetFlag(C, false);
    return 1;
}

/**
 * Bitwise OR between A and the value pointed to by REG.
 * Flags are the same as XOR_A_REG.
 */
int CPU::OR_A_Addr_REG16(uint16_t REG) {
    // returns 2
    return OR_A_REG(READ(REG)) + 1;
}

/**
 * Call address vec. Shorter/faster equivalent to CALL for suitable values of vec.
 * RST Vectors: 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38
 * No flags affected
 * 4 cycles
 * NOTE: ignore that we are assigning an 8-bit unsigned to a 16-bit one
 * Apparently this is how it is supposed to be.
 * TODO: clarify if the number of cycles is truly supposed to be 4.
 * I saw another resource online claiming it may be 8 (?)
 */
int CPU::RST_VEC(uint8_t VEC) {
    pushToStack(regs.pc);
    regs.pc = VEC;
    return 4;
}

// Load value into register and post-decrement register address
int CPU::LDD_Addr_REG_reg(uint16_t& REG, uint8_t& reg, bool inc) {
    if (inc) {
      WRITE(REG++, reg);
    } else {
      WRITE(REG--, reg);
    }
    return 2;
}

// Load value pointed to by register REG into register register
int CPU::LDD_reg_Addr_REG(uint8_t& reg, uint16_t& REG, bool inc) {
  if (inc) {
    reg = READ(REG++);
  } else {
    reg = READ(REG--);
  }
  return 2;
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
// Z affected, N unset, H affected
CPU::OPCODE CPU::INC_B()
{
    auto b = regs.bc.B;
    regs.bc.B++;
    SetFlag(Z, IS_ZERO_8(regs.bc.B));
    SetFlag(N, false);
    SetFlag(H, HAS_HALF_CARRY_8(b, 0x01u));
    return 1;
}

// Decrement B
// Z affected, N set, H affected
// NOTE: I have to check again whether this is what they mean with the Half carry condition
CPU::OPCODE CPU::DEC_B()
{
    regs.bc.B--;
    SetFlag(Z, IS_ZERO_8(regs.bc.B));
    SetFlag(N, true);
    SetFlag(H, HAS_HALF_CARRY_DECREMENT_8(regs.bc.B));
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
// Z affected, N set, H affected
// NOTE: I have to check again whether this is what they mean with the Half carry condition
CPU::OPCODE CPU::DEC_C() {
    regs.bc.C--;
    SetFlag(Z, IS_ZERO_8(regs.bc.C));
    SetFlag(N, true);
    SetFlag(H, HAS_HALF_CARRY_DECREMENT_8(regs.bc.C));
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
// NOTE: I may not need this anyways
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
    regs.de.D--;
    SetFlag(Z, IS_ZERO_8(regs.de.D));
    SetFlag(N, true);
    SetFlag(H, HAS_HALF_CARRY_DECREMENT_8(regs.de.D));
    return 1;
}

// Load unsigned 8-bit immediate into register D
CPU::OPCODE CPU::LD_D_n(uint8_t n) {
    regs.de.D = n;
    return 2;
}

// Rotate register A left through carry
// C <- [7 <- 0] <- C
// Z, N, H are unset, C is affected
CPU::OPCODE CPU::RL_A() {
    // C = 0; 10000001 -> C = 1; 00000010
    auto c = (regs.af.A & BYTE_MSB_MASK) >> 7u;
    auto oldC = GetFlag(C);
    regs.af.A = (uint8_t)(regs.af.A << 1u) | oldC;
    SetFlag(Z, false);
    SetFlag(N, false);
    SetFlag(H, false);
    SetFlag(C, c);
    return 1;
}

// Jump the PR by a signed 8-bit immediate address
CPU::OPCODE CPU::JR_i(int8_t n) {
    regs.pc += n;
    return 3;
}

// HL = HL + DE
// N unset, H, C affected
CPU::OPCODE CPU::ADD_HL_DE() {
    uint16_t hl = regs.hl.HL;
    uint16_t de = regs.de.DE;
    regs.hl.HL = regs.hl.HL + regs.de.DE;
    SetFlag(N, false);
    SetFlag(H, HAS_HALF_CARRY(hl, de));
    SetFlag(C, HAS_CARRY(hl, regs.hl.HL, regs.de.DE));
    return 2;
}

// Load the data stored at address stored in DE to A
CPU::OPCODE CPU::LD_A_Addr_DE() {
    regs.af.A = READ(regs.de.DE);
    return 2;
}

// Decrement registers DE
// NOTE: no flags are affected according to the OPCODE table I am looking at
CPU::OPCODE CPU::DEC_DE() {
    regs.de.DE--;
    return 2;
}

// Increment register E
// Z affected, N unset, H affected
CPU::OPCODE CPU::INC_E() {
    auto e = regs.de.E;
    regs.de.E++;
    SetFlag(Z, IS_ZERO_8(regs.de.E));
    SetFlag(N, false);
    SetFlag(H, HAS_HALF_CARRY_8(e, 0x01u));
    return 1;
}

// Decrement register E
// Z affected, N set, H affected
// NOTE: I have to check again whether this is what they mean with the Half carry condition
CPU::OPCODE CPU::DEC_E() {
    regs.de.E--;
    SetFlag(Z, IS_ZERO_8(regs.de.E));
    SetFlag(N, true);
    SetFlag(H, HAS_HALF_CARRY_DECREMENT_8(regs.de.E));
    return 1;
}

// Load an unsigned 8-bit immediate into register E
CPU::OPCODE CPU::LD_E_n(uint8_t n) {
    regs.de.E = n;
    return 2;
}

// Rotate register A right through carry
// C -> [7 -> 0] -> C
// Z, N, H unset, C affected
CPU::OPCODE CPU::RR_A() {
    auto c = regs.af.A & BYTE_LSB_MASK;
    auto oldC = GetFlag(C); // either 0x00 or 0x01
    regs.af.A = (uint8_t)(regs.af.A >> 1u) | (uint8_t)(oldC << 7u);
    SetFlag(Z, false);
    SetFlag(N, false);
    SetFlag(H, false);
    SetFlag(C, c);
    return 1;
}

// Relative jump by adding n to to the current address (PC) if the current condition (NZ=Non-Zero) is met
// NOTE: this has different timings based on whether there are branches or not
// TODO: update based on if there is branching
CPU::OPCODE CPU::JR_NZ_i(int8_t n) {
    if (!GetFlag(Z)) {
      regs.pc += n;
    }
    return 2;
}

// Load 16-bit unsigned immediate into register HL
CPU::OPCODE CPU::LD_HL_nn(uint16_t nn) {
    regs.hl.HL = nn;
    return 3;
}

// Load contents of register A into the address that register HL points to
// Also post-increment HL
CPU::OPCODE CPU::LDI_Addr_HL_A() {
    WRITE(regs.hl.HL++, regs.af.A);
    return 2;
}

// Increment register HL
CPU::OPCODE CPU::INC_HL() {
    regs.hl.HL++;
    return 2;
}

// Increment register H
// z affected, N unset, H affected
CPU::OPCODE CPU::INC_H() {
  auto h = regs.hl.H;
  regs.hl.H++;
  SetFlag(Z, IS_ZERO_8(regs.hl.H));
  SetFlag(N, false);
  SetFlag(H, HAS_HALF_CARRY_8(h, 0x01u));
  return 1;
}

// Decrement register H
// Z affected, N set, H affected
CPU::OPCODE CPU::DEC_H() {
    DECREMENT_8_BIT_REG(regs.hl.H);
    return 1;
}

// Load unsigned 8-bit immediate into register H
CPU::OPCODE CPU::LD_H_n(uint8_t n) {
    regs.hl.H = n;
    return 2;
}

// Decimal adjust register A to get correct BCD representation after arithmetic
// instruction. Z affected, H unset, C set or reset depending on the instruction
// (if the previous instruction was addition).
// NOTE: see https://forums.nesdev.com/viewtopic.php?t=15944
// NOTE: found this also (https://github.com/blazer82/gb.teensy/blob/master/lib/CPU/CPU.cpp#L2647)
CPU::OPCODE CPU::DAA() {
    uint8_t nFlag = GetFlag(N); // either 0x00 or 0x01
    uint8_t cFlag = GetFlag(C); // same
    uint8_t hFlag = GetFlag(H); // same
    if (!nFlag) {
      if (cFlag || regs.af.A > 0x99) { regs.af.A += 0x60; SetFlag(C, true); }
      if (hFlag || (regs.af.A & 0x0f) > 0x09) { regs.af.A += 0x06; }
    } else {
      if (cFlag) { regs.af.A += 0x60; }
      if (hFlag) { regs.af.A += 0x06; }
    }
    SetFlag(Z, IS_ZERO_8(regs.af.A));
    SetFlag(H, false);
    return 1;
}

// Relative jump by adding n to the current address (PC) if the Z (Z flag is
// set; is zero) NOTE: this has different timings based on whether there are
// branches or not
// TODO: update based on if there is branching
CPU::OPCODE CPU::JR_Z_i(int8_t n) {
    if (GetFlag(Z)) {
      regs.pc += n;
    }
    return 2;
}

// Add HL to itself and store the result in HL
// N unset, H affected (if overflow from bit 11), C affected (if overflow from bit 15)
CPU::OPCODE CPU::ADD_HL_HL() {
    uint16_t hl = regs.hl.HL; // store initial reg value
    regs.hl.HL = regs.hl.HL + regs.hl.HL;
    SetFlag(N, false);
    SetFlag(H, HAS_HALF_CARRY(hl, hl));
    SetFlag(C, HAS_CARRY(regs.hl.HL, hl, hl));
    return 2;
}

// Load the value stored at address pointed to by register HL into register A and postincrement A
CPU::OPCODE CPU::LDI_A_Addr_HL() {
    // read the value at address (HL) -> postincrement HL
    regs.af.A = READ(regs.hl.HL++);
    return 2;
}

// Decrement the register HL
// No flags affected
CPU::OPCODE CPU::DEC_HL() {
    regs.hl.HL--;
    return 2;
}

// Increment the register L
// Z affected, N unset, H affected
CPU::OPCODE CPU::INC_L() {
    // helper function takes care of this
    INCREMENT_8_BIT_REG(regs.hl.L);
    return 1;
}

// Decrement the register L
// Z affected, N set, H affected
CPU::OPCODE CPU::DEC_L() {
    // helper function takes care of this
    DECREMENT_8_BIT_REG(regs.hl.L);
    return 1;
}

// Load unsigned 8-bit immediate into register L
CPU::OPCODE CPU::LD_L_n(uint8_t n) {
    regs.hl.L = n;
    return 2;
}

// Compliment Accumulator (register A)
// N set, H set
CPU::OPCODE CPU::CPL() {
    regs.af.A = ~regs.af.A;
    SetFlag(N, true);
    SetFlag(H, true);
    return 1;
}

// Relative jump by signed 8-bit immediate to the current adddress
// if not carry
// NOTE: 2 cycles but 3 if branching is enabled
CPU::OPCODE CPU::JR_NC_i(int8_t n) {
    if (!GetFlag(C)) {
      regs.pc += n;
    }
    return 2;
}

// Load unsigned 16-bit immediate into SP
CPU::OPCODE CPU::LD_SP_nn(uint16_t nn) {
    regs.sp = nn;
    return 3;
}

// Load the 8-bit value stored in A to memory pointed to by register HL
CPU::OPCODE CPU::LDD_Addr_HL_A() {
    return LDD_Addr_REG_reg(regs.hl.HL, regs.af.A, false);
}

// Increment the Stack Pointer
CPU::OPCODE CPU::INC_SP() {
    regs.sp++;
    return 2;
}

// Increment the value pointed to by register HL
// Z affected, C unset, H affected
CPU::OPCODE CPU::INC_Addr_HL() {
    // read its value
    uint8_t val = READ(regs.hl.HL);
    // take advantage of this register increment function -> takes care of flags for you
    INCREMENT_8_BIT_REG(val);
    return 3;
}

// Decrement the value pointed to by the register HL
// Z affected, N set, H affected
CPU::OPCODE CPU::DEC_Addr_HL() {
    uint8_t val = READ(regs.hl.HL);
    DECREMENT_8_BIT_REG(val);
    return 3;
}

// Load 8-bit immediate into the address register HL points to
CPU::OPCODE CPU::LD_Addr_HL_n(uint8_t n) {
    WRITE(regs.hl.HL, n);
    return 3;
}

// Set Carry Flag
// N unset, H unset, C set
CPU::OPCODE CPU::SCF() {
    SetFlag(N, false);
    SetFlag(H, false);
    SetFlag(C, true);
    return 1;
}

// Relative jump by signed 8-bit immediate if last operation resulted in a Carry set
// NOTE: if no branching: 2 cycles, else 3
CPU::OPCODE CPU::JR_C_i(int8_t n) {
    if (GetFlag(C)) {
      regs.pc += n;
    }
    return 2;
}

// Add the value stored in SP to HL
CPU::OPCODE CPU::ADD_HL_SP() {
    return ADD_HL_REG(regs.sp);
}

// Load value pointed to by register HL into register A and post decrement HL
CPU::OPCODE CPU::LDD_A_Addr_HL() {
    return LDD_reg_Addr_REG(regs.af.A, regs.hl.HL, false);
}

// Decrement the stack pointer
CPU::OPCODE CPU::DEC_SP() {
    regs.sp--;
    return 2;
}

// Increment register A
// Z affected, N unset, H affected
CPU::OPCODE CPU::INC_A() {
    INCREMENT_8_BIT_REG(regs.af.A);
    return 1;
}

// Decrement register A
// Z affected, N set, H affected
CPU::OPCODE CPU::DEC_A() {
    DECREMENT_8_BIT_REG(regs.af.A);
    return 1;
}

// load unsigned 8-bit immediate into register A
CPU::OPCODE CPU::LD_A_n(uint8_t n) {
    regs.af.A = n;
    return 2;
}

// Complement Carry flag
// N unset, H unset, C complimented
CPU::OPCODE CPU::CCF() {
    SetFlag(C, !GetFlag(C));
    return 1;
}

// load B into B
CPU::OPCODE CPU::LD_B_B() {
    regs.bc.B = regs.bc.B;
    return 1;
}

// load C into B
CPU::OPCODE CPU::LB_B_C() {
    regs.bc.B = regs.bc.C;
    return 1;
}

// load D into B
CPU::OPCODE CPU::LD_B_D() {
    regs.bc.B = regs.de.D;
    return 1;
}

// load E into B
CPU::OPCODE CPU::LD_B_E() {
    regs.bc.B = regs.de.E;
    return 1;
}

// load H into B
CPU::OPCODE CPU::LD_B_H() {
    regs.bc.B = regs.hl.H;
    return 1;
}

// load L into B
CPU::OPCODE CPU::LD_B_L() {
    regs.bc.B = regs.hl.L;
    return 1;
}

// load the value pointed to by reg HL into B
CPU::OPCODE CPU::LD_B_Addr_HL() {
    regs.bc.B = READ(regs.hl.HL);
    return 2;
}

// load A into B
CPU::OPCODE CPU::LD_B_A() {
    regs.bc.B = regs.af.A;
    return 1;
}

// load B into C
CPU::OPCODE CPU::LD_C_B() {
    regs.bc.C = regs.bc.B;
    return 1;
}

// load C into C
CPU::OPCODE CPU::LD_C_C() {
    regs.bc.C = regs.bc.C;
    return 1;
}

// load D into C
CPU::OPCODE CPU::LD_C_D() {
    regs.bc.C = regs.de.D;
    return 1;
}

// load E into C
CPU::OPCODE CPU::LD_C_E() {
    regs.bc.C = regs.de.E;
    return 1;
}

// load H into C
CPU::OPCODE CPU::LD_C_H() {
    regs.bc.C = regs.hl.H;
    return 1;
}

// load L into C
CPU::OPCODE CPU::LD_C_L() {
    regs.bc.C = regs.hl.L;
    return 1;
}

// load the value pointed by the address HL into C
CPU::OPCODE CPU::LD_C_Addr_HL() {
    regs.bc.C = READ(regs.hl.HL);
    return 2;
}

// load A into C
CPU::OPCODE CPU::LD_C_A() {
    regs.bc.C = regs.af.A;
    return 1;
}

// load B into D
CPU::OPCODE CPU::LD_D_B() {
    regs.de.D = regs.bc.B;
    return 1;
}

// load C into D
CPU::OPCODE CPU::LD_D_C() {
    regs.de.D = regs.bc.C;
    return 1;
}

// load D into D
CPU::OPCODE CPU::LD_D_D() {
    regs.de.D = regs.de.D;
    return 1;
}

// load E into D
CPU::OPCODE CPU::LD_D_E() {
    regs.de.D = regs.de.E;
    return 1;
}

// load H into D
CPU::OPCODE CPU::LD_D_H() {
    regs.de.D = regs.hl.H;
    return 1;
}

// load L into D
CPU::OPCODE CPU::LD_D_L() {
    regs.de.D = regs.hl.L;
    return 1;
}

// load the value pointed to by HL into D
CPU::OPCODE CPU::LD_D_Addr_HL() {
    regs.de.D = READ(regs.hl.HL);
    return 2;
}

// load A into D
CPU::OPCODE CPU::LD_D_A() {
    regs.de.D = regs.af.A;
    return 1;
}

// load B into E
CPU::OPCODE CPU::LD_E_B() {
    regs.de.E = regs.bc.B;
    return 1;
}

// load C into E
CPU::OPCODE CPU::LD_E_C() {
    regs.de.E = regs.bc.C;
    return 1;
}

// load D into E
CPU::OPCODE CPU::LD_E_D() {
    regs.de.E = regs.de.D;
    return 1;
}

// load E into E
CPU::OPCODE CPU::LD_E_E() {
    regs.de.E = regs.de.E;
    return 1;
}

// load H into E
CPU::OPCODE CPU::LD_E_H() {
    regs.de.E = regs.hl.H;
    return 1;
}

// load L into E
CPU::OPCODE CPU::LD_E_L() {
    regs.de.E = regs.hl.L;
    return 1;
}

// load the value pointed to by HL into E
CPU::OPCODE CPU::LD_E_Addr_HL() {
    regs.de.E = READ(regs.hl.HL);
    return 2;
}

// load A into E
CPU::OPCODE CPU::LD_E_A() {
    regs.de.E = regs.af.A;
    return 1;
}

// load B into H
CPU::OPCODE CPU::LD_H_B() {
    regs.hl.H = regs.bc.B;
    return 1;
}

// load B into H
CPU::OPCODE CPU::LD_H_C() {
    regs.hl.H = regs.bc.C;
    return 1;
}

// load D into H
CPU::OPCODE CPU::LD_H_D() {
    regs.hl.H = regs.de.D;
    return 1;
}

// load E into H
CPU::OPCODE CPU::LD_H_E() {
    regs.hl.H = regs.de.E;
    return 1;
}

// load H into H
CPU::OPCODE CPU::LD_H_H() {
    regs.hl.H = regs.hl.H;
    return 1;
}

// load L into H
CPU::OPCODE CPU::LD_H_L() {
    regs.hl.H = regs.hl.L;
    return 1;
}

// load the value pointed to by HL into H
CPU::OPCODE CPU::LD_H_Addr_HL() {
    regs.hl.H = READ(regs.hl.HL);
    return 2;
}

// load A into H
CPU::OPCODE CPU::LD_H_A() {
    regs.hl.H = regs.af.A;
    return 1;
}

// load B into L
CPU::OPCODE CPU::LD_L_B() {
    regs.hl.L = regs.bc.B;
    return 1;
}

// load C into L
CPU::OPCODE CPU::LD_L_C() {
    regs.hl.L = regs.bc.C;
    return 1;
}

// load D into L
CPU::OPCODE CPU::LD_L_D() {
    regs.hl.L = regs.de.D;
    return 1;
}

// load E into L
CPU::OPCODE CPU::LD_L_E() {
    regs.hl.L = regs.de.E;
    return 1;
}

// load H into L
CPU::OPCODE CPU::LD_L_H() {
    regs.hl.L = regs.hl.H;
    return 1;
}

// load L into L
CPU::OPCODE CPU::LD_L_L() {
    regs.hl.L = regs.hl.L;
    return 1;
}

// load the value pointed to by HL into L
CPU::OPCODE CPU::LD_L_Addr_HL() {
    regs.hl.L = READ(regs.hl.HL);
    return 2;
}

// load A into L
CPU::OPCODE CPU::LD_L_A() {
    regs.hl.L = regs.af.A;
    return 1;
}

// load B into the location HL points to
CPU::OPCODE CPU::LD_Addr_HL_B() {
    WRITE(regs.hl.HL, regs.bc.B);
    return 2;
}

// load C into the location HL points to
CPU::OPCODE CPU::LD_Addr_HL_C() {
    WRITE(regs.hl.HL, regs.bc.C);
    return 2;
}

// load D into the location HL points to
CPU::OPCODE CPU::LD_Addr_HL_D() {
    WRITE(regs.hl.HL, regs.de.D);
    return 2;
}

// load E into the location HL points to
CPU::OPCODE CPU::LD_Addr_HL_E() {
    WRITE(regs.hl.HL, regs.de.E);
    return 2;
}

// load H into the location HL points to
CPU::OPCODE CPU::LD_Addr_HL_H() {
    WRITE(regs.hl.HL, regs.hl.H);
    return 2;
}

// load L into the location HL points to
CPU::OPCODE CPU::LD_Addr_HL_L() {
    WRITE(regs.hl.HL, regs.hl.L);
    return 2;
}

// toggle the HALT_FLAG to on
// TODO: Check if this is the correct operation
CPU::OPCODE CPU::HALT() {
    HALT_FLAG = true;
    return 1;
}

// load A into the location HL points to
CPU::OPCODE CPU::LD_Addr_HL_A() {
    WRITE(regs.hl.HL, regs.af.A);
    return 2;
}

// load B into A
CPU::OPCODE CPU::LD_A_B() {
    regs.af.A = regs.bc.B;
    return 1;
}

// load C into A
CPU::OPCODE CPU::LD_A_C() {
    regs.af.A = regs.bc.C;
    return 1;
}

// load D into A
CPU::OPCODE CPU::LD_A_D() {
    regs.af.A = regs.de.D;
    return 1;
}

// load E into A
CPU::OPCODE CPU::LD_A_E() {
    regs.af.A = regs.de.E;
    return 1;
}

// load H into A
CPU::OPCODE CPU::LD_A_H() {
    regs.af.A = regs.hl.H;
    return 1;
}

// load L into A
CPU::OPCODE CPU::LD_A_L() {
    regs.af.A = regs.hl.L;
    return 1;
}

// load the value pointed to by HL into A
CPU::OPCODE CPU::LD_A_Addr_HL() {
    regs.af.A = READ(regs.hl.HL);
    return 2;
}

// load A into A
CPU::OPCODE CPU::LD_A_A() {
    regs.af.A = regs.af.A;
    return 1;
}

// add the value in B to A
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADD_A_B() {
    return ADD_A_REG(regs.bc.B);
}


//  add the value in C to A
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADD_A_C() {
    return ADD_A_REG(regs.bc.C);
}


// add the value in D to A
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADD_A_D() {
    return ADD_A_REG(regs.de.D);
}

// add the value in E to A
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADD_A_E() {
    return ADD_A_REG(regs.de.E);
}

// add the value in H to A
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADD_A_H() {
    return ADD_A_REG(regs.hl.H);
}

// add the value in L to A
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADD_A_L() {
    return ADD_A_REG(regs.hl.L);
}

// add the value pointed to by HL to A
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADD_A_Addr_HL() {
    return ADD_A_Addr_REG16(regs.hl.HL);
}

// add the value in A to A
// Z affected N unset, H affected, C affected
CPU::OPCODE CPU::ADD_A_A() {
    return ADD_A_REG(regs.af.A);
}

// add the value in B to A plus carry flag
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADC_A_B() {
    return ADC_A_REG(regs.bc.B);
}

// add the value in C to A plus carry flag
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADC_A_C() {
    return ADC_A_REG(regs.bc.C);
}

// add the value in D to A plus carry flag
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADC_A_D() {
    return ADC_A_REG(regs.de.D);
}

// add the value in E to A plus carry flag
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADC_A_E() {
    return ADC_A_REG(regs.de.E);
}

// add the value in H to A plus carry flag
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADC_A_H() {
    return ADC_A_REG(regs.hl.H);
}

// add the value in L to A plus carry flag
//  Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADC_A_L() {
    return ADC_A_REG(regs.hl.L);
}

// add the value pointed to by HL to A plus carry flag
// Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADC_A_Addr_HL() {
    return ADC_A_Addr_REG16(regs.hl.HL);
}

// add the value in A to A plus carry flag
//  Z affected, N unset, H affected, C affected
CPU::OPCODE CPU::ADC_A_A() {
    return ADC_A_REG(regs.af.A);
}

// sub the value in B from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SUB_A_B() {
    return SUB_A_REG(regs.bc.B);
}

// sub the value in C from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SUB_A_C() {
    return SUB_A_REG(regs.bc.C);
}

// sub the value in D from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SUB_A_D() {
    return SUB_A_REG(regs.de.D);
}

// sub the value in E from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SUB_A_E() {
    return SUB_A_REG(regs.de.E);
}

// sub the value in H from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SUB_A_H() {
    return SUB_A_REG(regs.hl.H);
}

// sub the value in L from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SUB_A_L() {
    return SUB_A_REG(regs.hl.L);
}

// sub the value pointed to by HL from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SUB_A_Addr_HL() {
    return SUB_A_Addr_REG16(regs.hl.HL);
}

// sub the value in A from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SUB_A_A() {
    return SUB_A_REG(regs.af.A);
}

// sub the value in B and the carry flag from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SBC_A_B() {
    return SBC_A_REG(regs.bc.B);
}

// sub the value in C and the carry flag from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SBC_A_C() {
    return SBC_A_REG(regs.bc.C);
}

// sub the value in D and the carry flag from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SBC_A_D() {
    return SBC_A_REG(regs.de.D);
}

// sub the value in E and the carry flag from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SBC_A_E() {
    return SBC_A_REG(regs.de.E);
}

// sub the value in H and the carry flag from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SBC_A_H() {
    return SBC_A_REG(regs.hl.H);
}

// sub the value in L and the carry flag from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SBC_A_L() {
    return SBC_A_REG(regs.hl.L);
}

// sub the value pointed to by HL and the carry flag from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SBC_A_Addr_HL() {
    return SBC_A_Addr_REG16(regs.hl.HL);
}

// sub the value in A and the carry flag from A
// Z affected, N set, H affected, C affected
CPU::OPCODE CPU::SBC_A_A() {
    return SBC_A_REG(regs.af.A);
}

/*
 * Bitwise AND betwen A and B
 * Z affected, N unset, H set, C unset
 */
CPU::OPCODE CPU::AND_A_B() {
    return AND_A_REG(regs.bc.B);
}

// Bitwise AND between A and C
// Z affaceted, N unset, H set, C unset
CPU::OPCODE CPU::AND_A_C() {
    return AND_A_REG(regs.bc.C);
}

// Bitwise AND between A and D
// Z affected, N unset, H set, C unset
CPU::OPCODE CPU::AND_A_D() {
    return AND_A_REG(regs.de.D);
}

//  Bitwise AND between A and E
// Z affected, N unset, H set, C unset
CPU::OPCODE CPU::AND_A_E() {
    return AND_A_REG(regs.de.E);
}

//  Bitwise AND between A and H
// Z affected, N unset, H set, C unset
CPU::OPCODE CPU::AND_A_H() {
    return AND_A_REG(regs.hl.H);
}

//  Bitwise AND between A and L
// Z affected, N unset, H set, C unset
CPU::OPCODE CPU::AND_A_L() {
    return AND_A_REG(regs.hl.L);
}

//  Bitwise AND between A and the value pointed to by HL
// Z affected, N unset, H set, C unset
CPU::OPCODE CPU::AND_A_Addr_HL() {
    return AND_A_Addr_REG16(regs.hl.HL);
}

//  Bitwise AND between A and A
// Z affected, N unset, H set, C unset
CPU::OPCODE CPU::AND_A_A() {
    return AND_A_REG(regs.af.A);
}

// Bitwise XOR between A and B
// Z affected, rest unset
CPU::OPCODE CPU::XOR_A_B() {
    return XOR_A_REG(regs.bc.B);
}

// Bitwise XOR between A and C
// Z affected, rest unset
CPU::OPCODE CPU::XOR_A_C() {
    return XOR_A_REG(regs.bc.C);
}

// Bitwise XOR between A and D
// Z affected, rest unset
CPU::OPCODE CPU::XOR_A_D() {
    return XOR_A_REG(regs.de.D);
}

// Bitwise XOR between A and E
// Z affected, rest unset
CPU::OPCODE CPU::XOR_A_E() {
    return XOR_A_REG(regs.de.E);
}

// Bitwise XOR between A and H
// Z affected, rest unset
CPU::OPCODE CPU::XOR_A_H() {
    return XOR_A_REG(regs.hl.H);
}

// Bitwise XOR between A and L
// Z affected, rest unset
CPU::OPCODE CPU::XOR_A_L() {
    return XOR_A_REG(regs.hl.L);
}

// Bitwise XOR between A and the value pointed to by HL
// Z affected, rest unset
CPU::OPCODE CPU::XOR_A_Addr_HL() {
    return XOR_A_Addr_REG16(regs.hl.HL);
}

// Bitwise XOR between A and A
// Z affected, rest unset
CPU::OPCODE CPU::XOR_A_A() {
    return XOR_A_REG(regs.af.A);
}

// Bitwise OR between A and B
// Z affected, rest unset
CPU::OPCODE CPU::OR_A_B() {
    return OR_A_REG(regs.bc.B);
}

// Bitwise OR between A and C
// Z affected, rest unset
CPU::OPCODE CPU::OR_A_C() {
    return OR_A_REG(regs.bc.C);
}

// Bitwise OR between A and D
// Z affected, rest unset
CPU::OPCODE CPU::OR_A_D() {
    return OR_A_REG(regs.de.D);
}

// Bitwise OR between A and E
// Z affected, rest unset
CPU::OPCODE CPU::OR_A_E() {
    return OR_A_REG(regs.de.E);
}

// Bitwise OR between A and H
// Z affected, rest unset
CPU::OPCODE CPU::OR_A_H() {
    return OR_A_REG(regs.hl.H);
}

// Bitwise OR between A and L
// Z affected, rest unset
CPU::OPCODE CPU::OR_A_L() {
    return OR_A_REG(regs.hl.L);
}

// Bitwise OR between A and value pointed to by HL
// Z affected, rest unset
CPU::OPCODE CPU::OR_A_Addr_HL() {
    return OR_A_Addr_REG16(regs.hl.HL);
}

// Bitwise OR between A and A
// Z affected, rest unset
CPU::OPCODE CPU::OR_A_A() {
    return OR_A_REG(regs.af.A);
}

// substract the value in B from A
// do not store the result
// Z affected, N set, H if borrow from bit 4
// C affected if borrow (REG > A)
CPU::OPCODE CPU::CP_A_B() {
    return CP_A_REG(regs.bc.B);
}

// substract the value in C from A
// do not store the result
// Z affected, N set, H if borrow from bit 4
// C affected if borrow (REG > A)
CPU::OPCODE CPU::CP_A_C() {
    return CP_A_REG(regs.bc.C);
}

// substract the value in D from A
// do not store the result
// Z affected, N set, H if borrow from bit 4
// C affected if borrow (REG > A)
CPU::OPCODE CPU::CP_A_D() {
    return CP_A_REG(regs.de.D);
}

// substract the value in E from A
// do not store the result
// Z affected, N set, H if borrow from bit 4
// C affected if borrow (REG > A)
CPU::OPCODE CPU::CP_A_E() {
    return CP_A_REG(regs.de.E);
}

// substract the value in H from A
// do not store the result
// Z affected, N set, H if borrow from bit 4
// C affected if borrow (REG > A)
CPU::OPCODE CPU::CP_A_H() {
    return CP_A_REG(regs.hl.H);
}

// substract the value in L from A
// do not store the result
// Z affected, N set, H if borrow from bit 4
// C affected if borrow (REG > A)
CPU::OPCODE CPU::CP_A_L() {
    return CP_A_REG(regs.hl.L);
}

// substract the value stored in HL from A
// do not store the result
// Z affected, N set, H if borrow from bit 4
// C affected if borrow (REG > A)
CPU::OPCODE CPU::CP_A_Addr_HL() {
    return CP_A_Addr_REG16(regs.hl.HL);
}


// substract the value in A from A
// do not store the result
// Z affected, N set, H if borrow from bit 4
// C affected if borrow (REG > A)
CPU::OPCODE CPU::CP_A_A() {
    return CP_A_REG(regs.af.A);
}

// Pop the next instruction from the stack if the last result was NOT ZERO
// TODO: When you get to interrupts, you have to update the amount of cycles returned to reflect
// that
CPU::OPCODE CPU::RET_NZ() {
    if (!GetFlag(Z)) {
        regs.pc = popFromStack();
    }
    return 2;
}

// Pop from the stack onto BC
// No flags are modified
CPU::OPCODE CPU::POP_BC() {
    return POP_REG(regs.bc.BC);
}

// Jump to address nn if NOT ZERO
// No flags affected
// TODO: w/wo interrupts 4/3 cycles
CPU::OPCODE CPU::JP_NZ_nn(uint16_t nn) {
    return JP_CC_n16(Z, false, nn);
}

// Jump to address nn
// no flags affected
// 4 cycles
CPU::OPCODE CPU::JP_nn(uint16_t nn) {
    regs.pc = nn;
    return 4;
}

// CALL pushes the address of the instruction AFTER CALL on the stack
// This is so RET can pop it later
// it then executes an implicit JP nn
// Call address nn if NOT ZERO
// no flags affected
// TODO: w/wo interrupts: 6/3 cycles
CPU::OPCODE CPU::CALL_NZ_nn(uint16_t nn) {
    return CALL_CC_n16(Z, false, nn);
}

// load the register BC onto the stack
CPU::OPCODE CPU::PUSH_BC() {
    return PUSH_REG(regs.bc.BC);
}

// Adds the value n to A
// Same flags affected as ADD_A_REG
CPU::OPCODE CPU::ADD_A_n(uint8_t n) {
    return ADD_A_n8(n);
}

// Call address 0x00
CPU::OPCODE CPU::RST_00h() {
    return RST_VEC(0x00u);
}

// Return from subroutine IF ZERO
// Basically POP PC
// TODO: w/wo interrupts 5/2
CPU::OPCODE CPU::RET_Z() {
    return RET_CC(Z, true);
}

// Return from subroutine
// Basically POP PC
// 4 cycles
CPU::OPCODE CPU::RET() {
    // returns 4 => 3 + 1
    return POP_REG(regs.pc) + 1;
}

// Jump to address nn IF ZERO
// TODO: w/wo interrupts 4/3 cycles
CPU::OPCODE CPU::JP_Z_nn(uint16_t nn) {
    return JP_CC_n16(Z, true, nn);
}

// CALL pushes the address of the instruction AFTER CALL on the stack
// This is so RET can pop it later
// it then executes an implicit JP nn
// Call address nn IF ZERO
// no flags affected
// TODO: w/wo interrupts: 6/3 cycles
CPU::OPCODE CPU::CALL_Z_nn(uint16_t nn) {
    return CALL_CC_n16(Z, true, nn);
}

// CALL pushes the address of the instruction AFTER CALL on the stack
// This is so RET can pop it later
// It then executes an implicit JP nn
// 6 cycles
CPU::OPCODE CPU::CALL_nn(uint16_t nn) {
    pushToStack(regs.pc);
    regs.pc = nn;
    return 6;
}

// Add the value n plus the carry flag to A
// 2 cycles
// Same flags as ADD_A_REG
CPU::OPCODE CPU::ADC_A_n(uint8_t n) {
    return ADC_A_n(n);
}

// Call address 0x08
CPU::OPCODE CPU::RST_08h() {
    return RST_VEC(0x08u);
}

// Return from subroutine IF CARRY IS NOT SET
// Basically POP PC
// TODO: w/wo interrupts 5/2
CPU::OPCODE CPU::RET_NC() {
    return RET_CC(C, false);
}

// Pop from the stack onto DE
// No flags are modified
CPU::OPCODE CPU::POP_DE() {
    return POP_REG(regs.de.DE);
}

// Jump to address nn if NOT CARRY
// No flags affected
CPU::OPCODE CPU::JP_NC_nn(uint16_t nn) {
    return JP_CC_n16(C, false, nn);
}

// CALL address nn if NOT CARRY
// executes an implicit JP nn
// no flags affected
CPU::OPCODE CPU::CALL_NC_nn(uint16_t nn) {
    return CALL_CC_n16(C, false, nn);
}

// load the register DE onto the stack
// no flags affected
CPU::OPCODE CPU::PUSH_DE() {
    return PUSH_REG(regs.de.DE);
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
