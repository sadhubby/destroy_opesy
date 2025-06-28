typedef enum {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR
} InstructionType;

typedef struct {
    InstructionType type;
    char args[3][32];
    int repeat_count;
    int sub_count;
    struct Instruction *sub_instructions;
} Instruction;
