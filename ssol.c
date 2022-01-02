#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *file;
    size_t line;
    size_t col;
} pos_t;

typedef struct {
    enum {
        TKN_ID,
        TKN_KEYWORD,
        TKN_INTRINSIC,
        TKN_INT,
        TKN_TYPE,
        TKN_COUNT
    } type;
    enum {
        OP_PUSH_INT,
        OP_PLUS,
        OP_MINUS,
        OP_MUL,
        OP_DIV,
        OP_MOD,
        OP_SHR,
        OP_SHL,
        OP_BAND,
        OP_BOR,
        OP_BNOT,
        OP_XOR,
        OP_PRINT,
        OP_DUP,
        OP_SWAP,
        OP_ROT,
        OP_DROP,
        OP_CAP,
        OP_START_INDEX,
        OP_END_INDEX,
        OP_EQUALS,
        OP_NOTEQUALS,
        OP_GREATER,
        OP_MINOR,
        OP_EQGREATER,
        OP_EQMINOR,
        OP_NOT,
        OP_IF,
        OP_DO,
        OP_ELSE,
        OP_LOOP,
        OP_CREATE_LABEL,
        OP_SET_LABEL,
        OP_CALL_LABEL,
        //OP_REPEAT,
        //OP_BREAK,
        OP_END,
        OP_SYSCALL0,
        OP_SYSCALL1,
        OP_SYSCALL2,
        OP_SYSCALL3,
        OP_SYSCALL4,
        OP_SYSCALL5,
        OP_SYSCALL6,
        OP_COUNT
    } operation;
    char *val;
    size_t jmp;
} token_t;

typedef struct {
    char *name;
    size_t size_bytes;
    int primitive;
    // TODO: add member labels to types
} labeltype_t;

typedef struct {
    char *name;
    size_t type;
    int arr;
    size_t cap;
} label_t;

typedef struct {
    size_t *stack;
    size_t size;
    size_t alloc;
} stack_t;

typedef struct {
    token_t *token_list;
    pos_t *pos_list;
    size_t token_size;
    size_t token_alloc;

    labeltype_t *labeltype_list;
    size_t labeltype_size;
    size_t labeltype_alloc;

    label_t *label_list;
    size_t label_size;
    size_t label_alloc;

    size_t idx;
    int error;
    int condition;
    int loop;
    int setting;
    int index;
    size_t idx_amount;
    size_t cur_label;
} program_t;

program_t program;

char token_name[TKN_COUNT][256] = {
    "id",
    "keyword",
    "intrinsic",
    "int",
    "type"
};

void malloc_check(void *block, char *info) {
    if (block == NULL) {
        fprintf(stderr, "[ERROR] malloc returned null\n[INFO] %s\n", info);
        exit(1);
    }
}

stack_t stack_create() {
    stack_t stack;
    stack.stack = malloc(sizeof(size_t));
    stack.size = 0;
    stack.alloc = 1;
    return stack;
}

void stack_push(stack_t *stack, size_t val) {
    stack->size++;
    if (stack->size >= stack->alloc) {
        stack->alloc *= 2;
        stack->stack = realloc(stack->stack, sizeof(size_t) * stack->alloc);
    }
    stack->stack[stack->size - 1] = val;
}

size_t stack_pop(stack_t *stack) {
    stack->size--;
    return stack->stack[stack->size];
}

void stack_destroy(stack_t stack) {
    free(stack.stack);
}

pos_t pos_create(char *file, size_t line, size_t col) {
    pos_t pos;
    pos.file = file;
    pos.line = line;
    pos.col = col;
    return pos;
}

token_t token_create() {
    token_t token = {.type = -1, .val = NULL};
    return token;
}

void token_set(token_t *token, int type, int operation, char *val) {
    token->type = type;
    token->operation = operation;
    token->val = val;
}

void print_token(token_t *token) {
    if (token->val != NULL) {
        printf("[%s, %s]\n", token_name[token->type], token->val);
    } else {
        printf("[%s]\n", token_name[token->type]);
    }
}

labeltype_t labeltype_create(char *name, size_t size_bytes, int primitive) {
    labeltype_t labeltype;
    labeltype.name = malloc(sizeof(*labeltype.name) * (strlen(name) + 1));
    malloc_check(labeltype.name, "malloc(labeltype.name) in function labeltype_create");
    strcpy(labeltype.name, name);
    labeltype.size_bytes = size_bytes;
    labeltype.primitive = primitive;
    return labeltype;
}

size_t find_labeltype(char *name) {
    size_t find = 0;
    for (size_t i = 0; i < program.labeltype_size; i++) {
        if (strcmp(program.labeltype_list[i].name, name) == 0) {
            find = i + 1;
            break;
        }
    }
    return find;
}

label_t label_create(char *name, char *type_name, int arr, size_t cap) {
    size_t type;
    type = find_labeltype(type_name);
    if (!type) return (label_t){.name=NULL};
    type--;
    label_t label;
    label.name = malloc(sizeof(*label.name) * (strlen(name) + 1));
    malloc_check(label.name, "malloc(label.name) in function label_create");
    strcpy(label.name, name);
    label.type = type;
    label.arr = arr;
    label.cap = cap;
    return label;
}

void program_add_token(pos_t pos) {
    program.token_size++;
    if (program.token_size >= program.token_alloc) {
        program.token_alloc *= 2;
        program.pos_list = realloc(program.pos_list, sizeof(pos_t) *program.token_alloc);
        program.token_list = realloc(program.token_list, sizeof(token_t) *program.token_alloc);
    }
    program.token_list[program.token_size - 1] = token_create();
    program.pos_list[program.token_size - 1] = pos;
}

void program_add_labeltype(labeltype_t labeltype) {
    program.labeltype_size++;
    if (program.labeltype_size >= program.labeltype_alloc) {
        program.labeltype_alloc *= 2;
        program.labeltype_list = realloc(program.labeltype_list, sizeof(labeltype_t) *program.labeltype_alloc);
    }
    program.labeltype_list[program.labeltype_size - 1] = labeltype;
}

void program_add_label(label_t label) {
    program.label_size++;
    if (program.label_size >= program.label_alloc) {
        program.label_alloc *= 2;
        program.label_list = realloc(program.label_list, sizeof(label_t ) *program.label_alloc);
    }
    program.label_list[program.label_size - 1] = label;
}

int word_is_int(char *word) {
    int result = 1;
    size_t len = strlen(word);
    for (size_t i = 0; i < len; i++) {
        int eq = 0;
        for (size_t j = 0; j < 10; j++) {
            if (word[i] == "0123456789"[j]) {
                eq = 1;
                break;
            }
        }
        if (eq == 0) {
            result = 0;
            break;
        }
    }
    return result;
}

void program_error(char *msg, pos_t pos) {
    fprintf(stderr, "%s:%lu:%lu ERROR: %s\n", pos.file, pos.line, pos.col, msg);
    program.error = 1;
}

int lex_word_as_token(char *word) {
    size_t idx = program.idx;
    if (idx >= program.token_size) return 0;
    if (strcmp(word, "+") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_PLUS, word);
    } else if (strcmp(word, "-") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_MINUS, word);
    } else if (strcmp(word, "*") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_MUL, word);
    } else if (strcmp(word, "/") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_DIV, word);
    } else if (strcmp(word, "%") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_MOD, word);
    } else if (strcmp(word, ">>") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SHR, word);
    } else if (strcmp(word, "<<") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SHL, word);
    } else if (strcmp(word, "&") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_BAND, word);
    } else if (strcmp(word, "|") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_BOR, word);
    } else if (strcmp(word, "~") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_BNOT, word);
    } else if (strcmp(word, "^") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_XOR, word);
    } else if (strcmp(word, "=") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SET_LABEL, word);
    } else if (strcmp(word, "==") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_EQUALS, word);
    } else if (strcmp(word, "!=") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_NOTEQUALS, word);
    } else if (strcmp(word, ">") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_GREATER, word);
    } else if (strcmp(word, "<") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_MINOR, word);
    } else if (strcmp(word, ">=") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_EQGREATER, word);
    } else if (strcmp(word, "<=") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_EQMINOR, word);
    } else if (strcmp(word, "not") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_NOT, word);
    } else if (strcmp(word, "print") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_PRINT, word);
    } else if (strcmp(word, "dup") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_DUP, word);
    } else if (strcmp(word, "swap") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SWAP, word);
    } else if (strcmp(word, "rot") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_ROT, word);
    } else if (strcmp(word, "drop") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_DROP, word);
    } else if (strcmp(word, "cap") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_CAP, word);
    } else if (strcmp(word, "[") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_START_INDEX, word);
    } else if (strcmp(word, "]") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_END_INDEX, word);
    } else if (strcmp(word, "syscall0") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SYSCALL0, word);
    } else if (strcmp(word, "syscall1") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SYSCALL1, word);
    } else if (strcmp(word, "syscall2") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SYSCALL2, word);
    } else if (strcmp(word, "syscall3") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SYSCALL3, word);
    } else if (strcmp(word, "syscall4") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SYSCALL4, word);
    } else if (strcmp(word, "syscall5") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SYSCALL5, word);
    } else if (strcmp(word, "syscall6") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SYSCALL6, word);
    } else if (strcmp(word, "do") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_DO, word);
    } else if (strcmp(word, "if") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_IF, word);
    } else if (strcmp(word, "else") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_ELSE, word);
    } else if (strcmp(word, "loop") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_LOOP, word);
    } else if (strcmp(word, "label") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_CREATE_LABEL, word);
    } else if (strcmp(word, "end") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_END, word);
    } else if (find_labeltype(word)) {
        token_set(&program.token_list[idx], TKN_TYPE, -1, word);
    } else if (word_is_int(word)) {
        token_set(&program.token_list[idx], TKN_INT, OP_PUSH_INT, word);
    } else {
        token_set(&program.token_list[idx], TKN_ID, -1, word);
    }
//    if (program.token_list[idx].type != -1)
//        print_token(program.token_list[idx]);
    program.idx++;
    return 1;
}

int parse_current_token() {
    size_t idx = program.idx;
    if (idx >= program.token_size) return 0;
    token_t *token_list = program.token_list;
    pos_t *pos_list = program.pos_list;

    switch (token_list[idx].type) {
    case TKN_KEYWORD: {
        switch(token_list[idx].operation) {
        case OP_DO: {
            if (program.condition) {
                size_t jmp = 0;
                size_t if_count = 0;
                for (size_t i = idx + 1; i < program.token_size; i++) {
                    if (token_list[i].type != TKN_KEYWORD) continue;
                    if ((token_list[i].operation == OP_IF && token_list[i - 1].operation != OP_ELSE) || token_list[i].operation == OP_LOOP) if_count++;
                    if (token_list[i].operation == OP_END) {
                        if (if_count > 0) {
                            if_count--;
                        } else {
                            jmp = i;
                            break;
                        }
                    } else if (token_list[i].operation == OP_ELSE && !program.loop) {
                        if (if_count == 0) {
                            jmp = i;
                            break;
                        }
                    }
                }
                if (jmp == 0) {
                    if (program.loop)
                        program_error("loop without a end", pos_list[idx]);
                    else
                        program_error("if without a end", pos_list[idx]);
                    return 0;
                }
                token_list[idx].jmp = jmp;
                program.condition = 0;
                program.loop = 0;
            } else {
                program_error("do without a if or loop", pos_list[idx]);
                return 0;
            }
        } break;
        case OP_IF: {
            program.condition = 1;
            if (token_list[idx].operation == OP_DO) {
                program_error("if condition can't be empty", program.pos_list[idx]);
                return 0;
            }
        } break;
        case OP_ELSE: {
            size_t jmp = 0;
            size_t if_count = 0;
            for (size_t i = idx + 1; i < program.token_size; i++) {
                if (token_list[i].type != TKN_KEYWORD) continue;
                if ((token_list[i].operation == OP_IF && token_list[i - 1].operation != OP_ELSE) && i > idx + 1) if_count++;
                if (token_list[i].operation == OP_END) {
                    if (if_count > 0) {
                        if_count--;
                    } else {
                        jmp = i;
                        break;
                    }
                }
            }
            int found_if = 0;
            for (size_t i = idx - 1; i >= 0; i--) {
                if ((long)i < 0) break;
                if (token_list[i].operation == OP_END) if_count++;
                if (token_list[i].operation == OP_IF && (i == 0 || token_list[i].operation != OP_ELSE)) {
                    if (if_count > 0) {
                        if_count--;
                    } else {
                        found_if = 1;
                        break;
                    }
                }
            }
            if (!found_if) {
                program_error("else without a if", pos_list[idx]);
                return 0;
            }
            if (jmp == 0) {
                program_error("else without a end", pos_list[idx]);
                return 0;
            }
            token_list[idx].jmp = jmp;
        } break;
        case OP_LOOP: {
            program.condition = 1;
            program.loop = 1;
            if (token_list[idx].operation == OP_DO) {
                program_error("loop condition can't be empty", pos_list[idx]);
                return 0;
            }
        } break;
        case OP_CREATE_LABEL: {
            // get label name
            program.idx++;
            idx = program.idx;
            if (token_list[idx].type != TKN_ID) {
                char *msg = malloc(sizeof(char) * ((strlen(token_list[idx].val) * 2) + strlen(token_name[token_list[idx].type]) + 50));
                sprintf(msg, "trying to define label '%s', but '%s' is %s", token_list[idx].val, token_list[idx].val, token_name[token_list[idx].type]);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            char *name = token_list[idx].val;
            // get label type
            program.idx++;
            idx = program.idx;
            if (token_list[idx].type != TKN_TYPE) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val) * 2 + 50));
                sprintf(msg, "trying to define label as type '%s', but '%s' is not a type is a %s", token_list[idx].val, token_list[idx].val, token_name[token_list[idx].type]);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            char *type = token_list[idx].val;
            // verify if label is an array
            size_t end = 0;
            stack_t stack = stack_create();
            for (size_t i = idx + 1; i < program.token_size; i++) {
                int invalid = 0;
                if (token_list[i].type == TKN_KEYWORD) {
                    if (token_list[i].operation == OP_END) {
                        end = i + 1;
                        break;
                    } else {
                        invalid = 1;
                    }
                } else if (token_list[i].type == TKN_INTRINSIC) {
                    switch (token_list[i].operation) {
                    case OP_PLUS: {
                        size_t b = stack_pop(&stack);
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, a + b);
                    } break;
                    case OP_MINUS: {
                        size_t b = stack_pop(&stack);
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, a - b);
                    } break;
                    case OP_MUL: {
                        size_t b = stack_pop(&stack);
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, a * b);
                    } break;
                    case OP_DIV: {
                        size_t b = stack_pop(&stack);
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, a / b);
                    } break;
                    case OP_MOD: {
                        size_t b = stack_pop(&stack);
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, a % b);
                    } break;
                    case OP_SHR: {
                        size_t b = stack_pop(&stack);
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, a >> b);
                    } break;
                    case OP_SHL: {
                        size_t b = stack_pop(&stack);
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, a << b);
                    } break;
                    case OP_BAND: {
                        size_t b = stack_pop(&stack);
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, a & b);
                    } break;
                    case OP_BOR: {
                        size_t b = stack_pop(&stack);
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, a | b);
                    } break;
                    case OP_BNOT: {
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, ~a);
                    } break;
                    case OP_XOR: {
                        size_t b = stack_pop(&stack);
                        size_t a = stack_pop(&stack);
                        stack_push(&stack, a ^ b);
                    } break;
                    default: {
                        invalid = 1;
                    } break;
                    }
                } else if (token_list[i].type == TKN_INT) {
                    stack_push(&stack, atol(token_list[i].val)); 
                } else {
                    invalid = 1;
                }
                if (invalid) {
                    char *msg = malloc(sizeof(char) * (strlen(token_list[i].val) + 40));
                    sprintf(msg, "'%s' is not valid in a array definition", token_list[i].val);
                    program_error(msg, pos_list[idx]);
                    free(msg);
                    return 0;
                }
            }
            if (end  == 0) {
                program_error("creating label without a end", pos_list[idx]);
                return 0;
            }
            label_t label;
            if (stack.size == 0) {
                label = label_create(name, type, 0, 1);
            } else if (stack.size == 1) {
                label = label_create(name, type, 1, stack_pop(&stack));
            } else {
                stack_destroy(stack);
                program_error("array definition can only have one constant value", pos_list[idx]);
                return 0;
            }
            stack_destroy(stack);
            if (label.name == NULL) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val) + 25));
                sprintf(msg, "type '%s' don't exists", token_list[idx].val);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            program_add_label(label);
            program.idx = end - 1;
            if (program.setting) {
                program.cur_label = program.label_size - 1;
            }
        } break;
        case OP_END: {
            size_t end_count = 0;
            int found_open = 0;
            for (size_t i = idx - 1; i >= 0; i--) {
                if (token_list[i].type != TKN_KEYWORD) continue;
                if (idx == 0 || (long)i < 0) break;
                if (token_list[i].operation == OP_END) end_count++;
                if ((token_list[i].operation == OP_IF  && token_list[i - 1].operation != OP_ELSE) || token_list[i].operation == OP_LOOP || token_list[i].operation == OP_CREATE_LABEL) {
                    if (end_count > 0) {
                        end_count--;
                    } else {
                        found_open = 1;
                        program.condition = token_list[i].operation != OP_CREATE_LABEL;
                        if (token_list[i].operation == OP_LOOP) {
                            program.loop = 1;
                            token_list[idx].jmp = i;
                        }
                        break;
                    }
                }
            }
            if (!found_open) {
                program_error("end without a opening", pos_list[idx]);
                return 0;
            }
        } break;
        default: {
            char *msg = malloc(strlen(token_list[idx].val) + 32);
            sprintf(msg, "undefined keyword '%s'", token_list[idx].val);
            program_error(msg, program.pos_list[idx]);
            free(msg);
            return 0;
        } break;
        }
    } break;
    case TKN_INTRINSIC: {
        switch(token_list[idx].operation) {
        case OP_SET_LABEL: {
            int find = 0;
            label_t label;
            if (token_list[idx + 1].type == TKN_ID) {
                for (size_t i = 0; i < program.label_size; i++) {
                    if (strcmp(token_list[idx + 1].val, program.label_list[i].name) == 0) {
                        find = 1;
                        label = program.label_list[i];
                        break;
                    }
                }
            } else if (token_list[idx + 1].operation == OP_CREATE_LABEL) {
                find = 1;
            }
            if (!find) {
                char *msg = malloc(strlen(token_list[idx + 1].val) + 16);
                sprintf(msg, "'%s' is not a label", token_list[idx + 1].val);
                program_error(msg, program.pos_list[idx]);
                free(msg);
                return 0;
            }

            if (token_list[idx + 1].type == TKN_ID && label.arr && token_list[idx + 2].operation != OP_START_INDEX) {
                char *msg = malloc(strlen(token_list[idx + 1].val) + 16);
                sprintf(msg, "'%s' value is not changeable", token_list[idx + 1].val);
                program_error(msg, program.pos_list[idx]);
                free(msg);
                return 0;
            }
            program.setting = 1;
        } break;
        case OP_START_INDEX: {
            int find = 0;
            label_t label;
            if (token_list[idx - 1].type == TKN_ID) {
                for (size_t i = 0; i < program.label_size; i++) {
                    if (strcmp(token_list[idx - 1].val, program.label_list[i].name) == 0) {
                        program.cur_label = i;
                        find = 1;
                        label = program.label_list[i];
                        break;
                    }
                }
            }
            if (!find) {
                char *msg = malloc(strlen(token_list[idx - 1].val) + 16);
                sprintf(msg, "'%s' is not a label", token_list[idx - 1].val);
                program_error(msg, program.pos_list[idx]);
                free(msg);
                return 0;
            }

            if (program.index) {
                program_error("trying to use '[]' operator inside a '[]' operator", program.pos_list[idx]);
                return 0;
            }

            if (!label.arr) {
                program_error("'[]' can only be used in arrays", program.pos_list[idx]);
                return 0;
            }
            program.index = 1;
        } break;
        case OP_END_INDEX: {
            if (!program.index) {
                program_error("']' without a '['", program.pos_list[idx]);
                return 0;
            }
            if (program.idx_amount != 1) {
                char *msg = malloc(strlen(token_list[idx - 1].val) + 72);
                sprintf(msg, "'[]' needs to receive just 1 value, but get '%lu'", program.idx_amount);
                program_error(msg, program.pos_list[idx]);
                free(msg);
                return 0;
            }
            if (program.index) 
                program.idx_amount--;

        } break;
        case OP_CAP: {
            int find = 0;
            label_t label;
            if (token_list[idx - 1].type == TKN_ID) {
                for (size_t i = 0; i < program.label_size; i++) {
                    if (strcmp(token_list[idx - 1].val, program.label_list[i].name) == 0) {
                        program.cur_label = i;
                        find = 1;
                        label = program.label_list[i];
                        break;
                    }
                }
            }
            if (!find) {
                char *msg = malloc(strlen(token_list[idx - 1].val) + 16);
                sprintf(msg, "'%s' is not a label", token_list[idx - 1].val);
                program_error(msg, program.pos_list[idx]);
                free(msg);
                return 0;
            }
            if (!label.arr) {
                program_error("'cap' can only be used in arrays", program.pos_list[idx]);
                return 0;
            }
        } break;
        case OP_PLUS:
        case OP_MINUS:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_DROP:
        case OP_PRINT:
        case OP_SHR:
        case OP_SHL:
        case OP_BAND:
        case OP_BOR:
        case OP_XOR:
            if (program.index) 
                program.idx_amount--;
            break;
        case OP_DUP:
            if (program.index) {
                if (program.idx_amount > 0) program.idx_amount--;
                program.idx_amount+=2;
            }
            break;
        case OP_SWAP:
        case OP_ROT: // TODO: maybe this not work because rotate might need +2 to the program.idx_amount instead of +1
            if (program.index && program.idx_amount == 0) {
                program.idx_amount++;
            }
            break;
        default:
            break;
        }
    } break;
    case TKN_ID: {
        int find = 0;
        
        // find label
        for (size_t i = 0; i < program.label_size; i++) {
            if (strcmp(program.label_list[i].name, token_list[idx].val) == 0) {
                token_list[idx].operation = OP_CALL_LABEL;
                find = 1;
                break;
            }
        }
        if (!find) {
            char *msg = malloc(strlen(token_list[idx].val) + 20);
            sprintf(msg, "undefined word '%s'", token_list[idx].val);
            program_error(msg, program.pos_list[idx]);
            free(msg);
            return 0;
        }
        if (program.index)
            program.idx_amount++;
    } break;
    case TKN_INT: {
        if (program.index)
            program.idx_amount++;
    } break;
    default:
        break;
    }

    return 1;
}

void program_init(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "[ERROR] File not provided\n[INFO] ssol needs one parameter, the .ssol file\n");
        exit(1);
    }
    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        fprintf(stderr, "[ERROR] File '%s' don't exists or can't be openned\n", argv[1]);
        exit(1);
    }

    program.idx = 0;

    program.pos_list = malloc(sizeof(pos_t));
    malloc_check(program.pos_list, "malloc(program.pos_list) in function program_init");
    program.token_list = malloc(sizeof(token_t));
    malloc_check(program.pos_list, "malloc(program.token_list) in function program_init");
    program.token_alloc = 1;
    program.token_size = 0;

    program.labeltype_list = malloc(sizeof(labeltype_t));
    malloc_check(program.labeltype_list, "malloc(program.labeltype_list) in function program_init");
    program.labeltype_alloc = 1;
    program.labeltype_size = 0;

    program.label_list = malloc(sizeof(label_t));
    malloc_check(program.label_list, "malloc(program.label_list) in function program_init");
    program.label_alloc = 1;
    program.label_size = 0;

    program.condition = 0;
    program.setting = 0;
    program.loop = 0;
    program.error = 0;

    program_add_labeltype(labeltype_create("byte", sizeof(char), 1));
    program_add_labeltype(labeltype_create("short", sizeof(short), 1));
    program_add_labeltype(labeltype_create("int", sizeof(int), 1));
    program_add_labeltype(labeltype_create("long", sizeof(long), 1));

    char *word = malloc(sizeof(char));
    malloc_check(word, "malloc(word) in function program_init");
    size_t word_size = 0;
    size_t word_alloc = 1;
    
    size_t line = 1;
    size_t col = 1;
    size_t col_word = 1;

    //char prv_char = 0;
    char cur_char = 0;
    char nxt_char = fgetc(f);

    while (cur_char != EOF) {
        //prv_char = cur_char;
        cur_char = nxt_char;
        nxt_char = fgetc(f);
        col++;
        if (cur_char == ' ' || cur_char == '\t' || cur_char == '\n' || cur_char == EOF || cur_char == '[' || cur_char == ']') {
            if (word_size > 0) {
                word[word_size] = '\0';
                program_add_token(pos_create(argv[1], line, col_word));
                lex_word_as_token(word);
                if (program.error) {
                    exit(1);
                }
                word = malloc(sizeof(char));
                malloc_check(word, "malloc(word) in function program_init");
                word_size = 0;
                word_alloc = 1;
            }
            if (cur_char == '\n') {
               line++;
               col = col_word = 1;
            } else {
               col_word = col;
            }
            if (cur_char == '[' || cur_char == ']') {
                word = realloc(word, sizeof(char) * 2);
                word[0] = cur_char;
                word[1] = '\0';
                program_add_token(pos_create(argv[1], line, col_word));
                lex_word_as_token(word);
                if (program.error) {
                    exit(1);
                }
                word = malloc(sizeof(char));
                malloc_check(word, "malloc(word) in function program_init");
                word_size = 0;
                word_alloc = 1;
            }
            continue;
        }
        word_size++;
        if (word_size >= word_alloc) {
            word_alloc *= 2;
            word = realloc(word, sizeof(char) *word_alloc);
            malloc_check(word, "realloc(word) in function program_init");
        }
        word[word_size - 1] = cur_char;
    }
    free(word);
    fclose(f);

//    for (size_t i = 0; i < program.token_size; i++) {
//        printf("token: %s, val: %s\n", token_name[program.token_list[i].type], program.token_list[i].val);
//    }

    program.idx = 0;
}

void generate_assembly_x86_64_linux() {
    FILE *output = fopen("output.asm", "w");
    if (output == NULL) {
        fprintf(stderr, "[ERROR] Failed to create output.asm\n");
        exit(1);
    }
    fprintf(output, "BITS 64\n");
    fprintf(output, "segment .text\n");
    fprintf(output, "global _start\n");
    fprintf(output, "_print:\n");
    fprintf(output, "    mov rsi,rsp\n");
    fprintf(output, "    sub rsp,32\n");
    fprintf(output, "    mov r9,1\n");
    fprintf(output, "    add rsi,31\n");
    fprintf(output, "    mov byte [rsi],0xa\n");
    fprintf(output, "    mov r10,10\n");
    fprintf(output, "    cmp rax,0\n");
    fprintf(output, "    je _IF0printJMP\n");
    fprintf(output, "_LOOPprintJMP:\n");
    fprintf(output, "    xor rdx,rdx\n");
    fprintf(output, "    div r10\n");
    fprintf(output, "    dec rsi\n");
    fprintf(output, "    inc r9\n");
    fprintf(output, "    add rdx,'0'\n");
    fprintf(output, "    mov [rsi],dl\n");
    fprintf(output, "    cmp rax,0\n");
    fprintf(output, "    jne _LOOPprintJMP ; loop\n");
    fprintf(output, "    jmp _printENDjmp\n");
    fprintf(output, "_IF0printJMP:\n");
    fprintf(output, "    dec rsi\n");
    fprintf(output, "    inc r9\n");
    fprintf(output, "    mov rdx,'0'\n");
    fprintf(output, "    mov [rsi],dl\n");
    fprintf(output, "_printENDjmp:\n");
    fprintf(output, "    mov rax,1\n");
    fprintf(output, "    mov rdi,1\n");
    fprintf(output, "    mov rdx,r9\n");
    fprintf(output, "    syscall\n");
    fprintf(output, "    add rsp,32\n");
    fprintf(output, "    ret\n");

    fprintf(output, "_start:\n");
    while (parse_current_token(program)) {
        size_t idx = program.idx;
        switch (program.token_list[idx].operation) {
        case OP_PUSH_INT: {
            fprintf(output, ";   push int\n");
            fprintf(output, "    push %s\n", program.token_list[idx].val);
        } break;
        case OP_PLUS: {
            fprintf(output, ";   add int\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    add rbx,rax\n");
            fprintf(output, "    push rbx\n");
        } break;
        case OP_MINUS: {
            fprintf(output, ";   sub int\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    sub rbx,rax\n");
            fprintf(output, "    push rbx\n");
        } break;
        case OP_MUL: {
            fprintf(output, ";   mul int\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    mul rbx\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_DIV: {
            fprintf(output, ";   div int\n");
            fprintf(output, "    xor rdx,rdx\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    div rbx\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_MOD: {
            fprintf(output, ";   div int\n");
            fprintf(output, "    xor rdx,rdx\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    div rbx\n");
            fprintf(output, "    push rdx\n");
        } break;
        case OP_SHR: {
            fprintf(output, ";   shift right int\n");
            fprintf(output, "    pop rcx\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    sar rbx,cl\n");
            fprintf(output, "    push rbx\n");
        } break;
        case OP_SHL: {
            fprintf(output, ";   div int\n");
            fprintf(output, "    pop rcx\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    sal rbx,cl\n");
            fprintf(output, "    push rbx\n");
        } break;
        case OP_BAND: {
            fprintf(output, ";   div int\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    and rbx,rax\n");
            fprintf(output, "    push rbx\n");
        } break;
        case OP_BOR: {
            fprintf(output, ";   div int\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    or rbx,rax\n");
            fprintf(output, "    push rbx\n");
        } break;
        case OP_BNOT: {
            fprintf(output, ";   div int\n");
            fprintf(output, "    mov rax,0xffffffffffffffff\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    xor rbx,rax\n");
            fprintf(output, "    push rbx\n");
        } break;
        case OP_XOR: {
            fprintf(output, ";   div int\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    xor rbx,rax\n");
            fprintf(output, "    push rbx\n");
        } break;
        case OP_PRINT: {
            fprintf(output, ";   print int\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    call _print\n");
        } break;
        case OP_DUP: {
            fprintf(output, ";   dup\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    push rax\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_SWAP: {
            fprintf(output, ";   swap\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    push rax\n");
            fprintf(output, "    push rbx\n");
        } break;
         case OP_ROT: {
            fprintf(output, ";   swap\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rcx\n");
            fprintf(output, "    push rax\n");
            fprintf(output, "    push rbx\n");
            fprintf(output, "    push rcx\n");
        } break;
        case OP_DROP: {
            fprintf(output, ";   drop\n");
            fprintf(output, "    pop rax\n");
        } break;
        case OP_CAP: {
            fprintf(output, ";   cap\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    push %lu\n", program.label_list[program.cur_label].cap);
        } break;
        case OP_SYSCALL0: {
            fprintf(output, ";   syscall\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    syscall\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_SYSCALL1: {
            fprintf(output, ";   syscall\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rdi\n");
            fprintf(output, "    syscall\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_SYSCALL2: {
            fprintf(output, ";   syscall\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rdi\n");
            fprintf(output, "    pop rsi\n");
            fprintf(output, "    syscall\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_SYSCALL3: {
            fprintf(output, ";   syscall\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rdi\n");
            fprintf(output, "    pop rsi\n");
            fprintf(output, "    pop rdx\n");
            fprintf(output, "    syscall\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_SYSCALL4: {
            fprintf(output, ";   syscall\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rdi\n");
            fprintf(output, "    pop rsi\n");
            fprintf(output, "    pop rdx\n");
            fprintf(output, "    pop r10\n");
            fprintf(output, "    syscall\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_SYSCALL5: {
            fprintf(output, ";   syscall\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rdi\n");
            fprintf(output, "    pop rsi\n");
            fprintf(output, "    pop rdx\n");
            fprintf(output, "    pop r10\n");
            fprintf(output, "    pop r8\n");
            fprintf(output, "    syscall\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_SYSCALL6: {
            fprintf(output, ";   syscall\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rdi\n");
            fprintf(output, "    pop rsi\n");
            fprintf(output, "    pop rdx\n");
            fprintf(output, "    pop r10\n");
            fprintf(output, "    pop r8\n");
            fprintf(output, "    pop r9\n");
            fprintf(output, "    syscall\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_EQUALS: {
            fprintf(output, ";   equals\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmove rcx,rdx\n");
            fprintf(output, "    push rcx\n");
        } break;
        case OP_GREATER: {
            fprintf(output, ";   greater\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmovg rcx,rdx\n");
            fprintf(output, "    push rcx\n");
        } break;
        case OP_MINOR: {
            fprintf(output, ";   minor\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmovl rcx,rdx\n");
            fprintf(output, "    push rcx\n");
        } break;
        case OP_EQGREATER: {
            fprintf(output, ";   eqgreater\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmovge rcx,rdx\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmovge rcx,rdx\n");
            fprintf(output, "    push rcx\n");
        } break;
        case OP_EQMINOR: {
            fprintf(output, ";   eqminor\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmovle rcx,rdx\n");
            fprintf(output, "    push rcx\n");
        } break;
        case OP_NOTEQUALS: {
            fprintf(output, ";   not\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmovne rcx,rdx\n");
            fprintf(output, "    push rcx\n");
        } break;
        case OP_CALL_LABEL: { 
            labeltype_t l;
            label_t label;
            for (size_t i = 0; i < program.label_size; i++) {
                if (strcmp(program.token_list[idx].val, program.label_list[i].name) == 0) {
                    l = program.labeltype_list[program.label_list[i].type];
                    label = program.label_list[i];
                    break;
                }
            }
            // TODO: for now 'set label' and 'get label' just supports primitive types
            if (program.setting && !label.arr && !program.index) { // set label value
                program.setting=0;
                fprintf(output, ";   set label value\n");
                fprintf(output, "    pop rax\n");
                if (l.primitive) {
                    switch (l.size_bytes) {
                    case sizeof(char):
                        fprintf(output, "    mov byte [%s],al\n", program.token_list[idx].val);
                        break;
                    case sizeof(short):
                        fprintf(output, "    mov word [%s],ax\n", program.token_list[idx].val);
                        break;
                    case sizeof(int):
                        fprintf(output, "    mov dword [%s],eax\n", program.token_list[idx].val);
                        break;
                    case sizeof(long):
                        fprintf(output, "    mov qword [%s],rax\n", program.token_list[idx].val);
                        break;
                    }
                }
            } else { // get label value
                fprintf(output, ";   get label value\n");
                if (l.primitive) {
                    fprintf(output, "    xor rax,rax\n");
                    if (!label.arr) {
                        switch (l.size_bytes) {
                        case sizeof(char):
                            fprintf(output, "    mov al,byte [%s]\n", program.token_list[idx].val);
                            break;
                        case sizeof(short):
                            fprintf(output, "    mov ax,word [%s]\n", program.token_list[idx].val);
                            break;
                        case sizeof(int):
                            fprintf(output, "    mov eax,dword [%s]\n", program.token_list[idx].val);
                            break;
                        case sizeof(long):
                            fprintf(output, "    mov rax,qword [%s]\n", program.token_list[idx].val);
                            break;
                        }
                    } else {
                        fprintf(output, "    mov rax, %s\n", program.token_list[idx].val);
                    }
                    fprintf(output, "    push rax\n");
                }
            }
        } break;
        case OP_END_INDEX: {
            program.index = 0;
            labeltype_t l = program.labeltype_list[program.label_list[program.cur_label].type];
            if (program.setting) {
                program.setting = 0;
                fprintf(output, ";   set array value\n");
                fprintf(output, "    pop rax\n");
                fprintf(output, "    pop rbx\n");
                fprintf(output, "    mov rdx,%lu\n", l.size_bytes);
                fprintf(output, "    mul rdx\n");
                fprintf(output, "    lea rax,[rbx + rax]\n");
                fprintf(output, "    pop rbx\n");
                if (l.primitive) {
                   switch (l.size_bytes) {
                   case sizeof(char):
                       fprintf(output, "    mov byte [rax],bl\n");
                       break;
                   case sizeof(short):
                       fprintf(output, "    mov word [rax],bx\n");
                       break;
                   case sizeof(int):
                       fprintf(output, "    mov dword [rax],ebx\n");
                       break;
                   case sizeof(long):
                       fprintf(output, "    mov qword [rax],rbx\n");
                       break;
                   }
                }
           } else {
                fprintf(output, ";   get array value\n");
                fprintf(output, "    pop rax\n");
                fprintf(output, "    pop rbx\n");
                fprintf(output, "    mov rdx,%lu\n", l.size_bytes);
                fprintf(output, "    mul rdx\n");
                fprintf(output, "    lea rax,[rbx + rax]\n");
                if (l.primitive) {
                   fprintf(output, "    xor rbx,rbx\n");
                   switch (l.size_bytes) {
                   case sizeof(char):
                       fprintf(output, "    mov bl, byte [rax]\n");
                       break;
                   case sizeof(short):
                       fprintf(output, "    mov bx, word [rax]\n");
                       break;
                   case sizeof(int):
                       fprintf(output, "    mov ebx, dword [rax]\n");
                       break;
                   case sizeof(long):
                       fprintf(output, "    mov rbx, qword [rax]\n");
                       break;
                   }
                }
                fprintf(output, "    push rbx\n");
            }
        } break;
        case OP_DO: {
            fprintf(output, ";   do\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    test rax,rax\n");
            fprintf(output, "    jz ADR%lu\n", program.token_list[idx].jmp);
        } break;
        case OP_ELSE: {
            fprintf(output, ";   else\n");
            fprintf(output, "    jmp ADR%lu\n", program.token_list[idx].jmp);
            fprintf(output, "ADR%lu:\n", program.idx);
        } break;
        case OP_LOOP: {
            fprintf(output, ";   loop\n");
            fprintf(output, "ADR%lu:\n", program.idx);
        } break;
        case OP_END: {
            if (program.condition) {
                program.condition = 0;
                fprintf(output, ";   end\n");
                if (program.loop) {
                    program.loop = 0;
                    fprintf(output, "    jmp ADR%lu\n", program.token_list[idx].jmp);
                }
                fprintf(output, "ADR%lu:\n", program.idx);
            } else if (program.setting) {
                label_t label = program.label_list[program.cur_label];
                labeltype_t l = program.labeltype_list[label.type];
                program.setting=0;
                if (!label.arr) {
                    fprintf(output, ";   set label value\n");
                    fprintf(output, "    pop rax\n");
                    if (l.primitive) {
                        switch (l.size_bytes) {
                        case sizeof(char):
                            fprintf(output, "    mov byte [%s],al\n", label.name);
                            break;
                        case sizeof(short):
                            fprintf(output, "    mov word [%s],ax\n", label.name);
                            break;
                        case sizeof(int):
                            fprintf(output, "    mov dword [%s],eax\n", label.name);
                            break;
                        case sizeof(long):
                            fprintf(output, "    mov qword [%s],rax\n", label.name);
                            break;
                        }
                    }
                } else {
                    fprintf(output, ";   set array value\n");
                    fprintf(output, "    mov rcx,%lu\n", label.cap - 1);
                    fprintf(output, "ADR%lu:\n", program.idx);
                    fprintf(output, "    mov rax,rcx\n");
                    fprintf(output, "    mov rdx,%lu\n", l.size_bytes);
                    fprintf(output, "    mul rdx\n");
                    fprintf(output, "    lea rax,[%s + rax]\n", label.name);
                    fprintf(output, "    pop rbx\n");
                    if (l.primitive) {
                       switch (l.size_bytes) {
                       case sizeof(char):
                           fprintf(output, "    mov byte [rax],bl\n");
                           break;
                       case sizeof(short):
                           fprintf(output, "    mov word [rax],bx\n");
                           break;
                       case sizeof(int):
                           fprintf(output, "    mov dword [rax],ebx\n");
                           break;
                       case sizeof(long):
                           fprintf(output, "    mov qword [rax],rbx\n");
                           break;
                       }
                    }
                    fprintf(output, "    dec rcx\n");
                    fprintf(output, "    cmp rcx,0\n");
                    fprintf(output, "    jge ADR%lu\n", program.idx);
                }
            }
        } break;
        default:
            break;
        }
        program.idx++;
    }
    if (program.error) {
        exit(1);
    }
    fprintf(output, ";  exit program_code\n");
    fprintf(output, "   mov rax,60\n");
    fprintf(output, "   mov rdi,0\n");
    fprintf(output, "   syscall\n");
    fprintf(output, "segment .bss\n");
    for (size_t i = 0; i < program.label_size; i++) {
        labeltype_t l = program.labeltype_list[program.label_list[i].type];
        size_t alloc = program.label_list[i].cap;
        if (l.primitive) {
            switch (l.size_bytes) {
            case sizeof(char):
                fprintf(output, "%s: resb %lu\n", program.label_list[i].name, alloc);
                break;
            case sizeof(short):
                fprintf(output, "%s: resw %lu\n", program.label_list[i].name, alloc);
                break;
            case sizeof(int):
                fprintf(output, "%s: resd %lu\n", program.label_list[i].name, alloc);
                break;
            case sizeof(long):
                fprintf(output, "%s: resq %lu\n", program.label_list[i].name, alloc);
                break;
            default:
                break;
            }
        }
    }
    fclose(output);
    system("nasm -felf64 output.asm -o output.o");
    system("ld -o output output.o");
}

void program_quit() {
    for (size_t i = 0; i < program.token_size; i++) {
        free(program.token_list[i].val);
    }
    for (size_t i = 0; i < program.labeltype_size; i++) {
        free(program.labeltype_list[i].name);
    }
    for (size_t i = 0; i < program.label_size; i++) {
        free(program.label_list[i].name);
    }
    free(program.pos_list);
    free(program.token_list);
    free(program.labeltype_list);
    free(program.label_list);
}

int main(int argc, char **argv) {
    program_init(argc, argv);
    generate_assembly_x86_64_linux();
    program_quit();
    return 0;
}

