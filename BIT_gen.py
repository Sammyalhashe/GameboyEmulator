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



def generate_comment(bit, REG, addr = False):
    return """// Test bit {0} in {1}, set the zero flag if bit not set.
    // {2} cycles
    // Z if selected bit it zero, N unset, H set
    """.format(bit, REG, 2 if not addr else 3)

def generate_code(bit, REG, addr = False):
    return """CPU::OPCODE CPU::BIT_{0}{1}_{2}() {{
        return {3}({0}{4});
    }}
    """.format(bit, "_Addr" if addr else "", REG, "BIT_u3_REG8" if not addr else "BIT_u3_Addr_HL", ", regs." + REG_MAPPINGS[REG] + "." + REG if not addr else "")


with open("BIT_u3_REG8", "w") as f:

    for bit in BITS.split(","):
        for reg in REGS.split(","):
            addr = len(reg) > 1
            f.write(generate_comment(bit, reg, addr) + generate_code(bit, reg, addr))
