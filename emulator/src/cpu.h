#pragma once
#include <cstdint>
#include "memory.h"

class CPU {
public:
    // Flag bits within FL
    static constexpr uint16_t FL_Z = 1 << 0; // zero
    static constexpr uint16_t FL_N = 1 << 1; // negative
    static constexpr uint16_t FL_C = 1 << 2; // carry / no-borrow
    static constexpr uint16_t FL_V = 1 << 3; // signed overflow
    static constexpr uint16_t FL_I = 1 << 4; // interrupt enable
    static constexpr uint16_t FL_P = 1 << 5; // privilege (1 = kernel)

    uint16_t r[8] = {}; // r[0] is hardwired zero
    uint16_t pc   = Memory::ROM_START;
    uint16_t sp   = Memory::STACK_INIT;
    uint16_t fl   = FL_P; // start in kernel mode, interrupts disabled
    bool     halted = false;

    void reset();
    void step(Memory& mem);
    void dump() const; // print register state (debug)

private:
    // Register access respects the R0=0 invariant
    uint16_t rr(uint8_t idx) const { return (idx == 0) ? 0 : r[idx]; }
    void     wr(uint8_t idx, uint16_t val) { if (idx != 0) r[idx] = val; }

    // Sign-extend an n-bit value to int16_t
    static int16_t sext(uint16_t val, int bits) {
        uint16_t sign = (uint16_t)(1u << (bits - 1));
        return (int16_t)((val ^ sign) - sign);
    }

    // Flag helpers
    void set_flags_add(uint16_t a, uint16_t b, uint16_t result);
    void set_flags_sub(uint16_t a, uint16_t b, uint16_t result);
    void set_flags_logic(uint16_t result);
    void set_flags_shift(uint16_t result, bool carry);

    // Opcode handlers (one per table entry)
    void exec_alu   (uint16_t w);
    void exec_addi  (uint16_t w);
    void exec_shift (uint16_t w);
    void exec_shifti(uint16_t w);
    void exec_alul  (uint16_t w, Memory& mem);
    void exec_load  (uint16_t w, Memory& mem);
    void exec_store (uint16_t w, Memory& mem);
    void exec_stack (uint16_t w, Memory& mem);
    void exec_spreg (uint16_t w);
    void exec_jmp   (uint16_t w);
    void exec_jmpx  (uint16_t w, Memory& mem);
    void exec_jmpl  (uint16_t w, Memory& mem);
    void exec_bcc   (uint16_t w);
    void exec_sys   (uint16_t w, Memory& mem);

    // Shared shift logic (used by both exec_shift and exec_shifti)
    void do_shift(uint8_t rd, uint16_t a, uint8_t type, uint8_t n);

    // Interrupt machinery
    void deliver_interrupt(uint8_t vector, Memory& mem);
    void check_interrupts(Memory& mem);
    void trigger_fault(Memory& mem);
};
