//
// Created by Sammy Al Hashemi on 2020-02-02.
//
#include <cstdint>
#include "CPU.h"
#include <array>
#ifndef NESEMULATOR_BUS_H
#define NESEMULATOR_BUS_H

using std::uint16_t;
using std::uint8_t;


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
    std::array<uint8_t, 64 * 1024> RAM{};

private:
    static const uint16_t LOW = 0x0000; // NOTE: GB boots up with PC at 0x0100
    static const uint16_t HI = 0xFFFF;
    static bool addressInRange(uint16_t addr) {
        return (addr >= LOW && addr <= HI);
    }

private:
    uint16_t loadBootROM();
    uint16_t loadCartridge(uint16_t start);

public:
    void WRITE(uint16_t addr, uint8_t data);
    uint8_t READ(uint16_t addr);
    void run();
};


#endif //NESEMULATOR_BUS_H
