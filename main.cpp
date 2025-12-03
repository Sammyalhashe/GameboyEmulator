#include <iostream>
#include <string>
#include <cstdint>
#include "CPU.h"
#include "Bus.h"

int main(int argc, char** argv) {
    setbuf(stdout, NULL);
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <rom_file> [--skip-boot]" << std::endl;
        return 1;
    }

    std::string romPath = argv[1];
    bool skipBoot = false;

    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--skip-boot") {
            skipBoot = true;
        }
    }

    Bus bus;
    bus.init(romPath, skipBoot);
    bus.run();

    return 0;
}
