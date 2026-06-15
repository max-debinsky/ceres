// CPU-16 Assembler
//
// Pipeline:
//   source text  ──[Lexer]──>  token list
//   token list   ──[Parser]──> node list
//   node list    ──[Pass 1]──> label table + per-node addresses/sizes
//   node list    ──[Pass 2]──> binary output

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

using u8  = uint8_t;
using u16 = uint16_t;
using i32 = int32_t;

// ============================================================
// STAGE 1 — LEXER
// ============================================================
// The lexer turns raw source text into a flat list of typed
// tokens. It knows nothing about grammar — it just classifies
// character sequences into kinds.
//
// Token kinds:
//   IDENT      — any bare word: mnemonic, directive (.org), or label ref
//   REG        — R0..R7, ival = register number 0-7
//   IMM        — #value, ival = parsed integer
//   LABEL_DEF  — foo: (the definition site of a label)
//   LBRACK     — [
//   RBRACK     — ]
//   PLUS       — +
//   MINUS      — -
//   COMMA      — ,
//   NEWLINE    — end of a logical source line
//   END        — end of input
// ============================================================

enum class TK {
    IDENT,
    REG,
    IMM,
    LABEL_DEF,
    LBRACK,
    RBRACK,
    PLUS,
    MINUS,
    COMMA,
    NEWLINE,
    END,
};

struct Token {
    TK          kind;
    std::string text;  // label/ident name, or raw text
    i32         ival;  // register 0-7, or immediate value
    int         line;  // 1-based source line number
};

static std::vector<Token> lex(const char* src, int len) {
    std::vector<Token> out;
    int i = 0, line = 1;

    auto push = [&](TK k, std::string t = {}, i32 v = 0) {
        out.push_back({k, std::move(t), v, line});
    };

    while (i < len) {
        char c = src[i];

        // Skip spaces and tabs (newline is significant)
        if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }

        // Semicolon starts a comment — skip to end of line
        if (c == ';') { while (i < len && src[i] != '\n') ++i; continue; }

        if (c == '\n') { push(TK::NEWLINE); ++line; ++i; continue; }

        // Single-character punctuation
        if (c == '[') { push(TK::LBRACK); ++i; continue; }
        if (c == ']') { push(TK::RBRACK); ++i; continue; }
        if (c == '+') { push(TK::PLUS);   ++i; continue; }
        if (c == '-') { push(TK::MINUS);  ++i; continue; }
        if (c == ',') { push(TK::COMMA);  ++i; continue; }

        // Immediate: # followed by optional sign and a number literal.
        // Supports decimal, hex (0x), and binary (0b).
        if (c == '#') {
            ++i;
            bool neg = false;
            if (i < len && src[i] == '-') { neg = true; ++i; }
            else if (i < len && src[i] == '+') { ++i; }

            i32 val = 0;
            if (i + 1 < len && src[i] == '0' &&
                (src[i+1] == 'x' || src[i+1] == 'X')) {
                i += 2;
                while (i < len && isxdigit(src[i])) {
                    val = val * 16 + (isdigit(src[i]) ? src[i] - '0'
                                                       : tolower(src[i]) - 'a' + 10);
                    ++i;
                }
            } else if (i + 1 < len && src[i] == '0' &&
                       (src[i+1] == 'b' || src[i+1] == 'B')) {
                i += 2;
                while (i < len && (src[i] == '0' || src[i] == '1')) {
                    val = val * 2 + (src[i] - '0');
                    ++i;
                }
            } else {
                while (i < len && isdigit(src[i])) {
                    val = val * 10 + (src[i] - '0');
                    ++i;
                }
            }
            push(TK::IMM, {}, neg ? -val : val);
            continue;
        }

        // Word: starts with letter, underscore, or dot (for directives like .org)
        if (isalpha(c) || c == '_' || c == '.') {
            int start = i;
            while (i < len && (isalnum(src[i]) || src[i] == '_' || src[i] == '.'))
                ++i;
            std::string word(src + start, i - start);

            // A word followed by ':' is a label definition
            if (i < len && src[i] == ':') {
                ++i;
                push(TK::LABEL_DEF, word);
                continue;
            }

            // R0..R7 are registers (case-insensitive)
            if (word.size() == 2 &&
                (word[0] == 'R' || word[0] == 'r') &&
                word[1] >= '0' && word[1] <= '7') {
                push(TK::REG, word, word[1] - '0');
                continue;
            }

            // Everything else: mnemonic or label reference — parser decides
            push(TK::IDENT, word);
            continue;
        }

        fprintf(stderr, "lex error line %d: unexpected char 0x%02X\n",
                line, (unsigned char)c);
        ++i;
    }

    push(TK::END);
    return out;
}

// ============================================================
// DEBUG — print a token list
// ============================================================

static const char* tk_name(TK k) {
    switch (k) {
        case TK::IDENT:     return "IDENT";
        case TK::REG:       return "REG";
        case TK::IMM:       return "IMM";
        case TK::LABEL_DEF: return "LABEL_DEF";
        case TK::LBRACK:    return "LBRACK";
        case TK::RBRACK:    return "RBRACK";
        case TK::PLUS:      return "PLUS";
        case TK::MINUS:     return "MINUS";
        case TK::COMMA:     return "COMMA";
        case TK::NEWLINE:   return "NEWLINE";
        case TK::END:       return "END";
    }
    return "?";
}

static void print_tokens(const std::vector<Token>& toks) {
    for (const auto& t : toks) {
        printf("line %2d  %-12s", t.line, tk_name(t.kind));
        if (!t.text.empty())          printf("  \"%s\"", t.text.c_str());
        if (t.kind == TK::REG)        printf("  R%d", t.ival);
        if (t.kind == TK::IMM)        printf("  %d  (0x%04X)", t.ival, (u16)t.ival);
        putchar('\n');
    }
}

// ============================================================
// MAIN — read source file, lex it, print tokens
// (parser/pass1/pass2 will be added next)
// ============================================================

static char* read_file(const char* path, int* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); return nullptr; }
    fseek(f, 0, SEEK_END);
    int len = (int)ftell(f);
    rewind(f);
    char* buf = (char*)malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    *out_len = len;
    return buf;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: asm <source.asm> [output.rom]\n");
        return 1;
    }

    int len = 0;
    char* src = read_file(argv[1], &len);
    if (!src) return 1;

    auto tokens = lex(src, len);
    free(src);

    // For now: print the token stream so we can verify the lexer.
    // Parser and binary output come next.
    print_tokens(tokens);

    return 0;
}
