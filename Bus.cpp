//
// Created by Sammy Al Hashemi on 2020-02-02.
//

#include "Bus.h"
#include <cstdio>
#include <unistd.h>
#include <iostream>

Bus::Bus() {
    // set the memory to empty at first
    for (auto &i : RAM) i = 0x00;
    // connect the cpu
    cpu.connectBus(this);
}

Bus::~Bus()=default;

void Bus::init(std::string romPath, bool skipBoot, bool debugMode) {
    cpu.debugMode = debugMode;
    loadCartridge(romPath);

    bool bootLoaded = false;
    if (!skipBoot) {
        bootLoaded = loadBootROM("DMG_ROM_2_2.bin");
        if (!bootLoaded) {
            std::cout << "Warning: Could not load boot ROM. Falling back to skip-boot mode." << std::endl;
        }
    }

    if (!bootLoaded || skipBoot) {
        bootRomEnabled = false;
        cpu.regs.pc = 0x0100;
        // Post-boot register values
        cpu.regs.af.AF = 0x01B0;
        cpu.regs.bc.BC = 0x0013;
        cpu.regs.de.DE = 0x00D8;
        cpu.regs.hl.HL = 0x014D;
        cpu.regs.sp = 0xFFFE;
    } else {
        bootRomEnabled = true;
        cpu.regs.pc = 0x0000;
    }
}

void Bus::WRITE(uint16_t addr, u_int8_t data) {
    // Handle Boot ROM unmapping
    if (addr == 0xFF50 && bootRomEnabled && data != 0) {
        bootRomEnabled = false;
        return; // The write to 0xFF50 itself isn't stored in RAM usually, but if needed we can fall through
    }

    // write the contents into memory
    // into the correct memory range
    if (addressInRange(addr))
        RAM[addr] = data;
}

uint8_t Bus::READ(uint16_t addr) {
    // Read from Boot ROM if enabled and in range
    if (bootRomEnabled && addr < 0x0100) {
        if (addr < bootRomData.size()) {
            return bootRomData[addr];
        }
        return 0x00;
    }

    if (addressInRange(addr))
        return RAM[addr];
    // If there is an illegal read
    return LOW;
}

bool Bus::loadBootROM(const std::string& path) {
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) return false;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    bootRomData.resize(size);
    size_t read = fread(bootRomData.data(), 1, size, file);
    fclose(file);

    return read == size;
}

void Bus::loadCartridge(const std::string& path) {
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        std::cerr << "Failed to load ROM: " << path << std::endl;
        return;
    }

    // Read directly into RAM
    size_t read = fread(RAM.data(), 1, RAM.size(), file);
    std::cout << "Loaded " << read << " bytes from ROM into RAM." << std::endl;
    fclose(file);
}

void Bus::run() {
    int cycles;
    while (true) {
        // TODO: handle HALT state
        if (cpu.unpaused) {
            if (!cpu.HALT_FLAG) {
                // process OPCODE and check flags
                cycles = cpu.stepCPU();

                // handle the cycles
                cpu.handleCycles(cycles);

                // handle any interrupts
                // I was reading that the processor let's any instruction complete
                // and then handles interrupts
                cpu.handleInterrupts();

                cpu.printSummary();

                // Check for serial output (Blargg's test ROMs write to 0xFF02)
                if (RAM[0xff02u] == 0x81) {
                    uint8_t c = RAM[0xff01u];
                    printf("%c", c);
                    RAM[0xff02u] = 0x0u;
                }
            }
        } else {
            break;
        }
        /* usleep(1000000); */
    }
}
