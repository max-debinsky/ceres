#include "memory.h"

// Little-endian: low byte at addr, high byte at addr+1.
// addr+1 wraps around within 16-bit space.

uint16_t Memory::read16(uint16_t addr) {
    if (addr >= IO_START) return io_read(addr);
    return (uint16_t)ram[addr] | ((uint16_t)ram[(uint16_t)(addr + 1)] << 8);
}

void Memory::write16(uint16_t addr, uint16_t val) {
    if (addr >= ROM_START && addr <= ROM_END) return; // ROM: silently ignore
    if (addr >= IO_START) { io_write(addr, val); return; }
    ram[addr]                    = (uint8_t)(val & 0xFF);
    ram[(uint16_t)(addr + 1)]    = (uint8_t)(val >> 8);
}

void Memory::load_rom(const uint8_t* data, size_t size) {
    size_t copy = (size < 16384u) ? size : 16384u;
    memcpy(&ram[ROM_START], data, copy);
}

void Memory::tick(int cycles) {
    if (!(tmr_ctrl_ & 0x01)) return; // timer not enabled

    if (tmr_count_ <= (uint16_t)cycles) {
        tmr_ctrl_ |= 0x08; // set EXPIRED
        if (tmr_ctrl_ & 0x04) pending_irqs_ |= (1 << 4); // timer IRQ = vector 4

        if (tmr_ctrl_ & 0x02) {
            // periodic: reload and continue
            tmr_count_ = tmr_reload_;
        } else {
            // one-shot: stop
            tmr_count_ = 0;
            tmr_ctrl_ &= (uint8_t)~0x01;
        }
    } else {
        tmr_count_ -= (uint16_t)cycles;
    }
}

void Memory::push_key(uint8_t scancode, bool down) {
    kbd_data_   = scancode;
    kbd_status_ = 0x01 | (down ? 0x02 : 0x00);
    if (kbd_ctrl_ & 0x01) pending_irqs_ |= (1 << 5); // keyboard IRQ = vector 5
}

// ---------------------------------------------------------------------------
// MMIO
// ---------------------------------------------------------------------------

uint16_t Memory::io_read(uint16_t addr) {
    switch (addr) {
        case 0xFF00: return vid_ctrl_;
        case 0xFF01: return vid_addr_;
        case 0xFF02: {
            uint8_t b = vram[vid_addr_++];
            return b;
        }
        case 0xFF07: return 0; // VID_STATUS (vblank timing not yet implemented)
        case 0xFF10: {
            uint8_t d = kbd_data_;
            kbd_status_ &= (uint8_t)~0x01; // clear key-available on read
            return d;
        }
        case 0xFF11: return kbd_status_;
        case 0xFF20: return tmr_count_;
        case 0xFF21: return tmr_reload_;
        case 0xFF22: {
            uint8_t s = tmr_ctrl_;
            tmr_ctrl_ &= (uint8_t)~0x08; // clear EXPIRED on read
            return s;
        }
        default: return 0;
    }
}

void Memory::io_write(uint16_t addr, uint16_t val) {
    switch (addr) {
        case 0xFF00: vid_ctrl_ = val; break;
        case 0xFF01: vid_addr_ = val; break;
        case 0xFF02: vram[vid_addr_++] = (uint8_t)(val & 0xFF); break; // only low byte used
        case 0xFF03: pal_idx_ = (uint8_t)(val & 0xFF); break;
        case 0xFF04: palette[pal_idx_][0] = (uint8_t)(val & 0xFF); break; // R
        case 0xFF05: palette[pal_idx_][1] = (uint8_t)(val & 0xFF); break; // G
        case 0xFF06: palette[pal_idx_][2] = (uint8_t)(val & 0xFF); break; // B
        case 0xFF12: kbd_ctrl_ = (uint8_t)(val & 0xFF); break;
        case 0xFF20: tmr_count_  = val; break;
        case 0xFF21: tmr_reload_ = val; break;
        case 0xFF22: tmr_ctrl_   = (uint8_t)(val & 0xFF); break;
        case 0xFFFF: reset_requested = true; break;
        default: break;
    }
}
