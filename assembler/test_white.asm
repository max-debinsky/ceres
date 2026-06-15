; White screen test — same program as gen_test_rom.cpp, but as source text

        ; palette[0] = black
        STORE   [#0xFF03], R0   ; VID_PAL_IDX = 0
        STORE   [#0xFF04], R0   ; R = 0
        STORE   [#0xFF05], R0   ; G = 0
        STORE   [#0xFF06], R0   ; B = 0

        ; palette[1] = white
        ADDI    R1, R0, #1      ; R1 = 1
        STORE   [#0xFF03], R1   ; VID_PAL_IDX = 1
        ALUL    ADD, R3, R0, #255   ; R3 = 255
        STORE   [#0xFF04], R3   ; R = 255
        STORE   [#0xFF05], R3   ; G = 255
        STORE   [#0xFF06], R3   ; B = 255

        ; fill VRAM
        STORE   [#0xFF01], R0   ; VID_ADDR = 0
        ALUL    ADD, R2, R0, #64000  ; R2 = 64000
        ADDI    R4, R0, #1      ; R4 = 1 (fill color)

fill_loop:
        STORE   [#0xFF02], R4   ; vram[VID_ADDR++] = R4
        ADDI    R2, R2, #-1     ; R2--
        BNE     fill_loop       ; loop while R2 != 0

        STI
        HALT
