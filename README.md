# Ceres

A fictional 16-bit home computer designed from scratch — architecture, emulator, assembler, and eventually a compiler for a custom programming language.

This project is built incrementally, decision by decision, with full documentation of every design choice and tradeoff. The goal is a complete, internally consistent computer that can be emulated in software and understood top to bottom.

---

## Project status

This project is in active design. The table below tracks what exists so far.

| Component | Status | Notes |
|---|---|---|
| CPU architecture | 🟡 In progress | Registers, flags, encoding locked |
| Instruction set | 🔴 Not started | Opcode table being designed |
| Memory map | 🟡 In progress | Preliminary layout defined |
| Graphics system | 🟡 In progress | VRAM model locked, registers TBD |
| Emulator | 🔴 Not started | — |
| Assembler | 🔴 Not started | — |
| BASIC interpreter | 🔴 Not started | — |
| Compiler | 🔴 Not started | Future milestone |

---

## The machine, briefly

| Property | Value |
|---|---|
| Word size | 16-bit |
| Address space | 64 KB flat (banking optional later) |
| General registers | 8 × 16-bit (R0–R7), R0 hardwired zero |
| Special registers | PC, SP, FL (flags) |
| Flags | Z, N, C, V, I, P |
| Instruction encoding | Variable: 16-bit or 32-bit |
| Addressing modes | 7 (register, immediate, indirect, reg+offset, absolute, PC-relative, reg+reg) |
| Stack | Full descending, SP starts at 0xFFFF |
| Display | 320×200, 256-color palette, framebuffer |
| VRAM | 64 KB dedicated, outside CPU address space |
| Audio | TBD |
| Storage | TBD |

Full documentation lives in [`docs/`](docs/).

---

## Roadmap

**Phase 1 — Architecture (now)**
- [x] CPU registers and flags
- [x] Instruction encoding strategy
- [x] Addressing modes
- [x] Memory map (preliminary)
- [x] Graphics model
- [ ] Complete ISA opcode table
- [ ] Interrupt system
- [ ] Audio system
- [ ] I/O system

**Phase 2 — Emulator**
- [ ] CPU execution loop
- [ ] Memory subsystem
- [ ] Graphics rendering
- [ ] Keyboard input
- [ ] Debugger

**Phase 3 — Assembler**
- [ ] Lexer and parser
- [ ] Symbol resolution
- [ ] Binary output format
- [ ] Listing output

**Phase 4 — Interpreter**
- [ ] BASIC-style language
- [ ] ROM image

**Phase 5 — Compiler**
- [ ] Custom language design
- [ ] Compiler frontend
- [ ] Code generation targeting CPU-16

---

## Documentation

All documentation is in `docs/`. Start with [`docs/architecture/overview.md`](docs/architecture/overview.md) for the big picture, then drill into specific subsystems.

---

## Contributing

This is a personal design and learning project. Notes, questions, and spotted inconsistencies are welcome as issues.

---

## License

Architecture specification, documentation, and all original source code: MIT.