//
// Created by Sammy Al Hashemi on 2020-02-02.
//

#include "Bus.h"
#include <_stdio.h>
#include <cstdio>
#include <unistd.h>

#define TESTING true

Bus::Bus() {
    // set the memory to empty at first
    for (auto &i : RAM) i = 0x00;
    // connect the cpu
    cpu.connectBus(this);

    // load the cartridge
    loadCartridge(0x0000u);
    // load the bootROM into memory
    // the result of this should be the pc at 0x100
    loadBootROM();


    // run the cpu
    run();
}

Bus::~Bus()=default;

void Bus::WRITE(uint16_t addr, u_int8_t data) {
    // write the contents into memory
    // into the correct memory range
    if (addressInRange(addr))
        RAM[addr] = data;
}

uint8_t Bus::READ(uint16_t addr) {
    if (addressInRange(addr))
        return RAM[addr];
    // If there is an illegal read
    return LOW;
}

uint16_t Bus::loadBootROM() {
    FILE* file = fopen("DMG_ROM_2_2.bin", "rb");
    uint16_t pos = 0;
    // Read in the bootROM into memory
    // the first 0x100 instructions (0 - ff) (overlays them until finished executing)
    while (fread(&RAM[pos], 1, 1, file)) {
        pos++;
    }
    fclose(file);
    return pos;
}

uint16_t Bus::loadCartridge(uint16_t start) {
    FILE* file = fopen("Resources/cpu_instrs/cpu_instrs.gb", "rb");
    uint16_t pos = start;

    while (fread(&RAM[pos], 1, 1, file)) {
        pos++;
    }
    fclose(file);
    return pos;
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

#if TESTING == true
                // printf("checking mem 0x%04x\n", 0xff02);
                if (RAM[0xff02u] == 0x81) {
                    uint8_t c = RAM[0xff01u];
                    printf("%c\n", c);
                    RAM[0xff02u] = 0x0u;
                }
#endif
            }
        } else {
            break;
        }
        usleep(1000000);
    }
}
