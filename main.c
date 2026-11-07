#include <raylib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t i16;
typedef int32_t i32;

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

#define INPUT_KEYS 16
#define INPUT_0 0x0
#define INPUT_1 0x1
#define INPUT_2 0x2
#define INPUT_3 0x3
#define INPUT_4 0x4
#define INPUT_5 0x5
#define INPUT_6 0x6
#define INPUT_7 0x7 
#define INPUT_8 0x8
#define INPUT_9 0x9
#define INPUT_A 0xA
#define INPUT_B 0xB
#define INPUT_C 0xC
#define INPUT_D 0xD
#define INPUT_E 0xE
#define INPUT_F 0xF

Sound c8_create_beep(float frequency, float duration) {
    int sampleRate = 44100;
    int sampleCount = (int)(sampleRate * duration);
    short *data = (short *)malloc(sampleCount * sizeof(short));

    for (int i = 0; i < sampleCount; i++) {
        // Fórmula de onda senoidal: sin(2 * PI * f * t)
        float t = (float)i / sampleRate;
        data[i] = (short)(sinf(2.0f * PI * frequency * t) * 16000.0f); // Volumen medio
    }

    Wave wave = {
        .frameCount = sampleCount,
        .sampleRate = sampleRate,
        .sampleSize = 16,
        .channels = 1,
        .data = data
    };

    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave); // Libera la copia de RAM, queda en la GPU/Audio
    return sound;
}

typedef struct IO {
    u8 display[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    u8 input[INPUT_KEYS];
} IO;

void c8_io_init(IO *io) {
    assert(io);
    *io = (IO){};
}

u8 c8_io_display_at(IO *io, u8 x, u8 y) {
    assert(io);
    assert(x < DISPLAY_WIDTH);
    assert(y < DISPLAY_HEIGHT);

    return io->display[y*DISPLAY_WIDTH + x];
}

bool c8_io_xor_at(IO *io, u8 x, u8 y, u8 n) {
    assert(io);
    assert(x < DISPLAY_WIDTH);
    assert(y < DISPLAY_HEIGHT);

    u8 before = c8_io_display_at(io,x,y);

    io->display[y*DISPLAY_WIDTH + x] = (io->display[y*DISPLAY_WIDTH + x] ^ n) & 0x1;

    u8 after = c8_io_display_at(io,x,y);

    return before == 1 && after == 0;
}

void c8_io_set_input(IO *io, u8 key, u8 pressed) {
    assert(io);
    assert(key < INPUT_KEYS);

    io->input[key] = pressed;
}

u8 c8_io_get_input(IO *io, u8 key) {
    assert(io);
    assert(key < INPUT_KEYS);

    return io->input[key];
}

void c8_io_clear_display(IO *io) {
    assert(io);

    memset(io->display,0,sizeof(io->display)*sizeof(*io->display));
}

#define SPRITE_SEGMENT_BASE 0
#define SPRITE_SEGMENT_SIZE 0x200
#define SPRITE_SIZE 16 

#define MEM_SIZE 4096

typedef struct Memory {
    u8 ram[MEM_SIZE];
} Memory;

void c8_mem_init(Memory *mem) {
    assert(mem);

    memset(mem,0,sizeof(mem->ram));
    
    const u8 FONTSET[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0 
        0x20, 0x60, 0x20, 0x20, 0x70, // 1 
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2 
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3 
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4 
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5 
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6 
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7 
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8 
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9 
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A 
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B 
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C 
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D 
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E 
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F 
    };
    memcpy(mem,FONTSET,sizeof(FONTSET));
}

void c8_mem_set(Memory *mem, u16 addr, u8 val) {
    assert(mem);
    assert(addr < MEM_SIZE);

    mem->ram[addr] = val;
}

void c8_mem_cpy(Memory *mem, u8 *cpy, u16 start, u16 size_cpy) {
    assert(mem);
    assert(cpy);
    assert(start < MEM_SIZE);
    assert(start+size_cpy < MEM_SIZE);

    memcpy(mem->ram+start,cpy,size_cpy);
}

u8 c8_mem_get(const Memory *mem, u16 addr) {
    assert(mem);
    assert(addr < MEM_SIZE);

    return mem->ram[addr];
}

#define PROGRAM_START 0x200
#define REGS_SIZE 16
#define STACK_SIZE 16
#define REG_VF 15

typedef struct Registers {
    u8 v[REGS_SIZE]; // general purpose regs. VF is not used, as it is used as a flag by some instructions
    u16 i; // to to store memory addresses. Its 12 bits, so the lower part of the register is used.

    u16 pc;

    u16 stack[STACK_SIZE];
    u8 sp; // stack is an array of 16 16-bit values

    u8 dt, st; // decrements 1 until dt/ds==0, in a 60 hz
} Registers;

void c8_cpu_init(Registers *reg) {
    assert(reg);

    *reg = (Registers){};

    reg->pc = PROGRAM_START;
}

void c8_clear_display(Image *display) {
    ImageColorTint(display, BLACK);
}

void c8_cpu_disasm(u16 pc, u16 instruction) {
    u8  op  = (instruction & 0xF000) >> 12;
    u8  x   = (instruction & 0x0F00) >> 8;
    u8  y   = (instruction & 0x00F0) >> 4;
    u8  n   =  instruction & 0x000F;
    u8  kk  =  instruction & 0x00FF;
    u16 nnn =  instruction & 0x0FFF;

    char text[64] = "UNKNOWN";

    switch (op) {
        case 0x0:
            if (instruction == 0x00E0) sprintf(text, "CLS");
            else if (instruction == 0x00EE) sprintf(text, "RET");
            else sprintf(text, "SYS 0x%03X", nnn);
            break;
        case 0x1: sprintf(text, "JP 0x%03X", nnn); break;
        case 0x2: sprintf(text, "CALL 0x%03X", nnn); break;
        case 0x3: sprintf(text, "SE V%X, 0x%02X", x, kk); break;
        case 0x4: sprintf(text, "SNE V%X, 0x%02X", x, kk); break;
        case 0x5: sprintf(text, "SE V%X, V%X", x, y); break;
        case 0x6: sprintf(text, "LD V%X, 0x%02X", x, kk); break;
        case 0x7: sprintf(text, "ADD V%X, 0x%02X", x, kk); break;
        case 0x8:
            switch (n) {
                case 0x0: sprintf(text, "LD V%X, V%X", x, y); break;
                case 0x1: sprintf(text, "OR V%X, V%X", x, y); break;
                case 0x2: sprintf(text, "AND V%X, V%X", x, y); break;
                case 0x3: sprintf(text, "XOR V%X, V%X", x, y); break;
                case 0x4: sprintf(text, "ADD V%X, V%X", x, y); break;
                case 0x5: sprintf(text, "SUB V%X, V%X", x, y); break;
                case 0x6: sprintf(text, "SHR V%X", x); break;
                case 0x7: sprintf(text, "SUBN V%X, V%X", x, y); break;
                case 0xE: sprintf(text, "SHL V%X", x); break;
                default:  sprintf(text, "8x%X%X", y, n); break;
            }
            break;
        case 0x9: sprintf(text, "SNE V%X, V%X", x, y); break;
        case 0xA: sprintf(text, "LD I, 0x%03X", nnn); break;
        case 0xB: sprintf(text, "JP V0, 0x%03X", nnn); break;
        case 0xC: sprintf(text, "RND V%X, 0x%02X", x, kk); break;
        case 0xD: sprintf(text, "DRW V%X, V%X, %d", x, y, n); break;
        case 0xE:
            if (kk == 0x9E) sprintf(text, "SKP V%X", x);
            else if (kk == 0xA1) sprintf(text, "SKNP V%X", x);
            break;
        case 0xF:
            switch (kk) {
                case 0x07: sprintf(text, "LD V%X, DT", x); break;
                case 0x0A: sprintf(text, "LD V%X, K", x); break;
                case 0x15: sprintf(text, "LD DT, V%X", x); break;
                case 0x18: sprintf(text, "LD ST, V%X", x); break;
                case 0x1E: sprintf(text, "ADD I, V%X", x); break;
                case 0x29: sprintf(text, "LD F, V%X", x); break;
                case 0x33: sprintf(text, "LD B, V%X", x); break;
                case 0x55: sprintf(text, "LD [I], V%X", x); break;
                case 0x65: sprintf(text, "LD V%X, [I]", x); break;
                default:   sprintf(text, "Fx%02X", kk); break;
            }
            break;
    }

    TraceLog(LOG_INFO, "[0x%03X] 0x%04X: %s", pc, instruction, text);
}

void c8_cpu_dump(const Registers *reg, const Memory *mem) {
    printf("\n==================== CHIP-8 CORE DUMP ====================\n");
    printf("PC: 0x%03X  |  I: 0x%03X  |  SP: 0x%02X  |  DT: %3d  |  ST: %3d\n",
           reg->pc, reg->i, reg->sp, reg->dt, reg->st);

    if (mem && reg->pc + 1 < MEM_SIZE) {
        u16 opcode = (mem->ram[reg->pc] << 8) | mem->ram[reg->pc + 1];
        printf("Opcode actual en PC: 0x%04X\n", opcode);
    }

    printf("\n--- REGISTROS V0 - VF ---\n");
    for (int idx = 0; idx < REGS_SIZE; idx++) {
        printf("V%X: 0x%02X (%3d) ", idx, reg->v[idx], reg->v[idx]);
        if ((idx + 1) % 4 == 0) printf("\n");
    }

    printf("\n--- CALL STACK (SP = %d) ---\n", reg->sp);
    if (reg->sp == 0) {
        printf("  (vacío)\n");
    } else {
        for (int idx = 0; idx < reg->sp && idx < STACK_SIZE; idx++) {
            printf("  [%2d] 0x%03X\n", idx, reg->stack[idx]);
        }
    }
    printf("==========================================================\n\n");
}

void c8_cpu_exe(Memory *mem, Registers *reg, IO *io) {
    u16 current_pc = reg->pc;
    u16 instruction = c8_mem_get(mem, reg->pc) << 8 | c8_mem_get(mem, reg->pc+1);

    u8  op  = (instruction & 0xF000) >> 12;
    u8  x   = (instruction & 0x0F00) >> 8;
    u8  y   = (instruction & 0x00F0) >> 4;
    u8  n   =  instruction & 0x000F;
    u8  kk  =  instruction & 0x00FF;
    u16 nnn =  instruction & 0x0FFF;

    reg->pc += 2;

    switch (op) {
        case 0x0:
            if (instruction == 0x00E0) { // 00E0 - CLS
                c8_io_clear_display(io);
            } else if (instruction == 0x00EE) { // 00EE - RET
                reg->sp--;
                reg->pc = reg->stack[reg->sp];
            }
            break;

        case 0x1: // 1nnn - JP addr
            reg->pc = nnn;
            break;

        case 0x2: // 2nnn - CALL addr
            reg->stack[reg->sp] = reg->pc;
            reg->sp++;
            reg->pc = nnn;
            break;

        case 0x3: { // 3xkk - SE Vx, byte
            if (reg->v[x] == kk) {
                reg->pc += 2;
            }
            break;
        }

        case 0x4: { // 4xkk - SNE Vx, byte
            u8 cmp = reg->v[x] - kk;
            if (reg->v[x] != kk) {
                reg->pc += 2;
            }
            break;
        }

        case 0x5: // 5xy0 - SE Vx, Vy
            if (n == 0) {
                if (reg->v[x] == reg->v[y]) {
                    reg->pc += 2;
                }
            }
            break;

        case 0x6: // 6xkk - LD Vx, byte
            reg->v[x] = kk;
            break;

        case 0x7: // 7xkk - ADD Vx, byte
            reg->v[x] += kk;
            break;

        case 0x8:
            switch (n) {
                case 0x0: // 8xy0 - LD Vx, Vy
                    reg->v[x] = reg->v[y];
                    break;
                case 0x1: // 8xy1 - OR Vx, Vy
                    reg->v[x] = reg->v[x] | reg->v[y];
                    break;
                case 0x2: // 8xy2 - AND Vx, Vy
                    reg->v[x] = reg->v[x] & reg->v[y];
                    break;
                case 0x3: // 8xy3 - XOR Vx, Vy
                    reg->v[x] = reg->v[x] ^ reg->v[y];
                    break;
                case 0x4: { // 8xy4 - ADD Vx, Vy
                    u16 sum = reg->v[x] + reg->v[y];
                    reg->v[x] = sum & 0xFFFF;
                    reg->v[REG_VF] = sum > 255;
                    break;
                }
                case 0x5: { // 8xy5 - SUB Vx, Vy
                    u8 not_borrow = reg->v[x] >= reg->v[y];
                    reg->v[x] = reg->v[x] - reg->v[y];
                    reg->v[REG_VF] = not_borrow;
                    break;
                }
                case 0x6: // 8xy6 - SHR Vx {, Vy}
                    u8 b_shft = reg->v[x] & 0x1;
                    reg->v[x] = reg->v[x] >> 1;
                    reg->v[REG_VF] = b_shft;
                    break;
                case 0x7: // 8xy7 - SUBN Vx, Vy
                    u8 b = reg->v[y] >= reg->v[x];
                    reg->v[x] = reg->v[y] - reg->v[x];
                    reg->v[REG_VF] = b;
                    break;
                case 0xE: // 8xyE - SHL Vx {, Vy}
                    u8 lsl = (reg->v[x] >> 7) == 1;
                    reg->v[x] = reg->v[x] << 1;
                    reg->v[REG_VF] = lsl;
                    break;
            }
            break;

        case 0x9: // 9xy0 - SNE Vx, Vy
            if (n == 0x0) {
                if (reg->v[x] != reg->v[y]) {
                    reg->pc += 2;
                }
            }
            break;

        case 0xA: // Annn - LD I, addr
            reg->i = nnn;
            break;

        case 0xB: // Bnnn - JP V0, addr
            reg->pc = reg->v[0] + nnn;
            break;

        case 0xC: // Cxkk - RND Vx, byte
            reg->v[x] = kk & GetRandomValue(0,255);
            break;

        case 0xD: { // Dxyn - DRW Vx, Vy, nibble
            u8 start_x = reg->v[x];
            u8 start_y = reg->v[y];
            
            reg->v[REG_VF] = 0;

            for (u16 row = 0; row < n; row++) {
                u16 real_mem = reg->i + row;
                u8 row_sprite = c8_mem_get(mem, real_mem);

                for (u8 column = 0; column < 8; column++) {
                    u8 real_x = (start_x+column) % DISPLAY_WIDTH;
                    u8 real_y = (start_y+row) % DISPLAY_HEIGHT;

                    u8 pixel = (row_sprite >> (7 - column)) & 1;
                    u8 changed = c8_io_xor_at(io, real_x, real_y, pixel);
                    if (changed) {
                        reg->v[REG_VF] = 1;
                    }
                }
            }
            break;
        }

        case 0xE:
            switch (kk) {
                case 0x9E: { // Ex9E - SKP Vx
                    u16 Vx = reg->v[x];
                    if (c8_io_get_input(io, Vx)) {
                        reg->pc += 2;
                    }
                    break;
                }
                case 0xA1: { // ExA1 - SKNP Vx
                    u16 Vx = reg->v[x];
                    if (!c8_io_get_input(io, Vx)) {
                        reg->pc += 2;
                    }
                    break;
                }
            }
            break;

        case 0xF:
            switch (kk) {
                case 0x07: // Fx07 - LD Vx, DT
                    reg->v[x] = reg->dt;
                    break;
                case 0x0A: { // Fx0A - LD Vx, K
                    bool found = false;
                    for (u8 k = 0; k < INPUT_KEYS; k++) {
                        if (c8_io_get_input(io, k)) {
                            reg->v[x] = k;
                            found = true;
                            break;
                        }
                    }            
                    if (!found) {
                        reg->pc -= 2;
                    }     
                    break;
                }
                case 0x15: // Fx15 - LD DT, Vx
                    reg->dt = reg->v[x];
                    break;
                case 0x18: // Fx18 - LD ST, Vx
                    reg->st = reg->v[x];
                    break;
                case 0x1E: // Fx1E - ADD I, Vx
                    reg->i += reg->v[x];
                    break;
                case 0x29: // Fx29 - LD F, Vx
                    reg->i = reg->v[x] * 5;
                    break;
                case 0x33: // Fx33 - LD B, Vx
                    mem->ram[reg->i]     = reg->v[x] / 100;
                    mem->ram[reg->i + 1] = (reg->v[x] / 10) % 10;
                    mem->ram[reg->i + 2] = reg->v[x] % 10;
                    break;
                case 0x55: // Fx55 - LD [I], Vx
                    memcpy(mem->ram+reg->i, reg->v,x+1);
                    break;
                case 0x65: // Fx65 - LD Vx, [I]
                    memcpy(reg->v,mem->ram+reg->i,x+1);
                    break;
            }
            break;
    }
    reg->pc &= 0xFFF;
}


void c8_cpu_update_timers(Registers *reg, Sound beep) {
    if (reg->dt > 0) reg->dt--;
    if (reg->st > 0) {
        if (!IsSoundPlaying(beep)) {
            PlaySound(beep);
        }
        reg->st--;
    }

    if (reg->st == 0 && IsSoundPlaying(beep)) {
         StopSound(beep);
    }
}

static const int KEYMAP[INPUT_KEYS] = {                                                                                                                               
    KEY_X,    // 0x0
    KEY_ONE,  // 0x1
    KEY_TWO,  // 0x2
    KEY_THREE,// 0x3
    KEY_Q,    // 0x4
    KEY_W,    // 0x5
    KEY_E,    // 0x6
    KEY_A,    // 0x7
    KEY_S,    // 0x8
    KEY_D,    // 0x9
    KEY_Z,    // 0xA
    KEY_C,    // 0xB
    KEY_FOUR, // 0xC
    KEY_R,    // 0xD
    KEY_F,    // 0xE
    KEY_V     // 0xF
}; 


#define WINDOW_W 800
#define WINDOW_H 400

int main(int argc, char *argv[]) {
    Memory mem = {};
    Registers reg = {};
    IO io = {};        

    c8_mem_init(&mem);
    c8_cpu_init(&reg);
    c8_io_init(&io);

    if (argc == 1) {
        c8_mem_set(&mem, 0x200, 0x12);
        c8_mem_set(&mem, 0x201, 0x00);
    } else if (argc == 2) {
        if (!IsPathFile(argv[1])) {
            TraceLog(LOG_ERROR, "Path to .ch8 is not a file");
            return 1;
        } 

        const char *ext = GetFileExtension(argv[1]);
        if (!TextIsEqual(ext,".ch8")) {
            TraceLog(LOG_ERROR, "File does not have the extension .ch8");
            return 1;
        }

        int size;
        unsigned char *data = LoadFileData(argv[1], &size);
        
        c8_mem_cpy(&mem, data, 0x200, size);

        UnloadFileData(data);
    } else {
        TraceLog(LOG_ERROR, "Use: %s <ruta_a_rom.ch8>\n", argv[0]);
        return 1;
    }

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(WINDOW_W, WINDOW_H, "CHIP-8");
    SetTargetFPS(60);
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    InitAudioDevice();

    Image screen = GenImageColor(DISPLAY_WIDTH, DISPLAY_HEIGHT, BLACK);
    Texture2D texture = LoadTextureFromImage(screen);
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    Sound beep = c8_create_beep(500,1);

    #define INSTRUCTIONS_PER_FRAME 15 // 10 * 60 FPS = 600 Hz

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F1)) {
            c8_cpu_dump(&reg, &mem);
        }

        for (u8 k = 0; k < INPUT_KEYS; k++) {             
            c8_io_set_input(&io, k, IsKeyDown(KEYMAP[k]));
        }

        for (int i = 0; i < INSTRUCTIONS_PER_FRAME; i++) {
            c8_cpu_exe(&mem, &reg, &io);
        }

        for (int y = 0; y < DISPLAY_HEIGHT; y++) {
            for (int x = 0; x < DISPLAY_WIDTH; x++) {
                Color c = c8_io_display_at(&io, x, y) ? WHITE : BLACK;
                ImageDrawPixel(&screen, x, y, c);
            }
        }
        UpdateTexture(texture, screen.data);
  
        c8_cpu_update_timers(&reg, beep); 

        BeginDrawing();
            ClearBackground(BLACK);

            DrawTexturePro(
                texture,
                (Rectangle){ 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT },
                (Rectangle){ 0, 0, GetRenderWidth(), GetRenderHeight() },
                (Vector2){ 0, 0 },
                0.0f,
                WHITE
            );
        EndDrawing();
    }

    UnloadTexture(texture);
    UnloadImage(screen);
    UnloadSound(beep);
    CloseAudioDevice();
    CloseWindow();
    return 0;

}

