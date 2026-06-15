#pragma once
#include <cstdint>
#include <cstring>

class Memory {
public:
    static constexpr uint16_t ROM_START  = 0x8000;
    static constexpr uint16_t ROM_END    = 0xBFFF;
    static constexpr uint16_t IO_START   = 0xFF00;
    static constexpr uint16_t STACK_INIT = 0x01FF;

    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 200;

    uint8_t ram[65536]      = {};
    uint8_t vram[65536]     = {};
    uint8_t palette[256][3] = {};  // [index][0=R, 1=G, 2=B]

    bool reset_requested = false;

    uint16_t read16(uint16_t addr);
    void     write16(uint16_t addr, uint16_t val);

    void load_rom(const uint8_t* data, size_t size);
    void tick(int cycles);                       // advance timer by N cycles
    void push_key(uint8_t scancode, bool down);  // feed a keyboard event

    uint8_t get_pending_irqs() const { return pending_irqs_; }
    void    clear_irq(int vector)    { pending_irqs_ &= (uint8_t)~(1 << vector); }

private:
    // Video controller
    uint16_t vid_addr_ = 0;
    uint16_t vid_ctrl_ = 0;
    uint8_t  pal_idx_  = 0;

    // Keyboard
    uint8_t kbd_data_   = 0;
    uint8_t kbd_status_ = 0;  // bit0=available, bit1=down
    uint8_t kbd_ctrl_   = 0;  // bit0=irq enable

    // Timer
    uint16_t tmr_count_  = 0;
    uint16_t tmr_reload_ = 0;
    uint8_t  tmr_ctrl_   = 0;  // bit0=enable, bit1=periodic, bit2=irq_en, bit3=expired

    uint8_t pending_irqs_ = 0;  // bit N = vector N is pending

    uint16_t io_read(uint16_t addr);
    void     io_write(uint16_t addr, uint16_t val);
};
