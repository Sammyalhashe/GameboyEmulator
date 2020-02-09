//
// Created by Sammy Al Hashemi on 2020-02-02.
//

#include "Bus.h"

Bus::Bus() {
    // set the memory to empty at first
    for (auto &i : RAM) i = 0x00;
    // connect the cpu
    cpu.connectBus(this);
    int cycles;
    while (true) {
        if (cpu.unpaused) {
            if (!cpu.HALT) {
                // process OPCODE and check flags
                cycles = cpu.stepCPU();

                // handle the cycles
                cpu.handleCycles(cycles);

                // handle any interrupts
                // I was reading that the processor let's any instruction complete
                // and then handles interrupts
                cpu.handleInterrupts();
            }
        } else {
            break;
        }
    }
}

Bus::~Bus()=default;

void Bus::WRITE(u_int16_t addr, u_int8_t data) {
    // write the contents into memory
    // into the correct memory range
    if (addressInRange(addr))
        RAM[addr] = data;
}

u_int8_t Bus::READ(u_int16_t addr) {
    if (addressInRange(addr))
        return RAM[addr];
    // If there is an illegal read
    return LOW;
}
