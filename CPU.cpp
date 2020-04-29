//
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
    return (uint8_t)(n2 << 8u) | n1;
}

void CPU::pushToStack(uint16_t ADDR) {
    // push MSB first
    WRITE(--regs.sp, ADDR >> 8u);
    // then push LSB
    WRITE(--regs.sp, ADDR & 0x00FFu);
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
    REG = (uint8_t)(high << 8u) | low;
    return 3;
}

/**
 * Jump to address nn if condition CC is met.
 * No flags affected
 * TODO: w/wo interrupts: 4/3 cycles
 */
int CPU::JP_CC_n16(Z80_FLAGS FLAG, bool CC, uint16_t nn) {
    // CC = true; GetFlag should be 1
    // CC = false; GetFlag should be 0
    if (CC == GetFlag(FLAG)) {
        regs.pc = nn;
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
    if (CC == GetFlag(FLAG)) {
        pushToStack(regs.pc);
        regs.pc = nn;
    }
    return 3;
}


/**
 * Return from subroutine if condition CC for Z80_FLAG FLAG is met
 * Basically POP PC
 * TODO: w/wo interrupts: 5/2
 */
int CPU::RET_CC(Z80_FLAGS FLAG, bool CC) {
    if (CC == GetFlag(FLAG)) {
        POP_REG(regs.pc);
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

uint8_t CPU::GetFlag(CPU::Z80_FLAGS f) const {
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

/**
 * Add the signed value i to SP
 * Z unset, N unset, H if overflow from bit 3, C if overflow from bit 7
 * 4 cycles
 * NOTE: int8_t negative types are represented by 2s complement.
 * THESE ARE STILL 8-BIT NUMBERS.
 * In addition, look at the definition of HAS_HALF_CARRY_8,
 * IT ONLY LOOKS AT THE LSB OF THE 8-BIT NUMBER.
 * IF THERE IS A HALF-CARRY IS THE SAME FOR BOTH SIGNED AND UNSIGNED
 * SO THE OPERATION BELOW IS SAFE.
 * Just for clarity of thought: check out https://repl.it/@selhash/LimeRubberyZip
 * @param i
 * @return
 */
int CPU::ADD_SP_i8(int8_t i) {
    auto n1 = regs.sp;
    auto n2 = i;
    regs.sp = n1 + n2;
    // set Z flag if the result is zero
    SetFlag(Z, false);
    // unset N flag
    SetFlag(N, false);
    // set H if overflow from bit 3
    SetFlag(H, HAS_HALF_CARRY_8(n1, (uint8_t) n2));
    // set C if overflow from bit 7
    SetFlag(C, HAS_CARRY_8(regs.sp, n1, n2));
    return 4;
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
 * Subtract n from A.
 * Same flags as SUB_A_REG.
 * 2 cycles
 */
int CPU::SUB_A_n8(uint8_t n) {
    auto n1 = regs.af.A;
    auto n2 = n;
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
    return 2;
}


/**
 * Subtract the value in REG from A, but DO NOT store the result.
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

/**
 * Subtract the value n and the carry flag from A
 * Same flags as SBC_A_REG
 * 2 cycles
 * @param n
 * @return int
 */
int CPU::SBC_A_n8(uint8_t n) {
    auto n1 = regs.af.A;
    auto n2 = n;
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
    return 2;
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
 * Bitwise AND between the value in n8 and A
 * Same flags as ANA_A_REG
 * 2 cycles
 * @param n
 * @return
 */
int CPU::AND_A_n8(uint8_t n) {
    regs.af.A &= n;
    SetFlag(Z, regs.af.A);
    SetFlag(N, false);
    SetFlag(H, true);
    SetFlag(C, false);
    return 2;
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
            /* Fifth Row*/
        case 0x40:
            return LD_B_B();
        case 0x41:
            return LD_B_C();
        case 0x42:
            return LD_B_D();
        case 0x43:
            return LD_B_E();
        case 0x44:
            return LD_B_H();
        case 0x45:
            return LD_B_L();
        case 0x46:
            return LD_B_Addr_HL();
        case 0x47:
            return LD_B_A();
        case 0x48:
            return LD_C_B();
        case 0x49:
            return LD_C_C();
        case 0x4A:
            return LD_C_D();
        case 0x4B:
            return LD_C_E();
        case 0x4C:
            return LD_C_H();
        case 0x4D:
            return LD_C_L();
        case 0x4E:
            return LD_C_Addr_HL();
        case 0x4F:
            return LD_C_A();
            /* Sixth Row*/
        case 0x50:
            return LD_D_B();
        case 0x51:
            return LD_D_C();
        case 0x52:
            return LD_D_D();
        case 0x53:
            return LD_D_E();
        case 0x54:
            return LD_D_H();
        case 0x55:
            return LD_D_L();
        case 0x56:
            return LD_D_Addr_HL();
        case 0x57:
            return LD_D_A();
        case 0x58:
            return LD_E_B();
        case 0x59:
            return LD_E_C();
        case 0x5A:
            return LD_E_D();
        case 0x5B:
            return LD_E_E();
        case 0x5C:
            return LD_E_H();
        case 0x5D:
            return LD_E_L();
        case 0x5E:
            return LD_E_Addr_HL();
        case 0x5F:
            return LD_E_A();
            /* Seventh Row*/
        case 0x60:
            return LD_H_B();
        case 0x61:
            return LD_H_C();
        case 0x62:
            return LD_H_D();
        case 0x63:
            return LD_H_C();
        case 0x64:
            return LD_H_H();
        case 0x65:
            return LD_H_L();
        case 0x66:
            return LD_H_Addr_HL();
        case 0x67:
            return LD_H_A();
        case 0x68:
            return LD_L_B();
        case 0x69:
            return LD_L_C();
        case 0x6A:
            return LD_L_D();
        case 0x6B:
            return LD_L_E();
        case 0x6C:
            return LD_L_H();
        case 0x6D:
            return LD_L_L();
        case 0x6E:
            return LD_L_Addr_HL();
        case 0x6F:
            return LD_L_A();
            /* Eigth Row*/
        case 0x70:
            return LD_Addr_HL_B();
        case 0x71:
            return LD_Addr_HL_C();
        case 0x72:
            return LD_Addr_HL_D();
        case 0x73:
            return LD_Addr_HL_E();
        case 0x74:
            return LD_Addr_HL_H();
        case 0x75:
            return LD_Addr_HL_L();
        case 0x76:
            return HALT();
        case 0x77:
            return LD_Addr_HL_A();
        case 0x78:
            return LD_A_B();
        case 0x79:
            return LD_A_C();
        case 0x7A:
            return LD_A_D();
        case 0x7B:
            return LD_A_E();
        case 0x7C:
            return LD_A_H();
        case 0x7D:
            return LD_A_L();
        case 0x7E:
            return LD_A_Addr_HL();
        case 0x7F:
            return LD_A_A();
            /* Ninth Row*/
        case 0x80:
            return ADD_A_B();
        case 0x81:
            return ADD_A_C();
        case 0x82:
            return ADD_A_D();
        case 0x83:
            return ADD_A_E();
        case 0x84:
            return ADD_A_H();
        case 0x85:
            return ADD_A_L();
        case 0x86:
            return ADD_A_Addr_HL();
        case 0x87:
            return ADD_A_A();
        case 0x88:
            return ADC_A_B();
        case 0x89:
            return ADC_A_C();
        case 0x8A:
            return ADC_A_D();
        case 0x8B:
            return ADC_A_E();
        case 0x8C:
            return ADC_A_H();
        case 0x8D:
            return ADC_A_L();
        case 0x8E:
            return ADC_A_Addr_HL();
        case 0x8F:
            return ADC_A_A();
            /* Tenth Row*/
        case 0x90:
            return SUB_A_B();
        case 0x91:
            return SUB_A_C();
        case 0x92:
            return SUB_A_D();
        case 0x93:
            return SUB_A_E();
        case 0x94:
            return SUB_A_H();
        case 0x95:
            return SUB_A_L();
        case 0x96:
            return SUB_A_Addr_HL();
        case 0x97:
            return SUB_A_A();
        case 0x98:
            return SBC_A_B();
        case 0x99:
            return SBC_A_C();
        case 0x9A:
            return SBC_A_D();
        case 0x9B:
            return SBC_A_E();
        case 0x9C:
            return SBC_A_H();
        case 0x9D:
            return SBC_A_L();
        case 0x9E:
            return SBC_A_Addr_HL();
        case 0x9F:
            return SBC_A_A();
            /* Eleventh Row*/
        case 0xA0:
            return AND_A_B();
        case 0xA1:
            return AND_A_C();
        case 0xA2:
            return AND_A_D();
        case 0xA3:
            return AND_A_E();
        case 0xA4:
            return AND_A_H();
        case 0xA5:
            return AND_A_L();
        case 0xA6:
            return AND_A_Addr_HL();
        case 0xA7:
            return AND_A_A();
        case 0xA8:
            return XOR_A_B();
        case 0xA9:
            return XOR_A_C();
        case 0xAA:
            return XOR_A_D();
        case 0xAB:
            return XOR_A_E();
        case 0xAC:
            return XOR_A_H();
        case 0xAD:
            return XOR_A_L();
        case 0xAE:
            return XOR_A_Addr_HL();
        case 0xAF:
            return XOR_A_A();
            /* Twelfth Row*/
        case 0xB0:
            return OR_A_B();
        case 0xB1:
            return OR_A_C();
        case 0xB2:
            return OR_A_D();
        case 0xB3:
            return OR_A_E();
        case 0xB4:
            return OR_A_H();
        case 0xB5:
            return OR_A_L();
        case 0xB6:
            return OR_A_Addr_HL();
        case 0xB7:
            return OR_A_A();
        case 0xB8:
            return CP_A_B();
        case 0xB9:
            return CP_A_C();
        case 0xBA:
            return CP_A_D();
        case 0xBB:
            return CP_A_E();
        case 0xBC:
            return CP_A_H();
        case 0xBD:
            return CP_A_L();
        case 0xBE:
            return CP_A_Addr_HL();
        case 0xBF:
            return CP_A_A();
            /* Thirteenth Row*/
        case 0xC0:
            return RET_NZ();
        case 0xC1:
            return POP_BC();
        case 0xC2:
            return JP_NZ_nn(ReadNn());
        case 0xC3:
            return JP_nn(ReadNn());
        case 0xC4:
            return CALL_NZ_nn(ReadNn());
        case 0xC5:
            return PUSH_BC();
        case 0xC6:
            return ADD_A_n(ReadN());
        case 0xC7:
            return RST_00h();
        case 0xC8:
            return RET_Z();
        case 0xC9:
            return RET();
        case 0xCA:
            return JP_Z_nn(ReadNn()); 
        //No 0xCB mapping 
        case 0xCC:
            return CALL_Z_nn(ReadNn());
        case 0xCD:
            return CALL_nn(ReadNn());
        case 0xCE:
            return ADC_A_n(ReadN());
        case 0xCF:
            return RST_08h();
            /* Fourteenth Row*/
        case 0xD0:
            return RET_NC();
        case 0xD1:
            return POP_DE();
        case 0xD2:
            return JP_NC_nn(ReadNn());
        // No 0xD3 mapping
        case 0xD4:
            return CALL_NC_nn(ReadNn());
        case 0xD5:
            return PUSH_DE();
        case 0xD6:
            return SUB_A_n(ReadN());
        case 0xD7:
            return RST_10h();
        case 0xD8:
            return RET_C();
        case 0xD9:
            return RETI();
        case 0xDA:
            return JP_C_nn(ReadNn());
        // No 0xDB mapping
        case 0xDC:
            return CALL_C_nn(ReadNn());
        // No 0xDB mapping
        case 0xDE:
            return SBC_A_n(ReadN());
        case 0xDF:
            return RST_18h();
            /* Fifteenth Row*/
        case 0xE0:
            return LD_FF00_n_A(ReadN());
        case 0xE1:
            return POP_HL();
        case 0xE2:
            return LD_FF00_C_A();
        // No 0xD3 mapping
        // No 0xD4 mapping
        case 0xE5:
            return PUSH_HL();
        case 0xE6:
            return AND_A_n(ReadN());
        case 0xE7:
            return RST_20h();
        case 0xE8:
            return ADD_SP_i(ReadI());
        case 0xE9:
            return JP_HL();
        case 0xEA:
            return LD_nn_A(ReadNn());
       // No 0xDB mapping
       // No 0xDC mapping
       // No 0xDD mapping
        case 0xEE:
            return XOR_A_n(ReadN());
        case 0xEF:
            return RST_28h();
            /* Sixteenth Row*/
        case 0xF0:
            return LD_A_FF00_n(ReadN());
        case 0xF1:
            return POP_AF();
        case 0xF2:
            return LD_A_FF00_C(ReadN());
        case 0xF3:
            return DI();
        // No 0xF4 mapping
        case 0xF5:
            return PUSH_AF();
        case 0xF6:
            return OR_A_n(ReadN());
        case 0xF7:
            return RST_30h();
        case 0xF8:
            return LD_HL_SP_i(ReadI());
        case 0xF9:
            return LD_SP_HL();
        case 0xFA:
            return LD_A_Addr_nn(ReadNn());
        case 0xFB:
            return EI();
        // No 0xFC mapping
        // No 0xFD mapping
        case 0xFE:
            return CP_A_n(ReadN());
        case 0xFF:
            return RST_38h();
        /* Second OPCODE Table */
        case PREFIX:
            switch (READ(regs.pc++)) {
                    /* First Row*/
                case 0x00:
                    return p();
                case 0x01:
                    return p();
                case 0x02:
                    return p();
                case 0x03:
                    return p();
                case 0x04:
                    return p();
                case 0x05:
                    return p();
                case 0x06:
                    return p();
                case 0x07:
                    return p();
                case 0x08:
                    return p();
                case 0x09:
                    return p();
                case 0x0A:
                    return p();
                case 0x0B:
                    return p();
                case 0x0C:
                    return p();
                case 0x0D:
                    return p();
                case 0x0E:
                    return p();
                case 0x0F:
                    return p();
                    /* Second Row*/
                case 0x10:
                    return p();
                case 0x11:
                    return p();
                case 0x12:
                    return p();
                case 0x13:
                    return p();
                case 0x14:
                    return p();
                case 0x15:
                    return p();
                case 0x16:
                    return p();
                case 0x17:
                    return p();
                case 0x18:
                    return p();
                case 0x19:
                    return p();
                case 0x1A:
                    return p();
                case 0x1B:
                    return p();
                case 0x1C:
                    return p();
                case 0x1D:
                    return p();
                case 0x1E:
                    return p();
                case 0x1F:
                    return p();
                    /* Third Row*/
                case 0x20:
                    return p();
                case 0x21:
                    return p();
                case 0x22:
                    return p();
                case 0x23:
                    return p();
                case 0x24:
                    return p();
                case 0x25:
                    return p();
                case 0x26:
                    return p();
                case 0x27:
                    return p();
                case 0x28:
                    return p();
                case 0x29:
                    return p();
                case 0x2A:
                    return p();
                case 0x2B:
                    return p();
                case 0x2C:
                    return p();
                case 0x2D:
                    return p();
                case 0x2E:
                    return p();
                case 0x2F:
                    return p();
                    /* Fourth Row*/
                case 0x30:
                    return p();
                case 0x31:
                    return p();
                case 0x32:
                    return p();
                case 0x33:
                    return p();
                case 0x34:
                    return p();
                case 0x35:
                    return p();
                case 0x36:
                    return p();
                case 0x37:
                    return p();
                case 0x38:
                    return p();
                case 0x39:
                    return p();
                case 0x3A:
                    return p();
                case 0x3B:
                    return p();
                case 0x3C:
                    return p();
                case 0x3D:
                    return p();
                case 0x3E:
                    return p();
                case 0x3F:
                    return p();
                    /* Fifth Row*/
                case 0x40:
                    return p();
                case 0x41:
                    return p();
                case 0x42:
                    return p();
                case 0x43:
                    return p();
                case 0x44:
                    return p();
                case 0x45:
                    return p();
                case 0x46:
                    return p();
                case 0x47:
                    return p();
                case 0x48:
                    return p();
                case 0x49:
                    return p();
                case 0x4A:
                    return p();
                case 0x4B:
                    return p();
                case 0x4C:
                    return p();
                case 0x4D:
                    return p();
                case 0x4E:
                    return p();
                case 0x4F:
                    return p();
                    /* Sixth Row*/
                case 0x50:
                    return p();
                case 0x51:
                    return p();
                case 0x52:
                    return p();
                case 0x53:
                    return p();
                case 0x54:
                    return p();
                case 0x55:
                    return p();
                case 0x56:
                    return p();
                case 0x57:
                    return p();
                case 0x58:
                    return p();
                case 0x59:
                    return p();
                case 0x5A:
                    return p();
                case 0x5B:
                    return p();
                case 0x5C:
                    return p();
                case 0x5D:
                    return p();
                case 0x5E:
                    return p();
                case 0x5F:
                    return p();
                    /* Seventh Row*/
                case 0x60:
                    return p();
                case 0x61:
                    return p();
                case 0x62:
                    return p();
                case 0x63:
                    return p();
                case 0x64:
                    return p();
                case 0x65:
                    return p();
                case 0x66:
                    return p();
                case 0x67:
                    return p();
                case 0x68:
                    return p();
                case 0x69:
                    return p();
                case 0x6A:
                    return p();
                case 0x6B:
                    return p();
                case 0x6C:
                    return p();
                case 0x6D:
                    return p();
                case 0x6E:
                    return p();
                case 0x6F:
                    return p();
                    /* Eighth Row*/
                case 0x70:
                    return p();
                case 0x71:
                    return p();
                case 0x72:
                    return p();
                case 0x73:
                    return p();
                case 0x74:
                    return p();
                case 0x75:
                    return p();
                case 0x76:
                    return p();
                case 0x77:
                    return p();
                case 0x78:
                    return p();
                case 0x79:
                    return p();
                case 0x7A:
                    return p();
                case 0x7B:
                    return p();
                case 0x7C:
                    return p();
                case 0x7D:
                    return p();
                case 0x7E:
                    return p();
                case 0x7F:
                    return p();
                    /* Ninth Row*/
                case 0x80:
                    return p();
                case 0x81:
                    return p();
                case 0x82:
                    return p();
                case 0x83:
                    return p();
                case 0x84:
                    return p();
                case 0x85:
                    return p();
                case 0x86:
                    return p();
                case 0x87:
                    return p();
                case 0x88:
                    return p();
                case 0x89:
                    return p();
                case 0x8A:
                    return p();
                case 0x8B:
                    return p();
                case 0x8C:
                    return p();
                case 0x8D:
                    return p();
                case 0x8E:
                    return p();
                case 0x8F:
                    return p();
                    /* Tenth Row*/
                case 0x90:
                    return p();
                case 0x91:
                    return p();
                case 0x92:
                    return p();
                case 0x93:
                    return p();
                case 0x94:
                    return p();
                case 0x95:
                    return p();
                case 0x96:
                    return p();
                case 0x97:
                    return p();
                case 0x98:
                    return p();
                case 0x99:
                    return p();
                case 0x9A:
                    return p();
                case 0x9B:
                    return p();
                case 0x9C:
                    return p();
                case 0x9D:
                    return p();
                case 0x9E:
                    return p();
                case 0x9F:
                    return p();
                    /* Eleventh Row*/
                case 0xA0:
                    return p();
                case 0xA1:
                    return p();
                case 0xA2:
                    return p();
                case 0xA3:
                    return p();
                case 0xA4:
                    return p();
                case 0xA5:
                    return p();
                case 0xA6:
                    return p();
                case 0xA7:
                    return p();
                case 0xA8:
                    return p();
                case 0xA9:
                    return p();
                case 0xAA:
                    return p();
                case 0xAB:
                    return p();
                case 0xAC:
                    return p();
                case 0xAD:
                    return p();
                case 0xAE:
                    return p();
                case 0xAF:
                    return p();
                    /* Twelfth Row*/
                case 0xB0:
                    return p();
                case 0xB1:
                    return p();
                case 0xB2:
                    return p();
                case 0xB3:
                    return p();
                case 0xB4:
                    return p();
                case 0xB5:
                    return p();
                case 0xB6:
                    return p();
                case 0xB7:
                    return p();
                case 0xB8:
                    return p();
                case 0xB9:
                    return p();
                case 0xBA:
                    return p();
                case 0xBB:
                    return p();
                case 0xBC:
                    return p();
                case 0xBD:
                    return p();
                case 0xBE:
                    return p();
                case 0xBF:
                    return p();
                    /* Thirteenth Row*/
                case 0xC0:
                    return p();
                case 0xC1:
                    return p();
                case 0xC2:
                    return p();
                case 0xC3:
                    return p();
                case 0xC4:
                    return p();
                case 0xC5:
                    return p();
                case 0xC6:
                    return p();
                case 0xC7:
                    return p();
                case 0xC8:
                    return p();
                case 0xC9:
                    return p();
                case 0xCA:
                    return p();
                case 0xCB:
                    return p();
                case 0xCC:
                    return p();
                case 0xCD:
                    return p();
                case 0xCE:
                    return p();
                case 0xCF:
                    return p();
                    /* Fourteenth Row*/
                case 0xD0:
                    return p();
                case 0xD1:
                    return p();
                case 0xD2:
                    return p();
                case 0xD3:
                    return p();
                case 0xD4:
                    return p();
                case 0xD5:
                    return p();
                case 0xD6:
                    return p();
                case 0xD7:
                    return p();
                case 0xD8:
                    return p();
                case 0xD9:
                    return p();
                case 0xDA:
                    return p();
                case 0xDB:
                    return p();
                case 0xDC:
                    return p();
                case 0xDD:
                    return p();
                case 0xDE:
                    return p();
                case 0xDF:
                    return p();
                    /* Fifteenth Row*/
                case 0xE0:
                    return p();
                case 0xE1:
                    return p();
                case 0xE2:
                    return p();
                case 0xE3:
                    return p();
                case 0xE4:
                    return p();
                case 0xE5:
                    return p();
                case 0xE6:
                    return p();
                case 0xE7:
                    return p();
                case 0xE8:
                    return p();
                case 0xE9:
                    return p();
                case 0xEA:
                    return p();
                case 0xEB:
                    return p();
                case 0xEC:
                    return p();
                case 0xED:
                    return p();
                case 0xEE:
                    return p();
                case 0xEF:
                    return p();
                    /* Sixteenth Row*/
                case 0xF0:
                    return p();
                case 0xF1:
                    return p();
                case 0xF2:
                    return p();
                case 0xF3:
                    return p();
                case 0xF4:
                    return p();
                case 0xF5:
                    return p();
                case 0xF6:
                    return p();
                case 0xF7:
                    return p();
                case 0xF8:
                    return p();
                case 0xF9:
                    return p();
                case 0xFA:
                    return p();
                case 0xFB:
                    return p();
                case 0xFC:
                    return p();
                case 0xFD:
                    return p();
                case 0xFE:
                    return p();
                case 0xFF:
                    return p();
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
      if (hFlag || (regs.af.A & 0x0fu) > 0x09) { regs.af.A += 0x06; }
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
CPU::OPCODE CPU::LD_B_C() {
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
    return ADC_A_n8(n);
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

// Subtract n from A
CPU::OPCODE CPU::SUB_A_n(uint8_t n) {
    return SUB_A_n8(n);
}

// Call address 0x10
CPU::OPCODE CPU::RST_10h() {
    return RST_VEC(0x10u);
}

// Return from subroutine if Carry is set
CPU::OPCODE CPU::RET_C() {
    return RET_CC(C, true);
}

// return from subroutine and enable interrupts
// Equivalent to calling EI then RET
// 4 cycles
CPU::OPCODE CPU::RETI() {
    EI();
    return RET();
}

// Jump to address nn IF CARRY is set
CPU::OPCODE CPU::JP_C_nn(uint16_t nn) {
    return JP_CC_n16(C, true, nn);
}

// CALL address nn if CARRY
// executes an implicit JP nn
// no flags affected
CPU::OPCODE CPU::CALL_C_nn(uint16_t nn) {
    return CALL_CC_n16(C, true, nn);
}

// subtract n and the carry flag from A
CPU::OPCODE CPU::SBC_A_n(uint8_t n) {
    return SBC_A_n8(n);
}

// Call address 0x18
CPU::OPCODE CPU::RST_18h() {
    return RST_VEC(0x18u);
}

// Store value in register A into byte at address n16, provided it is between $FF00 and $FFFF.
// no flags affected
// 3 cycles
// See https://rednex.github.io/rgbds/gbz80.7.html#LDH__n16_,A
CPU::OPCODE CPU::LD_FF00_n_A(uint8_t n) {
    WRITE(0xFF00u + n, regs.af.A);
    return 3;
}

// Pop from the stack onto HL
// No flags are modified
CPU::OPCODE CPU::POP_HL() {
    return POP_REG(regs.hl.HL);
}

// Load value in register A from byte pointed to by register r16.
// no flags affected
// 2 cycles
// See https://rednex.github.io/rgbds/gbz80.7.html#LDH__C_,A
// NOTE: C is the C register, NOT C flag
CPU::OPCODE CPU::LD_FF00_C_A() {
    WRITE(0xFF00u + regs.bc.C, regs.af.A);
    return 2;
}

// load the register HL onto the stack
CPU::OPCODE CPU::PUSH_HL() {
    return PUSH_REG(regs.hl.HL);
}

// Bitwise AND between A and n
// stores the result in A
CPU::OPCODE CPU::AND_A_n(uint8_t n) {
    return AND_A_n8(n);
}

// Call address 0x20
CPU::OPCODE CPU::RST_20h() {
    return RST_VEC(0x20u);
}

// Add the signed value i to SP
// Z unset, N unset, H if overflow from bit 3, C if overflow from bit 7
// 4 cycles
CPU::OPCODE CPU::ADD_SP_i(int8_t i) {
    return ADD_SP_i8(i);
}

// Jump to address in HL
// effectively, load PC with value in register HL
// no flags affected
// 1 cycles
CPU::OPCODE CPU::JP_HL() {
    regs.pc = regs.hl.HL;
    return 1;
}

// store value in reg A into byte at address nn
// no flags affected
CPU::OPCODE CPU::LD_nn_A(uint16_t nn) {
    WRITE(nn, regs.af.A);
    return 4;
}

// Bitwise XOR between the value in n and A
// Same flags as XOR_A_REG
// one more cycle than XOR_A_REG
CPU::OPCODE CPU::XOR_A_n(uint8_t n) {
    // returns 2
    return XOR_A_REG(n) + 1;
}

// call address 0x28
CPU::OPCODE CPU::RST_28h() {
    return RST_VEC(0x28u);
}

// Load value $FF00 + n [$FF00, $FFFF] into A
// 3 cycles
// no flags affected
CPU::OPCODE CPU::LD_A_FF00_n(uint8_t n) {
    regs.af.A = READ(0xFF00u + n);
    return 3;
}

// Pop reg AF from the stack
// 3 cycles
// NOTE: slightly different from the standard CPU::popFromStack
// Flags affected:
// Z: Set from bit 7 of the popped low byte
// N: Set from bit 6 of the popped low byte
// H: Set from bit 5 of the popped low byte
// C: Set from bit 4 of the popped low byte
CPU::OPCODE CPU::POP_AF() {
    uint8_t hi = READ(++regs.sp);
    uint8_t lo = READ(++regs.sp);
    // NOTE: Recall last 4 bits of F are unused
    regs.af.AF = (uint16_t)(((uint8_t)(hi << 8u) | lo) & 0xFFF0u);
    SetFlag(Z, lo & 0x80u); // 0b10000000
    SetFlag(N, lo & 0x40u); // 0b01000000
    SetFlag(H, lo & 0x20u); // 0b00100000
    SetFlag(C, lo & 0x10u); // 0b00100000
    return 3;
}

// Load the value pointed to by [$FF00 + C reg] to A
// 2 cycles
// No flags affected
CPU::OPCODE CPU::LD_A_FF00_C() {
    regs.af.A = READ(0xFF00u + regs.bc.C);
    return 2;
}

// Disable interrupts by clearing the interrupt enable flag
// 1 cycle
// no flags affected
CPU::OPCODE CPU::DI() {
    interrupts_enabled = false;
    return 1;
}

// push reg AF onto the stack.
// 4 cycles
// Roughly equivalent to the following:
// dec sp
// ld [sp] a
// dec sp
// ls [sp] Z << 7 | N << 6 | H << 5 | C << 4
// NOTE: Recall that AF=A3A2A1A0ZNHCXXXX
// Where X=unused
CPU::OPCODE CPU::PUSH_AF() {
    return PUSH_REG(regs.af.AF);
}

// Store teh Bitwise OR of A and n in A
// Same flags as OR_A_REG
// 2 cycles
CPU::OPCODE CPU::OR_A_n(uint8_t n) {
    // returns 2
    return OR_A_REG(n) + 1;
}

// Call address 0x30
CPU::OPCODE CPU::RST_30h() {
    return RST_VEC(0x30u);
}

// Add the signed value i to SP and store the result in HL
// 3 cycles
// Z unset, N unset, H set if overflow from bit 3, C set if overflow from bit 7
CPU::OPCODE CPU::LD_HL_SP_i(int8_t i) {
    auto n1 = regs.sp;
    auto n2 = i;
    regs.hl.HL = n1 + n2;
    SetFlag(Z, false);
    SetFlag(N, false);
    SetFlag(H, HAS_HALF_CARRY_8(n1, n2));
    SetFlag(C, HAS_CARRY_8(regs.hl.HL, n1, n2));
    return 3;
}

// Load register HL into register SP
// 2 cycles
// No flags affected
CPU::OPCODE CPU::LD_SP_HL() {
    regs.sp = regs.hl.HL;
    return 2;
}

// Load the value pointed to by address nn into A
// 3 cycles
// No flags affected
CPU::OPCODE CPU::LD_A_Addr_nn(uint16_t nn) {
    regs.af.A = READ(nn);
    return 3;
}

// Enable interrupts by setting the enable interrupts flag
// 1 cycle
// No flags affected
CPU::OPCODE CPU::EI() {
    interrupts_enabled = true;
    return 1;
}

// Subtract the value n from A and set flags accordingly, but don't store the result
// 2 cycles
// Same flags as CP_A_REG
CPU::OPCODE CPU::CP_A_n(uint8_t n) {
    // returns 2
    return CP_A_REG(n) + 1;
}

// Call address 0x38
CPU::OPCODE CPU::RST_38h() {
    return RST_VEC(0x38u);
}
