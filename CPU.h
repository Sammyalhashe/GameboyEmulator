//
// Created by Sammy Al Hashemi on 2020-02-02.
//

#include <_types/_uint16_t.h>
#include <_types/_uint8_t.h>
#include <algorithm>
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
    bool HALT_FLAG = false;
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
    // pop next instruction (next program count) from stack
    uint16_t popFromStack();
    void pushToStack(uint16_t ADDR);
    void handleCycles(int cycles);
    void handleInterrupts();

private:
    // Write to memory address -> See Bus implementation
    void WRITE(u_int16_t addr, u_int8_t data);
    // Read from an address in memory (I use a long array - as seems standard - to represent my memory)
    // See implementation in Bus class
    uint8_t READ(u_int16_t addr, bool read_only = false);


private:
    int  cycles = 0;

    uint8_t GetFlag(Z80_FLAGS f) const;
    void SetFlag(Z80_FLAGS f, bool v);


private: //OPCODES
    struct INSTRUCTION {
        std::string name;
        uint8_t (CPU::*operate)(REGS&) = nullptr;
        uint8_t cycles = 0;
    };

    std::vector<INSTRUCTION> lookup;

    /*
     * Summary of function names:
     * [Mnemonic(I/D)]_[(Addr)...args]
     * NOTE: If one of the arguments has an "Addr" prepended, this means we are referring to the address pointed to
     * NOTE: If the Mnemonic has as "I"/"D" appended, this means that the operation should post-increment/decrement
     * the argument which is a pointer (has "Addr" prepended)
     * by that argument (reg).
     */
    /** Unprefixed Table     +0                             +1                            +2                            +3                         +4                              +5                     +6                              +7                     +8                                 +9                  +A                                +B                        +C                             +D                           +E                         +F            **/
    /*  00+  */              OPCODE NOP();                  OPCODE LD_BC_nn(uint16_t nn); OPCODE LD_Addr_BC_A();        OPCODE INC_BC();           OPCODE INC_B();                 OPCODE DEC_B();        OPCODE LD_B_n(uint8_t n);       OPCODE RLC_A();        OPCODE LD_Addr_nn_SP(uint16_t nn); OPCODE ADD_HL_BC(); OPCODE LD_A_Addr_BC();            OPCODE DEC_BC();          OPCODE INC_C();                OPCODE DEC_C();              OPCODE LD_C_n(uint8_t n);  OPCODE RRC_A();
    /*  10+  */              OPCODE STOP();                 OPCODE LD_DE_nn(uint16_t nn); OPCODE LD_Addr_DE_A();        OPCODE INC_DE();           OPCODE INC_D();                 OPCODE DEC_D();        OPCODE LD_D_n(uint8_t n);       OPCODE RL_A();         OPCODE JR_i(int8_t n);             OPCODE ADD_HL_DE(); OPCODE LD_A_Addr_DE();            OPCODE DEC_DE();          OPCODE INC_E();                OPCODE DEC_E();              OPCODE LD_E_n(uint8_t n);  OPCODE RR_A();
    /*  20+  */              OPCODE JR_NZ_i(int8_t n);      OPCODE LD_HL_nn(uint16_t nn); OPCODE LDI_Addr_HL_A();       OPCODE INC_HL();           OPCODE INC_H();                 OPCODE DEC_H();        OPCODE LD_H_n(uint8_t n);       OPCODE DAA();          OPCODE JR_Z_i(int8_t n);           OPCODE ADD_HL_HL(); OPCODE LDI_A_Addr_HL();           OPCODE DEC_HL();          OPCODE INC_L();                OPCODE DEC_L();              OPCODE LD_L_n(uint8_t n);  OPCODE CPL();
    /*  30+  */              OPCODE JR_NC_i(int8_t n);      OPCODE LD_SP_nn(uint16_t nn); OPCODE LDD_Addr_HL_A();       OPCODE INC_SP();           OPCODE INC_Addr_HL();           OPCODE DEC_Addr_HL();  OPCODE LD_Addr_HL_n(uint8_t n); OPCODE SCF();          OPCODE JR_C_i(int8_t n);           OPCODE ADD_HL_SP(); OPCODE LDD_A_Addr_HL();           OPCODE DEC_SP();          OPCODE INC_A();                OPCODE DEC_A();              OPCODE LD_A_n(uint8_t n);  OPCODE CCF();
    /*  40+  */              OPCODE LD_B_B();               OPCODE LD_B_C();              OPCODE LD_B_D();              OPCODE LD_B_E();           OPCODE LD_B_H();                OPCODE LD_B_L();       OPCODE LD_B_Addr_HL();          OPCODE LD_B_A();       OPCODE LD_C_B();                   OPCODE LD_C_C();    OPCODE LD_C_D();                  OPCODE LD_C_E();          OPCODE LD_C_H();               OPCODE LD_C_L();             OPCODE LD_C_Addr_HL();     OPCODE LD_C_A();
    /*  50+  */              OPCODE LD_D_B();               OPCODE LD_D_C();              OPCODE LD_D_D();              OPCODE LD_D_E();           OPCODE LD_D_H();                OPCODE LD_D_L();       OPCODE LD_D_Addr_HL();          OPCODE LD_D_A();       OPCODE LD_E_B();                   OPCODE LD_E_C();    OPCODE LD_E_D();                  OPCODE LD_E_E();          OPCODE LD_E_H();               OPCODE LD_E_L();             OPCODE LD_E_Addr_HL();     OPCODE LD_E_A();
    /*  60+  */              OPCODE LD_H_B();               OPCODE LD_H_C();              OPCODE LD_H_D();              OPCODE LD_H_E();           OPCODE LD_H_H();                OPCODE LD_H_L();       OPCODE LD_H_Addr_HL();          OPCODE LD_H_A();       OPCODE LD_L_B();                   OPCODE LD_L_C();    OPCODE LD_L_D();                  OPCODE LD_L_E();          OPCODE LD_L_H();               OPCODE LD_L_L();             OPCODE LD_L_Addr_HL();     OPCODE LD_L_A();
    /*  70+  */              OPCODE LD_Addr_HL_B();         OPCODE LD_Addr_HL_C();        OPCODE LD_Addr_HL_D();        OPCODE LD_Addr_HL_E();     OPCODE LD_Addr_HL_H();          OPCODE LD_Addr_HL_L(); OPCODE HALT();                  OPCODE LD_Addr_HL_A(); OPCODE LD_A_B();                   OPCODE LD_A_C();    OPCODE LD_A_D();                  OPCODE LD_A_E();          OPCODE LD_A_H();               OPCODE LD_A_L();             OPCODE LD_A_Addr_HL();     OPCODE LD_A_A();
    /*  80+  */              OPCODE ADD_A_B();              OPCODE ADD_A_C();             OPCODE ADD_A_D();             OPCODE ADD_A_E();          OPCODE ADD_A_H();               OPCODE ADD_A_L();      OPCODE ADD_A_Addr_HL();         OPCODE ADD_A_A();      OPCODE ADC_A_B();                  OPCODE ADC_A_C();   OPCODE ADC_A_D();                 OPCODE ADC_A_E();         OPCODE ADC_A_H();              OPCODE ADC_A_L();            OPCODE ADC_A_Addr_HL();    OPCODE ADC_A_A();
    /*  90+  */              OPCODE SUB_A_B();              OPCODE SUB_A_C();             OPCODE SUB_A_D();             OPCODE SUB_A_E();          OPCODE SUB_A_H();               OPCODE SUB_A_L();      OPCODE SUB_A_Addr_HL();         OPCODE SUB_A_A();      OPCODE SBC_A_B();                  OPCODE SBC_A_C();   OPCODE SBC_A_D();                 OPCODE SBC_A_E();         OPCODE SBC_A_H();              OPCODE SBC_A_L();            OPCODE SBC_A_Addr_HL();    OPCODE SBC_A_A();
    /*  A0+  */              OPCODE AND_A_B();              OPCODE AND_A_C();             OPCODE AND_A_D();             OPCODE AND_A_E();          OPCODE AND_A_H();               OPCODE AND_A_L();      OPCODE AND_A_Addr_HL();         OPCODE AND_A_A();      OPCODE XOR_A_B();                  OPCODE XOR_A_C();   OPCODE XOR_A_D();                 OPCODE XOR_A_E();         OPCODE XOR_A_H();              OPCODE XOR_A_L();            OPCODE XOR_A_Addr_HL();    OPCODE XOR_A_A();
    /*  B0+  */              OPCODE OR_A_B();               OPCODE OR_A_C();              OPCODE OR_A_D();              OPCODE OR_A_E();           OPCODE OR_A_H();                OPCODE OR_A_L();       OPCODE OR_A_Addr_HL();          OPCODE OR_A_A();       OPCODE CP_A_B();                   OPCODE CP_A_C();    OPCODE CP_A_D();                  OPCODE CP_A_E();          OPCODE CP_A_H();               OPCODE CP_A_L();             OPCODE CP_A_Addr_HL();     OPCODE CP_A_A();
    /*  C0+  */              OPCODE RET_NZ();               OPCODE POP_BC();              OPCODE JP_NZ_nn(uint16_t nn); OPCODE JP_nn(uint16_t nn); OPCODE CALL_NZ_nn(uint16_t nn); OPCODE PUSH_BC();      OPCODE ADD_A_n(uint8_t n);      OPCODE RST_00h();      OPCODE RET_Z();                    OPCODE RET();       OPCODE JP_Z_nn(uint16_t nn);      /* Maps to other table */ OPCODE CALL_Z_nn(uint16_t nn); OPCODE CALL_nn(uint16_t nn); OPCODE ADC_A_n(uint8_t n); OPCODE RST_08h();
    /*  D0+  */              OPCODE RET_NC();               OPCODE POP_DE();              OPCODE JP_NC_nn(uint16_t nn); /*      No Mapping      */ OPCODE CALL_NC_nn(uint16_t nn); OPCODE PUSH_DE();      OPCODE SUB_A_n(uint8_t n);      OPCODE RST_10h();      OPCODE RET_C();                    OPCODE RETI();      OPCODE JP_C_nn(uint16_t nn);      /*  No Mapping   */       OPCODE CALL_C_nn(uint16_t nn); /*       No Mapping       */ OPCODE SBC_A_n(uint8_t n); OPCODE RST_18h();
    /*  E0+  */              OPCODE LD_FF00_n_A(uint8_t n); OPCODE POP_HL();              OPCODE LD_FF00_C_A();         /*      No Mapping      */ /*         No Mapping        */ OPCODE PUSH_HL();      OPCODE AND_A_n(uint8_t n);      OPCODE RST_20h();      OPCODE ADD_SP_i(int8_t i);         OPCODE JP_HL();     OPCODE LD_nn_A(uint16_t nn);      /*  No Mapping   */       /*       No Mapping         */ /*       No Mapping       */ OPCODE XOR_A_n(uint8_t n); OPCODE RST_28h();
    /*  F0+  */              OPCODE LD_A_FF00_n(uint8_t n); OPCODE POP_AF();              OPCODE LD_A_FF00_C();         OPCODE DI();               /*         No Mapping        */ OPCODE PUSH_AF();      OPCODE OR_A_n(uint8_t n);       OPCODE RST_30h();      OPCODE LD_HL_SP_i(int8_t i);       OPCODE LD_SP_HL();  OPCODE LD_A_Addr_nn(uint16_t nn); OPCODE EI();              /*       No Mapping         */ /*       No Mapping       */ OPCODE CP_A_n(uint8_t n);  OPCODE RST_38h();

    /** Prefixed Table     +0                             +1                            +2                            +3                         +4                              +5                     +6                              +7                     +8                                 +9                  +A                                +B                        +C                             +D                           +E                         +F            **/
    /*  00+  */              OPCODE RLC_B();                OPCODE RLC_C();               OPCODE RLC_D();               OPCODE RLC_E();            OPCODE RLC_H();                 OPCODE RLC_L();        OPCODE RLC_Addr_HL();           OPCODE RLC_A();        OPCODE RRC_B();                    OPCODE RRC_C();     OPCODE RRC_D();                   OPCODE RRC_E();           OPCODE RRC_H();                OPCODE RRC_L();              OPCODE RRC_Addr_HL();      OPCODE RRC_A();
    /*  10+  */              OPCODE RL_B();                 OPCODE RL_C();                OPCODE RL_D();                OPCODE RL_E();             OPCODE RL_H();                  OPCODE RL_L();         OPCODE RL_Addr_HL();            OPCODE RL_A();         OPCODE RR_B();                     OPCODE RR_C();      OPCODE RR_D();                    OPCODE RR_E();            OPCODE RR_H();                 OPCODE RR_L();               OPCODE RR_Addr_HL();       OPCODE RR_A();
    /*  20+  */              OPCODE SLA_B();                OPCODE SLA_C();               OPCODE SLA_D();               OPCODE SLA_E();            OPCODE SLA_H();                 OPCODE SLA_L();        OPCODE SLA_Addr_HL();           OPCODE SLA_A();        OPCODE SRA_B();                    OPCODE SRA_C();     OPCODE SRA_D();                   OPCODE SRA_E();           OPCODE SRA_H();                OPCODE SRA_L();              OPCODE SRA_Addr_HL();      OPCODE SRA_A();
    /*  30+  */              OPCODE SWAP_B();               OPCODE SWAP_C();              OPCODE SWAP_D();              OPCODE SWAP_E();           OPCODE SWAP_H();                OPCODE SWAP_L();       OPCODE SWAP_Addr_HL();          OPCODE SWAP_A();       OPCODE SRL_B();                    OPCODE SRL_C();     OPCODE SRL_D();                   OPCODE SRL_E();           OPCODE SRL_H();                OPCODE SRL_L();              OPCODE SRL_Addr_HL();      OPCODE SRL_A();
    /*  40+  */              OPCODE BIT_0_B();              OPCODE BIT_0_C();             OPCODE BIT_0_D();             OPCODE BIT_0_E();          OPCODE BIT_0_H();               OPCODE BIT_0_L();      OPCODE BIT_0_Addr_HL();         OPCODE BIT_0_A();      OPCODE BIT_1_B();                  OPCODE BIT_1_C();   OPCODE BIT_1_D();                 OPCODE BIT_1_E();         OPCODE BIT_1_H();              OPCODE BIT_1_L();            OPCODE BIT_1_Addr_HL();    OPCODE BIT_1_A();
    /*  50+  */              OPCODE BIT_2_B();              OPCODE BIT_2_C();             OPCODE BIT_2_D();             OPCODE BIT_2_E();          OPCODE BIT_2_H();               OPCODE BIT_2_L();      OPCODE BIT_2_Addr_HL();         OPCODE BIT_2_A();      OPCODE BIT_3_B();                  OPCODE BIT_3_C();   OPCODE BIT_3_D();                 OPCODE BIT_3_E();         OPCODE BIT_3_H();              OPCODE BIT_3_L();            OPCODE BIT_3_Addr_HL();    OPCODE BIT_3_A();
    /*  60+  */              OPCODE BIT_4_B();              OPCODE BIT_4_C();             OPCODE BIT_4_D();             OPCODE BIT_4_E();          OPCODE BIT_4_H();               OPCODE BIT_4_L();      OPCODE BIT_4_Addr_HL();         OPCODE BIT_4_A();      OPCODE BIT_5_B();                  OPCODE BIT_5_C();   OPCODE BIT_5_D();                 OPCODE BIT_5_E();         OPCODE BIT_5_H();              OPCODE BIT_5_L();            OPCODE BIT_5_Addr_HL();    OPCODE BIT_5_A();
    /*  70+  */              OPCODE BIT_6_B();              OPCODE BIT_6_C();             OPCODE BIT_6_D();             OPCODE BIT_6_E();          OPCODE BIT_6_H();               OPCODE BIT_6_L();      OPCODE BIT_6_Addr_HL();         OPCODE BIT_6_A();      OPCODE BIT_7_B();                  OPCODE BIT_7_C();   OPCODE BIT_7_D();                 OPCODE BIT_7_E();         OPCODE BIT_7_H();              OPCODE BIT_7_L();            OPCODE BIT_7_Addr_HL();    OPCODE BIT_7_A();
    /*  80+  */              OPCODE RES_0_B();              OPCODE RES_0_C();             OPCODE RES_0_D();             OPCODE RES_0_E();          OPCODE RES_0_H();               OPCODE RES_0_L();      OPCODE RES_0_Addr_HL();         OPCODE RES_0_A();      OPCODE RES_1_B();                  OPCODE RES_1_C();   OPCODE RES_1_D();                 OPCODE RES_1_E();         OPCODE RES_1_H();              OPCODE RES_1_L();            OPCODE RES_1_Addr_HL();    OPCODE RES_1_A();
    /*  90+  */              OPCODE RES_2_B();              OPCODE RES_2_C();             OPCODE RES_2_D();             OPCODE RES_2_E();          OPCODE RES_2_H();               OPCODE RES_2_L();      OPCODE RES_2_Addr_HL();         OPCODE RES_2_A();      OPCODE RES_3_B();                  OPCODE RES_3_C();   OPCODE RES_3_D();                 OPCODE RES_3_E();         OPCODE RES_3_H();              OPCODE RES_3_L();            OPCODE RES_3_Addr_HL();    OPCODE RES_3_A();
    /*  A0+  */              OPCODE RES_4_B();              OPCODE RES_4_C();             OPCODE RES_4_D();             OPCODE RES_4_E();          OPCODE RES_4_H();               OPCODE RES_4_L();      OPCODE RES_4_Addr_HL();         OPCODE RES_4_A();      OPCODE RES_5_B();                  OPCODE RES_5_C();   OPCODE RES_5_D();                 OPCODE RES_5_E();         OPCODE RES_5_H();              OPCODE RES_5_L();            OPCODE RES_5_Addr_HL();    OPCODE RES_5_A();
    /*  B0+  */              OPCODE RES_6_B();              OPCODE RES_6_C();             OPCODE RES_6_D();             OPCODE RES_6_E();          OPCODE RES_6_H();               OPCODE RES_6_L();      OPCODE RES_6_Addr_HL();         OPCODE RES_6_A();      OPCODE RES_7_B();                  OPCODE RES_7_C();   OPCODE RES_7_D();                 OPCODE RES_7_E();         OPCODE RES_7_H();              OPCODE RES_7_L();            OPCODE RES_7_Addr_HL();    OPCODE RES_7_A();
    /*  C0+  */              OPCODE SET_x_B();              OPCODE SET_x_C();             OPCODE SET_x_D();             OPCODE SET_x_E();          OPCODE SET_x_H();               OPCODE SET_x_L();      OPCODE SET_x_Addr_HL();         OPCODE SET_x_A();      OPCODE SET_x_B();                  OPCODE SET_x_C();   OPCODE SET_x_D();                 OPCODE SET_x_E();         OPCODE SET_x_H();              OPCODE SET_x_L();            OPCODE SET_x_Addr_HL();    OPCODE SET_x_A();
    /*  D0+  */              OPCODE SET_x_B();              OPCODE SET_x_C();             OPCODE SET_x_D();             OPCODE SET_x_E();          OPCODE SET_x_H();               OPCODE SET_x_L();      OPCODE SET_x_Addr_HL();         OPCODE SET_x_A();      OPCODE SET_x_B();                  OPCODE SET_x_C();   OPCODE SET_x_D();                 OPCODE SET_x_E();         OPCODE SET_x_H();              OPCODE SET_x_L();            OPCODE SET_x_Addr_HL();    OPCODE SET_x_A();
    /*  E0+  */              OPCODE SET_x_B();              OPCODE SET_x_C();             OPCODE SET_x_D();             OPCODE SET_x_E();          OPCODE SET_x_H();               OPCODE SET_x_L();      OPCODE SET_x_Addr_HL();         OPCODE SET_x_A();      OPCODE SET_x_B();                  OPCODE SET_x_C();   OPCODE SET_x_D();                 OPCODE SET_x_E();         OPCODE SET_x_H();              OPCODE SET_x_L();            OPCODE SET_x_Addr_HL();    OPCODE SET_x_A();
    /*  F0+  */              OPCODE SET_x_B();              OPCODE SET_x_C();             OPCODE SET_x_D();             OPCODE SET_x_E();          OPCODE SET_x_H();               OPCODE SET_x_L();      OPCODE SET_x_Addr_HL();         OPCODE SET_x_A();      OPCODE SET_x_B();                  OPCODE SET_x_C();   OPCODE SET_x_D();                 OPCODE SET_x_E();         OPCODE SET_x_H();              OPCODE SET_x_L();            OPCODE SET_x_Addr_HL();    OPCODE SET_x_A();

    // Increment register (8-bit/1-byte)
    // Z affected, N unset, H affected
    void INCREMENT_8_BIT_REG(uint8_t&);
    // Decrement register (8-bit/1-byte)
    // Z affected, N set, H affected
    void DECREMENT_8_BIT_REG(uint8_t&);
    // Load 8-bit value stored in register reg into memory REG points to, and post decrement it's address
    // returns the number of cycles it takes (2)
    // No flags affected
    int LDD_Addr_REG_reg(uint16_t&, uint8_t&, bool);
    // Load value pointed to by register REG into register reg
    // return the number of cycles it takes (2)
    // No flags affected
    int LDD_reg_Addr_REG(uint8_t&, uint16_t&, bool);
    /**
     * Adds HL, BC and puts the result in HL.
     * N is unset, H, C are affected by the result
     * In this operation, H is set if there is a carry-over from 11-12.
     * NOTE: watch out for these half-carry. I was reading online that they can
     * vary per operation.
     */
    int ADD_HL_REG(uint16_t REG);
    /**
     * Adds the value stored in REG to A and stores the result in A.
     */
    int ADD_A_REG(uint8_t REG);
    int ADD_A_n8(uint8_t n);
    int ADD_A_Addr_REG16(uint16_t REG);
    int ADD_SP_i8(int8_t i);
    int ADC_A_REG(uint8_t REG);
    int ADC_A_n8(uint8_t n);
    int ADC_A_Addr_REG16(uint16_t REG);
    int SUB_A_REG(uint8_t REG);
    int SUB_A_n8(uint8_t n);
    int SUB_A_Addr_REG16(uint16_t REG);
    int SBC_A_REG(uint8_t REG);
    int SBC_A_n8(uint8_t n);
    int SBC_A_Addr_REG16(uint16_t REG);
    int AND_A_REG(uint8_t REG);
    int AND_A_n8(uint8_t n);
    int AND_A_Addr_REG16(uint16_t REG);
    int XOR_A_REG(uint8_t REG);
    int XOR_A_Addr_REG16(uint16_t REG);
    int OR_A_REG(uint8_t REG);
    int OR_A_Addr_REG16(uint16_t REG);
    int CP_A_REG(uint8_t REG);
    int CP_A_Addr_REG16(uint16_t REG);
    int POP_REG(uint16_t& REG);
    int PUSH_REG(uint16_t REG);
    int RST_VEC(uint8_t VEC);
    int JP_CC_n16(Z80_FLAGS FLAG, bool CC, uint16_t nn);
    int CALL_CC_n16(Z80_FLAGS FLAG, bool CC, uint16_t nn);
    int RET_CC(Z80_FLAGS FLAG, bool CC);

    /** Helper functions to read memory **/
    uint16_t ReadNn();
    uint8_t ReadN();
    int8_t ReadI();
};


#endif //NESEMULATOR_ARMTDMI_H
