# Interrupts

CPU-16 uses a vectored, priority-based interrupt system. The I flag in FL globally enables and disables maskable interrupts. The P flag tracks privilege level (user/kernel).

---

## Interrupt types

| Type | Maskable by I flag | Source |
|------|--------------------|--------|
| Hardware IRQ | Yes | Peripherals: timer, keyboard, video V-blank |
| NMI | No | Non-maskable hardware fault (reserved for future use) |
| TRAP | No | `TRAP #n` instruction — user-to-kernel call |
| Fault | No | Privilege violation, undefined instruction |

Maskable interrupts are ignored when FL.I = 0. They remain pending and are delivered as soon as I is set to 1 (e.g., after STI). Non-maskable sources (NMI, TRAP, Fault) are always delivered regardless of I.

---

## Interrupt vector table

Vectors live in the zero page at 0x0000–0x001F: 16 entries, 2 bytes each. Each entry holds the 16-bit address of the handler routine.

| Vector | Address | Source |
|--------|---------|--------|
| 0 | 0x0000 | Reset |
| 1 | 0x0002 | NMI |
| 2 | 0x0004 | Fault (privilege violation / undefined instruction) |
| 3 | 0x0006 | TRAP (software interrupt) |
| 4 | 0x0008 | Timer IRQ |
| 5 | 0x000A | Keyboard IRQ |
| 6 | 0x000C | Video V-blank IRQ |
| 7–15 | 0x000E–0x001E | Reserved |

Lower vector numbers have higher priority. When multiple interrupt sources are pending simultaneously, the lowest-numbered vector is delivered first.

The vector table can be patched at runtime (it lives in writable RAM), which is useful for installing OS handlers after boot.

---

## Interrupt entry sequence

When an interrupt fires and can be accepted (maskable: I=1; non-maskable: always):

1. The CPU completes the current instruction.
2. FL is pushed onto the stack (preserves privilege level and interrupt-enable state).
3. PC is pushed onto the stack (the address of the next instruction to run after return).
4. FL.P is set to 1 (switch to kernel mode).
5. FL.I is cleared to 0 (disable maskable interrupts while handling this one).
6. PC is loaded from the vector table entry for this interrupt source.

The handler runs in kernel mode with interrupts disabled. It can re-enable interrupts with STI if it needs to be preemptible (reentrant interrupt handling).

---

## Interrupt return (IRET)

IRET reverses the entry sequence:

1. Pop FL from the stack (restores privilege level and I flag).
2. Pop PC from the stack (resumes the interrupted code).

This is atomic from the CPU's perspective — no interrupt can be taken between the two pops. IRET is a privileged instruction and traps in user mode.

After IRET, if the restored I flag is 1, the CPU will immediately accept any pending maskable interrupt before fetching the next instruction.

---

## Handler skeleton

```asm
; Timer IRQ handler — vector 4, address stored at 0x0008
timer_handler:
    PUSH  R1              ; save registers clobbered by this handler
    PUSH  R2

    ; ... handle the timer event ...
    ; read TMR_CTRL to clear the EXPIRED flag:
    LOAD  R1, [0xFF22]

    POP   R2
    POP   R1
    IRET
```

The handler must save and restore every register it uses. There is no automatic register save/restore on interrupt entry.

---

## TRAP — software interrupt

`TRAP #n` is the user-mode path into the kernel. It behaves like a hardware interrupt: FL and PC are pushed, P is set to 1, I is cleared, and vector 3 is dispatched.

The 3-bit n value (0–7) is encoded in the instruction. By convention, n identifies the service class; additional arguments go in registers R4–R7.

```asm
; User-mode kernel call (example convention)
MOV   R4, #1            ; argument 1
MOV   R5, #buffer       ; argument 2
TRAP  #0                ; kernel service class 0
; return value in R1; errors in R2
```

The kernel's TRAP handler (vector 3) reads n from the saved instruction (via the saved PC) or from a register convention to dispatch to the right service routine.

---

## Fault handling

A privilege violation (executing a privileged instruction in user mode) or an undefined instruction dispatches through vector 2. The fault handler runs in kernel mode.

The pushed PC points to the faulting instruction (not the instruction after it), allowing the handler to inspect or retry it.

---

## Enabling and disabling interrupts

```asm
STI    ; FL.I = 1 — enable maskable interrupts   (privileged)
CLI    ; FL.I = 0 — disable maskable interrupts  (privileged)
```

On reset, FL.I = 0. The boot sequence enables interrupts after initializing the vector table and peripherals.

Interrupt-enable state is saved and restored through the stack on every interrupt entry and IRET, so nested interrupts (if the handler calls STI) work correctly.

---

## Priority summary

```
Reset (0) — highest priority
NMI (1)
Fault (2)
TRAP (3)
Timer IRQ (4)
Keyboard IRQ (5)
Video V-blank IRQ (6)
Reserved (7–15) — lowest priority
```
