//
// Created by Sammy Al Hashemi on 2020-02-02.
//

#include <cstdint>
#include <array>
#include <string>
#include <vector>

#ifndef NESEMULATOR_ARMTDMI_H
#define NESEMULATOR_ARMTDMI_H

class Bus;

class CPU {


public:
    CPU();
    ~CPU();


private:
    Bus *bus = nullptr;



public:
    enum Z80_FLAGS {
        // NOTE: the "u" appended makes it unsigned; This helps me get around CLANG's signed x unsigned warning
        Z  = (1u << 7u),   // Zero flag
        N  = (1u << 6u),   // Subtract flag
        H  = (1u << 5u),   // Half carry flag
        C  = (1u << 4u),   // Carry flag
        U3 = (0u << 3u),   // Unused 3 -> should stay at 0
        U2 = (0u << 2u),   // Unused 2 -> should stay at 0
        U1 = (0u << 1u),   // Unused 1 -> should stay at 0
        U0 = (0u << 0u),   // Unused 0 -> should stay at 0
    };
    bool HALT = false;
    //          Array of registers                              //
    //          16 registers in the ARM CPU                     //
    //          Several important registers:                    //
    //          14: subroutine Link Register (LR)               //
    //          15: Program Counter (PC)                        //
    //          In ARM state ([1:0] are 0, [31:2] contain PC)   //
    //          In THUMB state [0] is 0, [31:1] contain PC)     //
    //          r14 recieves a copy of r15(PC) when a           //
    //          Branch with Link (BL) instruction is executed   //
    typedef struct {
        // GB registers (8 bits but an be combined for 16-bit operations)
        // AF, BC, DE, HL
        // (a) accumulator, (f) status register, rest are general purpose
        // (a) reg is where almost all data being processed pasees through
        // It is also the only register that can be complemented, decimal adjusted, or negated.
        // It is also the source AND destination for 8-bit arithmetic operations.
        // (b) & (c) are generally used as counters during repetitive blocks of code (like moving data to somewhere else)
        // (d) & (e) are generally used together as a 16-bit reg for holding destination address.
        // (h) & (l) are special in that they are extensively used for indirect addressing.
        // Indirect addressing example: LOAD R1 HL -> Load content of mem address stored at mem address HL to R1
        union AF {
            uint16_t AF;
            struct {
                uint8_t F;
                uint8_t A;
            };
        };
        union BC {
            uint16_t BC;
            struct {
                uint8_t C;
                uint8_t B;
            };
        };
        union DE {
            uint16_t DE;
            struct {
                uint8_t E;
                uint8_t D;
            };
        };
        union HL {
            uint16_t HL;
            struct {
                uint8_t L;
                uint8_t H;
            };
        };
        AF af;
        BC bc;
        DE de;
        HL hl;
        uint8_t a = 0x00, b = 0x00, c = 0x00, d = 0x00, e = 0x00, f = 0x00, h = 0x00, l = 0x00;
        uint16_t pc = 0x0100; // when the GB starts up, this is the initial value as per gbdev site
        // NOTE: the Stack in the Gameboy CPU grows from the top DOWN as per gbdev
        uint16_t sp = 0xFFFE; // when the GB starts up, this is the initial value as per gbdev site

        // GBA regs (32-bits in ARM mode)
        // u_int32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15; // normal regs in System and User mode
        // u_int32_t r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq, r13_fiq, r14_fiq;          // FIQ mode
        // u_int32_t r13_svc, r14_svc;                                                     // Supervisor mode
        // u_int32_t r13_abt, r14_abt;                                                     // Abort mode
        // u_int32_t r13_irq, r14_irq;                                                     // IRQ mode
        // u_int32_t r13_und, r14_und;                                                     // Undefined mode
        
        uint64_t clkcount; // clk
    } REGS;
    typedef int OPCODE; // type alias for OPCODES (for mapping)
    REGS regs;
    bool unpaused = true;
    bool interrupts_enabled = false;

public:
    // connects the CPU to the created Bus
    void connectBus(Bus *newBus) {bus = newBus;}
    // steps the CPU forward
    int stepCPU();
    void handleCycles(int cycles);
    void handleInterrupts();

private:
    // Write to memory address -> See Bus implementation
    void WRITE(u_int16_t addr, u_int8_t data);
    // Read from an address in memory (I use a long array - as seems standard - to represent my memory)
    // See implementation in Bus class
    u_int8_t READ(u_int16_t addr, bool read_only = false);


private:
    int  cycles = 0;

    uint8_t GetFlag(Z80_FLAGS f);
    void SetFlag(Z80_FLAGS f, bool v);


private: //OPCODES
    struct INSTRUCTION {
        std::string name;
        uint8_t (CPU::*operate)(REGS&) = nullptr;
        uint8_t cycles = 0;
    };

    std::vector<INSTRUCTION> lookup;

    /** Unprefixed Table **/
    /* First Row  */ OPCODE NOP();  OPCODE LD_BC_nn(uint16_t nn); OPCODE LD_AddrBC_A();  OPCODE INC_BC(); OPCODE INC_B(); OPCODE DEC_B(); OPCODE LD_B_n(uint8_t n); OPCODE RLC_A(); OPCODE LD_Addr_nn_SP(uint16_t nn); OPCODE ADD_HL_BC(); OPCODE LD_A_Addr_BC(); OPCODE DEC_BC(); OPCODE INC_C(); OPCODE DEC_C(); OPCODE LD_C_n(uint8_t n); OPCODE RRC_A();
    /* Second Row */ OPCODE STOP(); OPCODE LD_DE_nn(uint16_t nn); OPCODE LD_Addr_DE_A(); OPCODE INC_DE(); OPCODE INC_D(); OPCODE DEC_D(); OPCODE LD_D_n(uint8_t n); OPCODE RL_A();  OPCODE JR_n(uint8_t n);            OPCODE ADD_HL_DE(); OPCODE LD_A_Addr_DE(); OPCODE DEC_DE(); OPCODE INC_E(); OPCODE DEC_E(); OPCODE LD_E_n(uint8_t n); OPCODE RR_A();
    /* Third Row  */
};


#endif //NESEMULATOR_ARMTDMI_H
