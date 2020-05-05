FUNC = "RES"
BIT = True
COMMENT = False

REG_MAPPINGS = {
    "A": "af",
    "B": "bc",
    "C": "bc",
    "D": "de",
    "E": "de",
    "F": "af",
    "L": "hl",
    "H": "hl",
    "HL": "hl"
}

REGS = "B,C,D,E,H,L,HL,A"
BITS = "0,1,2,3,4,5,6,7"


def generate_comment(bit, REG, addr=False):
    return """// Test bit {0} in {1}, set the zero flag if bit not set.
    // {2} cycles
    // Z if selected bit it zero, N unset, H set
    """.format(bit, REG, 2 if not addr else 3)


def generate_code(func, bit, REG, addr=False):
    return """CPU::OPCODE CPU::{5}_{0}{1}_{2}() {{
        return {3}({0}{4});
    }}
    """.format(bit, "_Addr" if addr else "", REG,
               "{}_u3_REG8".format(func)
               if not addr else "{}_u3_Addr_REG16".format(func), ", regs." + REG_MAPPINGS[REG]
               + "." + REG, func)


with open("../RES_u3_REG8", "w") as f:

    if BIT:
        for bit in BITS.split(","):
            for reg in REGS.split(","):
                addr = len(reg) > 1
                if COMMENT:
                    f.write(generate_comment(bit, reg, addr) +
                            generate_code(FUNC, bit, reg, addr))
                else:
                    f.write(generate_code(FUNC, bit, reg, addr))
    else:
        for reg in REGS.split(","):
            addr = len(reg) > 1
            f.write(generate_code(FUNC, bit, reg, addr))
