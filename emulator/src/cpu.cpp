#include "cpu.h"
#include <cstdio>

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void CPU::reset() {
    for (auto& reg : r) reg = 0;
    pc     = Memory::ROM_START;
    sp     = Memory::STACK_INIT;
    fl     = FL_P; // kernel mode, interrupts disabled
    halted = false;
}

void CPU::step(Memory& mem) {
    if (halted) {
        mem.tick(1);
        check_interrupts(mem);
        return;
    }

    uint16_t w = mem.read16(pc);
    pc += 2;

    switch ((w >> 12) & 0xF) {
        case 0x0: exec_alu   (w);       break;
        case 0x1: exec_addi  (w);       break;
        case 0x2: exec_shift (w);       break;
        case 0x3: exec_shifti(w);       break;
        case 0x4: exec_alul  (w, mem);  break;
        case 0x5: exec_load  (w, mem);  break;
        case 0x6: exec_store (w, mem);  break;
        case 0x7: exec_stack (w, mem);  break;
        case 0x8: exec_spreg (w);       break;
        case 0x9: exec_jmp   (w);       break;
        case 0xA: exec_jmpx  (w, mem);  break;
        case 0xB: exec_jmpl  (w, mem);  break;
        case 0xC: exec_bcc   (w);       break;
        case 0xD: exec_sys   (w, mem);  break;
        default: trigger_fault(mem);    break; // 0xE, 0xF reserved
    }

    mem.tick(1); // simplified: 1 cycle per instruction
    check_interrupts(mem);
}

void CPU::dump() const {
    printf("PC=%04X SP=%04X FL=%04X [%c%c%c%c%c%c]\n",
        pc, sp, fl,
        (fl & FL_P) ? 'P' : '-',
        (fl & FL_I) ? 'I' : '-',
        (fl & FL_V) ? 'V' : '-',
        (fl & FL_C) ? 'C' : '-',
        (fl & FL_N) ? 'N' : '-',
        (fl & FL_Z) ? 'Z' : '-');
    for (int i = 0; i < 8; i++)
        printf("R%d=%04X%s", i, (i == 0 ? 0 : r[i]), (i == 7) ? "\n" : "  ");
}

// ---------------------------------------------------------------------------
// Flag helpers
// ---------------------------------------------------------------------------

void CPU::set_flags_add(uint16_t a, uint16_t b, uint16_t result) {
    fl &= (uint16_t)~(FL_Z | FL_N | FL_C | FL_V);
    if (result == 0)                              fl |= FL_Z;
    if (result & 0x8000)                          fl |= FL_N;
    if ((uint32_t)a + b > 0xFFFF)                fl |= FL_C;
    // V: both inputs same sign, result different sign
    if (!((a ^ b) & 0x8000) && ((a ^ result) & 0x8000)) fl |= FL_V;
}

void CPU::set_flags_sub(uint16_t a, uint16_t b, uint16_t result) {
    fl &= (uint16_t)~(FL_Z | FL_N | FL_C | FL_V);
    if (result == 0)                              fl |= FL_Z;
    if (result & 0x8000)                          fl |= FL_N;
    if (a >= b)                                   fl |= FL_C; // C=1 means no borrow
    // V: inputs have different signs and result sign differs from a
    if (((a ^ b) & 0x8000) && ((a ^ result) & 0x8000)) fl |= FL_V;
}

void CPU::set_flags_logic(uint16_t result) {
    fl &= (uint16_t)~(FL_Z | FL_N | FL_C | FL_V);
    if (result == 0)       fl |= FL_Z;
    if (result & 0x8000)   fl |= FL_N;
    // C and V cleared
}

void CPU::set_flags_shift(uint16_t result, bool carry) {
    fl &= (uint16_t)~(FL_Z | FL_N | FL_C);
    if (result == 0)     fl |= FL_Z;
    if (result & 0x8000) fl |= FL_N;
    if (carry)           fl |= FL_C;
    // V left undefined (unchanged)
}

// ---------------------------------------------------------------------------
// 0x0 — ALU (Format R)
// ---------------------------------------------------------------------------

void CPU::exec_alu(uint16_t w) {
    uint8_t  rd   = (w >> 9) & 7;
    uint8_t  ra   = (w >> 6) & 7;
    uint8_t  rb   = (w >> 3) & 7;
    uint8_t  func =  w       & 7;
    uint16_t a    = rr(ra);
    uint16_t b    = rr(rb);
    uint16_t res;

    switch (func) {
        case 0: res = a + b;    set_flags_add  (a, b, res); wr(rd, res); break; // ADD
        case 1: res = a - b;    set_flags_sub  (a, b, res); wr(rd, res); break; // SUB
        case 2: res = (uint16_t)((uint32_t)a * b);                               // MUL
                fl &= (uint16_t)~(FL_Z | FL_N);
                if (!res)          fl |= FL_Z;
                if (res & 0x8000)  fl |= FL_N;
                wr(rd, res); break;
        case 3: res = a & b;    set_flags_logic(res);        wr(rd, res); break; // AND
        case 4: res = a | b;    set_flags_logic(res);        wr(rd, res); break; // OR
        case 5: res = a ^ b;    set_flags_logic(res);        wr(rd, res); break; // XOR
        case 6: res = ~a;       set_flags_logic(res);        wr(rd, res); break; // NOT (rb ignored)
        case 7: res = a - b;    set_flags_sub  (a, b, res);               break; // CMP (discard)
    }
}

// ---------------------------------------------------------------------------
// 0x1 — ADDI (Format I)
// ---------------------------------------------------------------------------

void CPU::exec_addi(uint16_t w) {
    uint8_t  rd   = (w >> 9) & 7;
    uint8_t  ra   = (w >> 6) & 7;
    uint16_t b    = (uint16_t)sext(w & 0x3F, 6);
    uint16_t a    = rr(ra);
    uint16_t res  = a + b;
    set_flags_add(a, b, res);
    wr(rd, res);
}

// ---------------------------------------------------------------------------
// Shared shift logic
// ---------------------------------------------------------------------------

void CPU::do_shift(uint8_t rd, uint16_t a, uint8_t type, uint8_t n) {
    uint16_t res;
    bool     carry;

    if (n == 0) {
        res   = a;
        carry = (fl & FL_C) != 0; // C unchanged for shift-by-zero
    } else {
        switch (type & 3) {
            case 0: // LSL
                carry = (a >> (16 - n)) & 1;
                res   = a << n;
                break;
            case 1: // LSR
                carry = (a >> (n - 1)) & 1;
                res   = a >> n;
                break;
            case 2: // ASR
                carry = (a >> (n - 1)) & 1;
                res   = (uint16_t)((int16_t)a >> n);
                break;
            default: // ROR
                carry = (a >> (n - 1)) & 1;
                res   = (a >> n) | (uint16_t)(a << (16 - n));
                break;
        }
    }

    set_flags_shift(res, carry);
    wr(rd, res);
}

// ---------------------------------------------------------------------------
// 0x2 — SHIFT (Format R)
// ---------------------------------------------------------------------------

void CPU::exec_shift(uint16_t w) {
    uint8_t rd   = (w >> 9) & 7;
    uint8_t ra   = (w >> 6) & 7;
    uint8_t rb   = (w >> 3) & 7;
    uint8_t type =  w       & 3;
    uint8_t n    = rr(rb) & 0xF;
    do_shift(rd, rr(ra), type, n);
}

// ---------------------------------------------------------------------------
// 0x3 — SHIFTI (Format I)
// ---------------------------------------------------------------------------

void CPU::exec_shifti(uint16_t w) {
    uint8_t rd   = (w >> 9) & 7;
    uint8_t ra   = (w >> 6) & 7;
    uint8_t type = (w >> 4) & 3;
    uint8_t n    =  w       & 0xF;
    do_shift(rd, rr(ra), type, n);
}

// ---------------------------------------------------------------------------
// 0x4 — ALUL (Format L, 32-bit)
// ---------------------------------------------------------------------------

void CPU::exec_alul(uint16_t w, Memory& mem) {
    uint8_t  rd  = (w >> 9) & 7;
    uint8_t  ra  = (w >> 6) & 7;
    uint8_t  op  =  w       & 7;
    uint16_t imm = mem.read16(pc);
    pc += 2;

    uint16_t a   = rr(ra);
    uint16_t res;

    switch (op) {
        case 0: res = a + imm;  set_flags_add  (a, imm, res); wr(rd, res); break; // ADD
        case 1: res = a - imm;  set_flags_sub  (a, imm, res); wr(rd, res); break; // SUB
        case 2: res = a & imm;  set_flags_logic(res);          wr(rd, res); break; // AND
        case 3: res = a | imm;  set_flags_logic(res);          wr(rd, res); break; // OR
        case 4: res = a ^ imm;  set_flags_logic(res);          wr(rd, res); break; // XOR
        case 5: res = (uint16_t)((uint32_t)a * imm);                                // MUL
                fl &= (uint16_t)~(FL_Z | FL_N);
                if (!res)         fl |= FL_Z;
                if (res & 0x8000) fl |= FL_N;
                wr(rd, res); break;
        case 6: res = a - imm;  set_flags_sub  (a, imm, res);               break; // CMP
        default: break;
    }
}

// ---------------------------------------------------------------------------
// 0x5 — LOAD (Format M, 16 or 32-bit)
// ---------------------------------------------------------------------------

void CPU::exec_load(uint16_t w, Memory& mem) {
    uint8_t rd  = (w >> 9) & 7;
    uint8_t ra  = (w >> 6) & 7;
    bool    i16 = (w >> 5) & 1;

    uint16_t addr;
    if (i16) {
        // 16-bit form: [Ra + disp5]
        addr = (uint16_t)(rr(ra) + (uint16_t)sext(w & 0x1F, 5));
    } else {
        // 32-bit form: read second word, mode in bits 3:0
        uint16_t w2   = mem.read16(pc);
        pc += 2;
        uint8_t  mode = w & 0xF;
        switch (mode) {
            case 0: addr = w2;                                           break; // [#imm16]
            case 1: addr = (uint16_t)(rr(ra) + w2);                     break; // [Ra + imm16]
            case 2: addr = (uint16_t)(pc     + (int16_t)w2);            break; // [PC + imm16]
            case 3: addr = (uint16_t)(rr(ra) + rr(w2 & 7));             break; // [Ra + Rb]
            default: trigger_fault(mem); return;
        }
    }

    wr(rd, mem.read16(addr));
}

// ---------------------------------------------------------------------------
// 0x6 — STORE (Format M, 16 or 32-bit)
// ---------------------------------------------------------------------------

void CPU::exec_store(uint16_t w, Memory& mem) {
    uint8_t rs  = (w >> 9) & 7; // source register is in the Rd field
    uint8_t ra  = (w >> 6) & 7;
    bool    i16 = (w >> 5) & 1;

    uint16_t addr;
    if (i16) {
        addr = (uint16_t)(rr(ra) + (uint16_t)sext(w & 0x1F, 5));
    } else {
        uint16_t w2   = mem.read16(pc);
        pc += 2;
        uint8_t  mode = w & 0xF;
        switch (mode) {
            case 0: addr = w2;                                break;
            case 1: addr = (uint16_t)(rr(ra) + w2);          break;
            case 3: addr = (uint16_t)(rr(ra) + rr(w2 & 7));  break;
            default: trigger_fault(mem); return;
        }
    }

    mem.write16(addr, rr(rs));
}

// ---------------------------------------------------------------------------
// 0x7 — STACK (PUSH / POP)
// ---------------------------------------------------------------------------

void CPU::exec_stack(uint16_t w, Memory& mem) {
    bool is_pop = w & 1;
    if (!is_pop) {
        uint8_t ra = (w >> 6) & 7;
        sp -= 2;
        mem.write16(sp, rr(ra));
    } else {
        uint8_t rd = (w >> 9) & 7;
        wr(rd, mem.read16(sp));
        sp += 2;
    }
}

// ---------------------------------------------------------------------------
// 0x8 — SPREG (special register access)
// ---------------------------------------------------------------------------

void CPU::exec_spreg(uint16_t w) {
    uint8_t rd   = (w >> 9) & 7;
    uint8_t ra   = (w >> 6) & 7;
    uint8_t func =  w       & 7;

    switch (func) {
        case 0: wr(rd, sp); break;                             // GETSP
        case 1: sp = rr(ra); break;                            // SETSP
        case 2: wr(rd, pc); break;                             // GETPC (pc past this instr)
        case 3: wr(rd, fl); break;                             // GETFL
        case 4:                                                // SETFL (privileged)
            if (fl & FL_P) fl = rr(ra);
            // else: would trigger fault — simplify for now and silently ignore
            break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// 0x9 — JMP (Format J)
// ---------------------------------------------------------------------------

void CPU::exec_jmp(uint16_t w) {
    // pc has already been incremented past the instruction
    int16_t offset12 = sext(w & 0x0FFF, 12);
    pc = (uint16_t)(pc + offset12 * 2);
}

// ---------------------------------------------------------------------------
// 0xA — JMPX (register jump / call / ret / iret)
// ---------------------------------------------------------------------------

void CPU::exec_jmpx(uint16_t w, Memory& mem) {
    uint8_t ra   = (w >> 6) & 7;
    uint8_t func =  w       & 7;

    switch (func) {
        case 0: // JMP Rs
            pc = rr(ra);
            break;
        case 1: // CALL Rs
            sp -= 2;
            mem.write16(sp, pc); // pc already past this instruction
            pc = rr(ra);
            break;
        case 2: // RET
            pc = mem.read16(sp);
            sp += 2;
            break;
        case 3: // IRET (privileged)
            if (fl & FL_P) {
                fl = mem.read16(sp); sp += 2;
                pc = mem.read16(sp); sp += 2;
                halted = false;
            } else {
                trigger_fault(mem);
            }
            break;
        default: trigger_fault(mem); break;
    }
}

// ---------------------------------------------------------------------------
// 0xB — JMPL (Format L, 32-bit absolute jump / call)
// ---------------------------------------------------------------------------

void CPU::exec_jmpl(uint16_t w, Memory& mem) {
    bool     is_call = w & 1;
    uint16_t addr    = mem.read16(pc);
    pc += 2;

    if (is_call) {
        sp -= 2;
        mem.write16(sp, pc); // return address = instruction after the 32-bit CALL
        pc = addr;
    } else {
        pc = addr;
    }
}

// ---------------------------------------------------------------------------
// 0xC — BCC (Format B, conditional branch)
// ---------------------------------------------------------------------------

void CPU::exec_bcc(uint16_t w) {
    uint8_t  cond    = (w >> 9) & 7;
    int16_t  offset9 = sext(w & 0x1FF, 9);
    bool     take    = false;

    bool Z = (fl & FL_Z) != 0;
    bool N = (fl & FL_N) != 0;
    bool C = (fl & FL_C) != 0;
    bool V = (fl & FL_V) != 0;

    switch (cond) {
        case 0: take =  Z;         break; // BEQ
        case 1: take = !Z;         break; // BNE
        case 2: take = (N != V);   break; // BLT
        case 3: take = (N == V);   break; // BGE
        case 4: take = !C;         break; // BLO
        case 5: take =  C;         break; // BHS
        case 6: take =  N;         break; // BMI
        case 7: take = !N;         break; // BPL
    }

    if (take) pc = (uint16_t)(pc + offset9 * 2);
}

// ---------------------------------------------------------------------------
// 0xD — SYS (system instructions)
// ---------------------------------------------------------------------------

void CPU::exec_sys(uint16_t w, Memory& mem) {
    uint8_t param = (w >> 6) & 7;
    uint8_t func  =  w       & 7;

    switch (func) {
        case 0: // HALT (privileged)
            if (fl & FL_P) halted = true;
            else           trigger_fault(mem);
            break;
        case 1: // STI (privileged)
            if (fl & FL_P) fl |= FL_I;
            else           trigger_fault(mem);
            break;
        case 2: // CLI (privileged)
            if (fl & FL_P) fl &= (uint16_t)~FL_I;
            else           trigger_fault(mem);
            break;
        case 3: // TRAP #n
            (void)param; // n = param[2:0] — available to the fault handler via saved PC
            deliver_interrupt(3, mem);
            break;
        default: trigger_fault(mem); break;
    }
}

// ---------------------------------------------------------------------------
// Interrupt machinery
// ---------------------------------------------------------------------------

void CPU::deliver_interrupt(uint8_t vector, Memory& mem) {
    halted = false;
    sp -= 2; mem.write16(sp, fl);
    sp -= 2; mem.write16(sp, pc);
    fl |= FL_P;             // switch to kernel mode
    fl &= (uint16_t)~FL_I; // disable maskable interrupts
    pc = mem.read16(vector * 2);
}

void CPU::trigger_fault(Memory& mem) {
    deliver_interrupt(2, mem); // vector 2 = fault
}

void CPU::check_interrupts(Memory& mem) {
    uint8_t pending = mem.get_pending_irqs();
    if (!pending) return;

    // Vectors 4+ are maskable; only deliver if FL.I is set
    for (int v = 4; v < 16; v++) {
        if (pending & (1 << v)) {
            if (fl & FL_I) {
                mem.clear_irq(v);
                deliver_interrupt((uint8_t)v, mem);
                return;
            }
            break; // I flag is off; no maskable IRQ delivered this cycle
        }
    }
}
