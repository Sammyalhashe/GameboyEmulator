#include <iostream>
#include <string>
#include <cstdint>
#include "CPU.h"
#include "Bus.h"

int main(int argc, char** argv) {
    setbuf(stdout, NULL);
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <rom_file> [--skip-boot] [--debug]" << std::endl;
        return 1;
    }

    std::string romPath = argv[1];
    bool skipBoot = false;
    bool debugMode = false;

    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--skip-boot") {
            skipBoot = true;
        } else if (std::string(argv[i]) == "--debug") {
            debugMode = true;
        }
    }

    Bus bus;
    bus.init(romPath, skipBoot, debugMode);
    bus.run();

    return 0;
}
