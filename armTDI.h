//
// Created by Sammy Al Hashemi on 2020-02-09.
//

#ifndef NESEMULATOR_ARMTDI_H
#define NESEMULATOR_ARMTDI_H

#include <cstdint>
#include <array>
#include <string>
#include <vector>


class armTDI {


private:
    // below is code I originally wrote for GBA emulation -> Will keep it around
    // or else it will go to waste
    u_int32_t cpsr;
    std::array<u_int32_t, 5> spsr; // one for each privilege mode (System & User use the same one -> 0)
    /**
     * Bits [31:28] of spsr
     * NOTE: had to comment this out as it conflicts with GB code
    enum TDMI_CONDITION_CODE_FLAGS {
        V = (1 << 0),
        C = (1 << 1),
        Z = (1 << 2),
        N = (1 << 3)
    };
     */
    /**
     * Bits [7:0] of spsr -> rest are reserved bits (make sure these aren't altered when flags are)
     */
    enum TDMI_CONTROL_BITS {
        M0 = (1u << 0u), // MX bits are the mode bits
        M1 = (1u << 1u), // define what mode the cpu is in
        M2 = (1u << 2u), // User, IRQ, FIQ, etc...
        M3 = (1u << 3u),
        M4 = (1u << 4u),
        T  = (1u << 5u), // reflects if operating in THUMB state, else in ARM
        F  = (1u << 6u), // disables FIQ interrupts
        I  = (1u << 7u), // disables IRQ interrupts
    };
};


#endif //NESEMULATOR_ARMTDI_H
