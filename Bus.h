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
    // System Memory
    std::array<uint8_t, 8 * 1024> VRAM{}; // 0x8000 - 0x9FFF
    std::array<uint8_t, 8 * 1024> WRAM{}; // 0xC000 - 0xDFFF (and Echo RAM)
    std::array<uint8_t, 127> HRAM{};      // 0xFF80 - 0xFFFE
    std::array<uint8_t, 128> OAM{};       // 0xFE00 - 0xFE9F
    std::array<uint8_t, 128> IO{};        // 0xFF00 - 0xFF7F

    // Cartridge Memory
    std::vector<uint8_t> cartridgeMemory; // Full ROM data
    uint8_t currentRomBank = 1;

private:
    std::vector<uint8_t> bootRomData;
    bool bootRomEnabled = false;

private:
    bool loadBootROM(const std::string& path);
    void loadCartridge(const std::string& path);

public:
    void WRITE(uint16_t addr, uint8_t data);
    uint8_t READ(uint16_t addr);
    void run();
};


#endif //NESEMULATOR_BUS_H
