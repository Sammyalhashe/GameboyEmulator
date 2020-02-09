//
// Created by Sammy Al Hashemi on 2020-02-02.
//
#include <cstdint>
#include "CPU.h"
#include <array>
#ifndef NESEMULATOR_BUS_H
#define NESEMULATOR_BUS_H


/**
 * Bus class
 * Contains the crucial devices connected to the Bus
 * as well as the crucial operations such as READ/WRITE.
 */
class Bus {
public:
    Bus();
    ~Bus();

public:
    CPU cpu;
    std::array<u_int8_t, 64 * 1024> RAM{};

private:
    static const u_int16_t LOW = 0x0000; // NOTE: GB boots up with PC at 0x0100
    static const u_int16_t HI = 0xFFFF;
    static bool addressInRange(u_int16_t addr) {
        return (addr >= LOW && addr <= HI);
    }

public:
    void WRITE(u_int16_t addr, u_int8_t data);
    u_int8_t READ(u_int16_t addr);
};


#endif //NESEMULATOR_BUS_H
