//
// Created by Sammy Al Hashemi on 2020-02-02.
//
#include <cstdint>
#include "CPU.h"
#include <array>
#include <vector>
#include <string>

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

    // Initialize the bus with ROM loading options
    void init(std::string romPath, bool skipBoot, bool debugMode);

public:
    CPU cpu;
    std::array<uint8_t, 64 * 1024> RAM{};

private:
    std::vector<uint8_t> bootRomData;
    bool bootRomEnabled = false;

    static const uint16_t LOW = 0x0000; // NOTE: GB boots up with PC at 0x0100
    static const uint16_t HI = 0xFFFF;
    static bool addressInRange(uint16_t addr) {
        return (addr >= LOW && addr <= HI);
    }

private:
    bool loadBootROM(const std::string& path);
    void loadCartridge(const std::string& path);

public:
    void WRITE(uint16_t addr, uint8_t data);
    uint8_t READ(uint16_t addr);
    void run();
};


#endif //NESEMULATOR_BUS_H
