//
// Created by Sammy Al Hashemi on 2020-02-02.
//

#include "Bus.h"
#include <cstdio>
#include <unistd.h>
#include <iostream>

Bus::Bus() {
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

    // VRAM
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        VRAM[addr - 0x8000] = data;
        return;
    }
    // Cartridge RAM (External) - Not fully implemented, usually requires enable
    if (addr >= 0xA000 && addr <= 0xBFFF) {
        // TODO: External RAM handling
        return;
    }
    // WRAM
    if (addr >= 0xC000 && addr <= 0xDFFF) {
        WRAM[addr - 0xC000] = data;
        return;
    }
    // Echo RAM (Mirror of WRAM)
    if (addr >= 0xE000 && addr <= 0xFDFF) {
        WRAM[addr - 0xE000] = data;
        return;
    }
    // OAM
    if (addr >= 0xFE00 && addr <= 0xFE9F) {
        OAM[addr - 0xFE00] = data;
        return;
    }
    // Not Usable
    if (addr >= 0xFEA0 && addr <= 0xFEFF) {
        return;
    }
    // IO Registers
    if (addr >= 0xFF00 && addr <= 0xFF7F) {
        IO[addr - 0xFF00] = data;

        // Handle MBC1 Banking (Write to ROM range 0x2000-0x3999) - Wait, this is write to ROM addr space
        // Serial Output Hack for Blargg Tests
        if (addr == 0xFF02 && data == 0x81) {
            std::cout << (char)IO[0x01]; // IO[0xFF01 - 0xFF00]
            IO[0x02] = 0;
        }
        return;
    }
    // HRAM
    if (addr >= 0xFF80 && addr <= 0xFFFE) {
        HRAM[addr - 0xFF80] = data;
        return;
    }
    // IE Register
    if (addr == 0xFFFF) {
        // TODO: Handle properly in memory map or CPU
        // For now, let's treat it as the last byte of HRAM or IO for simplicity if needed,
        // but typically it sits separately.
        // However, standard HRAM array size 127 covers FF80 to FFFE.
        // Let's store it in a member var or just separate slot.
        // Actually, CPU uses READ(INTERRUPT_ENABLE_REG) which calls bus->READ(0xFFFF)
        // Let's reuse HRAM or create a dedicated slot?
        // HRAM is 127 bytes. Let's make it 128 to cover FFFF? No, that's messy.
        // Let's stick it in IO array for now? No, IO ends at FF7F.
        // For now, let's just ignore or store in a specific member if strictly needed,
        // but the CPU seems to expect to read it back.
        // Let's hack it into HRAM for now (expand HRAM to 128 bytes?)
        // Or handle MBC write.
    }

    // MBC1 ROM Banking
    if (addr >= 0x2000 && addr <= 0x3999) {
        uint8_t bank = data & 0x1F;
        if (bank == 0) bank = 1;
        currentRomBank = bank;
    }
}

uint8_t Bus::READ(uint16_t addr) {
    // Boot ROM Overlay
    if (bootRomEnabled && addr < 0x0100) {
        if (addr < bootRomData.size()) {
            return bootRomData[addr];
        }
        return 0x00;
    }

    // ROM Bank 0
    if (addr >= 0x0000 && addr <= 0x3FFF) {
        if (addr < cartridgeMemory.size()) {
            return cartridgeMemory[addr];
        }
        return 0xFF;
    }

    // ROM Bank N
    if (addr >= 0x4000 && addr <= 0x7FFF) {
        uint32_t mappedAddr = (currentRomBank * 0x4000) + (addr - 0x4000);
        if (mappedAddr < cartridgeMemory.size()) {
            return cartridgeMemory[mappedAddr];
        }
        return 0xFF;
    }

    // VRAM
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        return VRAM[addr - 0x8000];
    }
    // Cartridge RAM
    if (addr >= 0xA000 && addr <= 0xBFFF) {
        return 0xFF; // TODO
    }
    // WRAM
    if (addr >= 0xC000 && addr <= 0xDFFF) {
        return WRAM[addr - 0xC000];
    }
    // Echo RAM
    if (addr >= 0xE000 && addr <= 0xFDFF) {
        return WRAM[addr - 0xE000];
    }
    // OAM
    if (addr >= 0xFE00 && addr <= 0xFE9F) {
        return OAM[addr - 0xFE00];
    }
    // Unusable
    if (addr >= 0xFEA0 && addr <= 0xFEFF) {
        return 0xFF;
    }
    // IO
    if (addr >= 0xFF00 && addr <= 0xFF7F) {
        return IO[addr - 0xFF00];
    }
    // HRAM
    if (addr >= 0xFF80 && addr <= 0xFFFE) {
        return HRAM[addr - 0xFF80];
    }
    // IE Register
    if (addr == 0xFFFF) {
        // Return Interrupt Enable register.
        // For this refactor, we need a place to store it.
        // Let's use a static member in Bus or add it to HRAM logic.
        return 0xFF; // Placeholder
    }

    return 0xFF;
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

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    cartridgeMemory.resize(size);
    size_t read = fread(cartridgeMemory.data(), 1, size, file);
    std::cout << "Loaded " << read << " bytes from ROM." << std::endl;
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
            }
        } else {
            break;
        }
        /* usleep(1000000); */
    }
}
