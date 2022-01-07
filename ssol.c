#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define RET_STACK_CAP 65536 // 64kb

// TODO: some places might be broken because of the local variables

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
        TKN_STR,
        TKN_TYPE,
        TKN_COUNT
    } type;
    enum {
        OP_PUSH_INT,
        OP_PUSH_STR,
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
        OP_MEMORY,
        OP_DELETE,
        OP_CREATE_CONST,
        OP_CREATE_VAR,
        OP_SET_VAR,
        OP_CALL_VAR,
        OP_GET_ADR,
        OP_STORE,
        OP_FETCH,
        OP_SIZEOF,
        OP_CREATE_PROC,
        OP_CALL_PROC,
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
    // TODO: add member vars to types
} vartype_t;

typedef struct {
    char *name;
    size_t type;
    int arr;
    size_t cap;
    size_t adr;
    int constant;
    // TODO: make the const_val be just a size_t and make constants be from 'long' type automatically
    union {
        unsigned char b8;
        unsigned short b16;
        unsigned int b32;
        unsigned long b64;
    } const_val;
} var_t;

typedef struct {
    char *str;
    size_t len;
    size_t adr;
} str_t;

typedef struct {
    char *name;
    size_t adr;
    size_t end;
    var_t *var_list;
    size_t var_alloc;
    size_t var_size;
    size_t local_var_capacity;
} proc_t;

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

    vartype_t *vartype_list;
    size_t vartype_size;
    size_t vartype_alloc;

    var_t *var_list;
    size_t var_size;
    size_t var_alloc;

    str_t *str_list;
    size_t str_size;
    size_t str_alloc;

    proc_t *proc_list;
    size_t proc_size;
    size_t proc_alloc;

    size_t idx;
    int error;
    int condition;
    int loop;
    int setting;
    int address;
    int index;
    int size_of;
    int proc_def;
    int local_def;
    int global_def;
    int has_malloc;
    int has_main;
    size_t idx_amount;
    size_t cur_var;
    size_t cur_vartype;
    stack_t cur_proc; // TODO: maybe this could be a size_t instead of a stack_t
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

str_t str_create(char *str_data, size_t adr) {
    str_t str;
    str.str = str_data;
    str.len = strlen(str_data);
    str.adr = adr + 1;
    return str;
}

proc_t proc_create(char *name) {
    proc_t proc;
    proc.name = name;
    proc.adr = program.proc_size;
    proc.var_list = malloc(sizeof(var_t));
    proc.var_alloc = 1;
    proc.var_size = 0;
    proc.local_var_capacity = 0;
    return proc;
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

vartype_t vartype_create(char *name, size_t size_bytes, int primitive) {
    vartype_t vartype;
    vartype.name = malloc(sizeof(*vartype.name) * (strlen(name) + 1));
    malloc_check(vartype.name, "malloc(vartype.name) in function vartype_create");
    strcpy(vartype.name, name);
    vartype.size_bytes = size_bytes;
    vartype.primitive = primitive;
    return vartype;
}

size_t find_vartype(char *name) {
    size_t find = 0;
    for (size_t i = 0; i < program.vartype_size; i++) {
        if (strcmp(program.vartype_list[i].name, name) == 0) {
            find = i + 1;
            break;
        }
    }
    return find;
}

var_t var_create(char *name, char *type_name, int arr, size_t cap) {
    size_t type;
    type = find_vartype(type_name);
    if (!type) return (var_t){.name=NULL};
    type--;
    var_t var;
    var.name = malloc(sizeof(*var.name) * (strlen(name) + 1));
    malloc_check(var.name, "malloc(var.name) in function var_create");
    strcpy(var.name, name);
    var.type = type;
    var.arr = arr;
    var.cap = cap;
    var.constant = 0;
    var.adr = program.var_size;
    return var;
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

void program_add_vartype(vartype_t vartype) {
    program.vartype_size++;
    if (program.vartype_size >= program.vartype_alloc) {
        program.vartype_alloc *= 2;
        program.vartype_list = realloc(program.vartype_list, sizeof(vartype_t) *program.vartype_alloc);
    }
    program.vartype_list[program.vartype_size - 1] = vartype;
}

void program_add_var(var_t var) {
    program.var_size++;
    if (program.var_size >= program.var_alloc) {
        program.var_alloc *= 2;
        program.var_list = realloc(program.var_list, sizeof(var_t ) *program.var_alloc);
    }
    program.var_list[program.var_size - 1] = var;
}

void program_add_local_var(var_t var) {
    proc_t *proc = &(program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]]);
    proc->var_size++;
    if (proc->var_size >= proc->var_alloc) {
        proc->var_alloc *= 2;
        proc->var_list = realloc(proc->var_list, sizeof(var_t ) *proc->var_alloc);
    }
    proc->var_list[proc->var_size - 1] = var;
}

void program_add_str(str_t str) {
    program.str_size++;
    if (program.str_size >= program.str_alloc) {
        program.str_alloc *= 2;
        program.str_list = realloc(program.str_list, sizeof(str_t ) *program.str_alloc);
    }
    program.str_list[program.str_size - 1] = str;
}

void program_add_proc(proc_t proc) {
    program.proc_size++;
    if (program.proc_size >= program.proc_alloc) {
        program.proc_alloc *= 2;
        program.proc_list = realloc(program.proc_list, sizeof(proc_t ) *program.proc_alloc);
    }
    program.proc_list[program.proc_size - 1] = proc;
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

int lex_word_as_token(char *word, int is_str, size_t adr) {
    size_t idx = program.idx;
    if (idx >= program.token_size) return 0;

    if (is_str) {
        token_set(&program.token_list[idx], TKN_STR, OP_PUSH_STR, word);
        program.token_list[idx].jmp = adr;
    } else if (strcmp(word, "+") == 0) {
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
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SET_VAR, word);
    } else if (strcmp(word, "$") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_GET_ADR, word);
    } else if (strcmp(word, "!") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_STORE, word);
    } else if (strcmp(word, "@") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_FETCH, word);
    } else if (strcmp(word, "sizeof") == 0) {
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_SIZEOF, word);
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
    } else if (strcmp(word, "memory") == 0) {
        if (program.has_malloc == 0) program.has_malloc = 1;
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_MEMORY, word);
    } else if (strcmp(word, "delete") == 0) {
        if (program.has_malloc == 0) program.has_malloc = 1;
        token_set(&program.token_list[idx], TKN_INTRINSIC, OP_DELETE, word);
    } else if (strcmp(word, "do") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_DO, word);
    } else if (strcmp(word, "if") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_IF, word);
    } else if (strcmp(word, "else") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_ELSE, word);
    } else if (strcmp(word, "loop") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_LOOP, word);
    } else if (strcmp(word, "var") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_CREATE_VAR, word);
    } else if (strcmp(word, "const") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_CREATE_CONST, word);
    } else if (strcmp(word, "proc") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_CREATE_PROC, word);
    } else if (strcmp(word, "end") == 0) {
        token_set(&program.token_list[idx], TKN_KEYWORD, OP_END, word);
    } else if (find_vartype(word)) {
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
            if (program.cur_proc.size == 0) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 40)));
                sprintf(msg, "'%s' can only be used in a procedure", token_list[idx].val);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            if (program.condition) {
                size_t jmp = 0;
                size_t if_count = 0;
                for (size_t i = idx + 1; i < program.token_size; i++) {
                    if (token_list[i].type != TKN_KEYWORD) continue;
                    if ((token_list[i].operation == OP_IF && token_list[i - 1].operation != OP_ELSE) || token_list[i].operation == OP_LOOP || token_list[i].operation == OP_CREATE_VAR ||  token_list[i].operation == OP_CREATE_CONST || token_list[i].operation == OP_CREATE_PROC) if_count++;
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
                program.loop = 0;
                program.condition = 0;
            } else {
                program_error("do without a if or loop", pos_list[idx]);
                return 0;
            }
        } break;
        case OP_IF: {
            if (program.cur_proc.size == 0) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 40)));
                sprintf(msg, "'%s' can only be used in a procedure", token_list[idx].val);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }

            program.condition = 1;
            if (token_list[idx].operation == OP_DO) {
                program_error("if condition can't be empty", program.pos_list[idx]);
                return 0;
            }
        } break;
        case OP_ELSE: {
            if (program.cur_proc.size == 0) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 40)));
                sprintf(msg, "'%s' can only be used in a procedure", token_list[idx].val);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }

            size_t jmp = 0;
            size_t if_count = 0;
            for (size_t i = idx + 1; i < program.token_size; i++) {
                if (token_list[i].type != TKN_KEYWORD) continue;
                if (((token_list[i].operation == OP_IF && token_list[i - 1].operation != OP_ELSE) && i > idx + 1) || token_list[i].operation == OP_LOOP || token_list[i].operation == OP_CREATE_VAR||  token_list[i].operation == OP_CREATE_CONST || token_list[i].operation == OP_CREATE_PROC) if_count++;
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
                if (token_list[i].operation == OP_LOOP || token_list[i].operation == OP_CREATE_VAR||  token_list[i].operation == OP_CREATE_CONST || token_list[i].operation == OP_CREATE_PROC) if_count--;
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
            if (program.cur_proc.size == 0) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 40)));
                sprintf(msg, "'%s' can only be used in a procedure", token_list[idx].val);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }

            program.condition = 1;
            program.loop = 1;
            if (token_list[idx].operation == OP_DO) {
                program_error("loop condition can't be empty", pos_list[idx]);
                return 0;
            }
        } break;
        case OP_CREATE_VAR: {
            // get var name
            program.idx++;
            idx = program.idx;
            if (program.token_size == program.idx) {
                program_error("variable definition is invalid", pos_list[idx-1]);
                return 0;
            }
            if (token_list[idx].type != TKN_ID) {
                char *msg = malloc(sizeof(char) * ((strlen(token_list[idx].val) * 2) + strlen(token_name[token_list[idx].type]) + 50));
                sprintf(msg, "trying to define var '%s', but '%s' is %s", token_list[idx].val, token_list[idx].val, token_name[token_list[idx].type]);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            if (program.cur_proc.size == 0) {
                for (size_t i = 0; i < program.var_size; i++) {
                    if (strcmp(token_list[idx].val, program.var_list[i].name) == 0) {
                        char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 30)));
                        sprintf(msg, "trying to redefine var '%s'", token_list[idx].val);
                        program_error(msg, pos_list[idx]);
                        free(msg);
                        return 0;
                    }
                }
            } else {
                for (size_t i = 0; i < program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_size; i++) {
                    if (strcmp(token_list[idx].val, program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i].name) == 0) {
                        char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 30)));
                        sprintf(msg, "trying to redefine var '%s'", token_list[idx].val);
                        program_error(msg, pos_list[idx]);
                        free(msg);
                        return 0;
                    }
                }
            }
            for (size_t i = 0; i < program.proc_size; i++) {
                if (strcmp(token_list[idx].val, program.proc_list[i].name) == 0) {
                    char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 30)));
                    sprintf(msg, "trying to redefine proc '%s' as var", token_list[idx].val);
                    program_error(msg, pos_list[idx]);
                    free(msg);
                    return 0;
                }
            }
            char *name = token_list[idx].val;
            // get var type
            program.idx++;
            idx = program.idx;
            if (program.token_size == program.idx) {
                program_error("variable definition is invalid", pos_list[idx-1]);
                return 0;
            }
            if (token_list[idx].type != TKN_TYPE) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val) * 2 + 50));
                sprintf(msg, "trying to define var as type '%s', but '%s' is not a type is a %s", token_list[idx].val, token_list[idx].val, token_name[token_list[idx].type]);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            char *type = token_list[idx].val;
            // verify if var is an array
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
                    case OP_SIZEOF: {
                        vartype_t vt;
                        int find_vt = 0;
                        for (size_t j = 0; j < program.vartype_size; j++) {
                            if (strcmp(token_list[i + 1].val, program.vartype_list[j].name) == 0) {
                                find_vt = 1;
                                vt = program.vartype_list[j];
                                break;
                            }
                        }
                        if (find_vt) {
                            stack_push(&stack, vt.size_bytes);
                        } else {
                            invalid = 1;
                        }
                    } break;
                    default: {
                        invalid = 1;
                    } break;
                    }
                } else if (token_list[i].type == TKN_INT) {
                    stack_push(&stack, atol(token_list[i].val)); 
                } else if (token_list[i].type == TKN_ID) {
                    var_t var;
                    int find_v = 0;
                    for (size_t j = 0; j < program.var_size; j++) {
                        if (strcmp(token_list[i].val, program.var_list[j].name) == 0) {
                            find_v = 1;
                            var = program.var_list[j];
                            break;
                        }
                    }
                    if (find_v && var.constant && program.vartype_list[var.type].primitive) {
                        switch (program.vartype_list[var.type].size_bytes) {
                        case sizeof(char):
                            stack_push(&stack, var.const_val.b8);
                            break;
                        case sizeof(short):
                            stack_push(&stack, var.const_val.b16);
                            break;
                        case sizeof(int):
                            stack_push(&stack, var.const_val.b32);
                            break;
                        case sizeof(long):
                            stack_push(&stack, var.const_val.b64);
                            break;
                        }
                    } else {
                        invalid = 1;
                    }
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
                program_error("creating var without a end", pos_list[idx]);
                return 0;
            }
            var_t var;
            if (stack.size == 0) {
                var = var_create(name, type, 0, 1);
            } else if (stack.size == 1) {
                var = var_create(name, type, 1, stack_pop(&stack));
            } else {
                stack_destroy(stack);
                program_error("array definition can only have one constant value", pos_list[idx]);
                return 0;
            }
            stack_destroy(stack);
            if (var.name == NULL) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val) + 25));
                sprintf(msg, "type '%s' don't exists", token_list[idx].val);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            if (program.cur_proc.size == 0) {
                program_add_var(var);
                if (program.setting) {
                    program.cur_var = program.var_size - 1;
                }
                program.global_def = 1;
            } else {
                program_add_local_var(var);
                proc_t *proc = &(program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]]); 
                var_t *v = &(proc->var_list[proc->var_size - 1]);
                size_t add_offset = program.vartype_list[v->type].size_bytes;
                if (v->arr) {
                    add_offset *= v->cap;
                }
                v->adr = 0;
                for (size_t i = 0; i < proc->var_size; i++) {
                    proc->var_list[i].adr += add_offset;
                }
                proc->local_var_capacity += add_offset;
                program.cur_var = proc->var_size - 1;
                program.local_def = 1;
            }
            program.idx = end - 1;
        } break;
        case OP_CREATE_CONST: {
            // get var name
            if (program.cur_proc.size != 0) {
                program_error("can't define a constant inside a procedure", pos_list[idx-1]);
                return 0;
            }
            program.idx++;
            idx = program.idx;
            if (program.token_size == program.idx) {
                program_error("constant definition is invalid", pos_list[idx-1]);
                return 0;
            }
            if (token_list[idx].type != TKN_ID) {
                char *msg = malloc(sizeof(char) * ((strlen(token_list[idx].val) * 2) + strlen(token_name[token_list[idx].type]) + 50));
                sprintf(msg, "trying to define const '%s', but '%s' is %s", token_list[idx].val, token_list[idx].val, token_name[token_list[idx].type]);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            for (size_t i = 0; i < program.var_size; i++) {
                if (strcmp(token_list[idx].val, program.var_list[i].name) == 0) {
                    char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 30)));
                    sprintf(msg, "trying to redefine const '%s'", token_list[idx].val);
                    program_error(msg, pos_list[idx]);
                    free(msg);
                    return 0;
                }
            }
            for (size_t i = 0; i < program.proc_size; i++) {
                if (strcmp(token_list[idx].val, program.proc_list[i].name) == 0) {
                    char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 30)));
                    sprintf(msg, "trying to redefine proc '%s' as const", token_list[idx].val);
                    program_error(msg, pos_list[idx]);
                    free(msg);
                    return 0;
                }
            }
            char *name = token_list[idx].val;
            // get var type
            program.idx++;
            idx = program.idx;
            if (program.token_size == program.idx) {
                program_error("constant definition is invalid", pos_list[idx-1]);
                return 0;
            }
            if (token_list[idx].type != TKN_TYPE) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val) * 2 + 50));
                sprintf(msg, "trying to define const as type '%s', but '%s' is not a type is a %s", token_list[idx].val, token_list[idx].val, token_name[token_list[idx].type]);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            char *type = token_list[idx].val;
            // TODO: for now constants can't be arrays and just support primitive types
            // get const value
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
                    case OP_SIZEOF: {
                        vartype_t vt;
                        int find_vt = 0;
                        for (size_t j = 0; j < program.vartype_size; j++) {
                            if (strcmp(token_list[i + 1].val, program.vartype_list[j].name) == 0) {
                                find_vt = 1;
                                vt = program.vartype_list[j];
                                break;
                            }
                        }
                        if (find_vt) {
                            stack_push(&stack, vt.size_bytes);
                            i++;
                        } else {
                            invalid = 1;
                        }
                    } break;
                    default: {
                        invalid = 1;
                    } break;
                    }
                } else if (token_list[i].type == TKN_INT) {
                    stack_push(&stack, atol(token_list[i].val)); 
                } else if (token_list[i].type == TKN_ID) {
                    var_t var;
                    int find_v = 0;
                    for (size_t j = 0; j < program.var_size; j++) {
                        if (strcmp(token_list[i].val, program.var_list[j].name) == 0) {
                            find_v = 1;
                            var = program.var_list[j];
                            break;
                        }
                    }
                    if (find_v && var.constant && program.vartype_list[var.type].primitive) {
                        switch (program.vartype_list[var.type].size_bytes) {
                        case sizeof(char):
                            stack_push(&stack, var.const_val.b8);
                            break;
                        case sizeof(short):
                            stack_push(&stack, var.const_val.b16);
                            break;
                        case sizeof(int):
                            stack_push(&stack, var.const_val.b32);
                            break;
                        case sizeof(long):
                            stack_push(&stack, var.const_val.b64);
                            break;
                        }
                    } else {
                        invalid = 1;
                    }
                } else {
                    invalid = 1;
                }
                if (invalid) {
                    char *msg = malloc(sizeof(char) * (strlen(token_list[i].val) + 40));
                    sprintf(msg, "'%s' is not valid in a const definition", token_list[i].val);
                    program_error(msg, pos_list[idx]);
                    free(msg);
                    return 0;
                }
            }
            if (end  == 0) {
                program_error("creating const without a end", pos_list[idx]);
                return 0;
            }
            var_t var;
            if (stack.size == 1) {
                var = var_create(name, type, 0, 1);
                vartype_t vt = program.vartype_list[var.type];
                var.constant = 1;
                if (vt.primitive) {
                    switch (vt.size_bytes) {
                    case sizeof(char):
                        var.const_val.b8 = stack_pop(&stack);
                        break;
                    case sizeof(short):
                        var.const_val.b16 = stack_pop(&stack);
                        break;
                    case sizeof(int):
                        var.const_val.b32 = stack_pop(&stack);
                        break;
                    case sizeof(long):
                        var.const_val.b64 = stack_pop(&stack);
                        break;
                    }
                }
            } else {
                char *msg = malloc(sizeof(char) * 92);
                sprintf(msg, "const definition can only have 1 constant value, but got %lu", stack.size);
                program_error(msg, pos_list[idx]);
                free(msg);
                stack_destroy(stack);
                return 0;
            }
            stack_destroy(stack);
            if (var.name == NULL) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val) + 25));
                sprintf(msg, "type '%s' don't exists", token_list[idx].val);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            program_add_var(var);
            program.idx = end - 1;
            program.cur_var = program.var_size - 1;
        } break;
        case OP_CREATE_PROC: {
            
            if (program.cur_proc.size != 0) {
                program_error("trying to define a procedure inside a procedure", pos_list[idx-1]);
                return 0;
            }
            // get proc name
            idx = program.idx + 1;
            if (program.token_size == program.idx) {
                program_error("procedure definition is invalid", pos_list[idx-1]);
                return 0;
            }
            if (token_list[idx].type != TKN_ID) {
                char *msg = malloc(sizeof(char) * ((strlen(token_list[idx].val) * 2) + strlen(token_name[token_list[idx].type]) + 50));
                sprintf(msg, "trying to define proc '%s', but '%s' is %s", token_list[idx].val, token_list[idx].val, token_name[token_list[idx].type]);
                program_error(msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            for (size_t i = 0; i < program.var_size; i++) {
                if (strcmp(token_list[idx].val, program.var_list[i].name) == 0) {
                    char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 30)));
                    sprintf(msg, "trying to redefine var '%s' as proc", token_list[idx].val);
                    program_error(msg, pos_list[idx]);
                    free(msg);
                    return 0;
                }
            }
            for (size_t i = 0; i < program.proc_size; i++) {
                if (strcmp(token_list[idx].val, program.proc_list[i].name) == 0) {
                    char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 30)));
                    sprintf(msg, "trying to redefine proc '%s'", token_list[idx].val);
                    program_error(msg, pos_list[idx]);
                    free(msg);
                    return 0;
                }
            }
            char *name = token_list[idx].val;
            if (strcmp(name, "main") == 0) {
                program.has_main = 1;
            }
            proc_t proc = proc_create(name);
            program_add_proc(proc);
            stack_push(&program.cur_proc, program.proc_size - 1);
            program.proc_def = 1;
            size_t end = 0;
            size_t end_count = 0;
            for (size_t i = idx + 1; i < program.token_size; i++) {
                if (token_list[i].type != TKN_KEYWORD) continue;
                if ((token_list[i].operation == OP_IF && token_list[i - 1].operation != OP_ELSE) || token_list[i].operation == OP_LOOP || token_list[i].operation == OP_CREATE_VAR ||  token_list[i].operation == OP_CREATE_CONST || token_list[i].operation == OP_CREATE_PROC) end_count++;
                if (token_list[i].operation == OP_END) {
                    if (end_count > 0) {
                        end_count--;
                    } else {
                        end = i;
                        break;
                    }
                }
            }
            if (end == 0) {
                program_error("proc without a end", pos_list[idx]);
                return 0;
            }
            //program.proc_list[program.proc_size - 1].end = end;
        } break;
        case OP_END: {
            size_t end_count = 0;
            int found_open = 0;
            for (size_t i = idx - 1; i >= 0; i--) {
                if (token_list[i].type != TKN_KEYWORD) continue;
                if (idx == 0 || (long)i < 0) break;
                if (token_list[i].operation == OP_END) end_count++;
                if ((token_list[i].operation == OP_IF  && token_list[i - 1].operation != OP_ELSE) || token_list[i].operation == OP_LOOP || token_list[i].operation == OP_CREATE_VAR || token_list[i].operation == OP_CREATE_PROC) {
                    if (end_count > 0) {
                        end_count--;
                    } else {
                        found_open = 1;
                        program.condition = token_list[i].operation != OP_CREATE_VAR && token_list[i].operation != OP_CREATE_PROC;
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
        if (program.cur_proc.size == 0 && (token_list[idx].operation != OP_PLUS && token_list[idx].operation != OP_MINUS && token_list[idx].operation != OP_MUL && token_list[idx].operation != OP_DIV && token_list[idx].operation != OP_MOD && token_list[idx].operation != OP_SHR && token_list[idx].operation != OP_SHL && token_list[idx].operation != OP_BAND && token_list[idx].operation != OP_BOR && token_list[idx].operation != OP_BNOT && token_list[idx].operation != OP_XOR)) {
            char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 40)));
            sprintf(msg, "'%s' can only be used in a procedure", token_list[idx].val);
            program_error(msg, pos_list[idx]);
            free(msg);
            return 0;
        }
        switch(token_list[idx].operation) {
        case OP_SIZEOF: {
            int find = 0;
             if (token_list[idx + 1].type == TKN_ID) {
                for (size_t i = 0; i < program.var_size; i++) {
                    if (strcmp(token_list[idx + 1].val, program.var_list[i].name) == 0) {
                        find = 1;
                        break;
                    }
                }
                for (size_t i = 0; i < program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_size; i++) {
                    if (strcmp(token_list[idx + 1].val, program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i].name) == 0) {
                        find = 1;
                        break;
                    }
                }
            }
            if (find) {
                program.size_of = 1;
                program.token_list[idx].operation = -1;
                break;
            }
        }
        case OP_FETCH:
        case OP_STORE: {
            int find = 0;
            if (token_list[idx + 1].type == TKN_TYPE) {
                for (size_t i = 0; i < program.vartype_size; i++) {
                    if (strcmp(token_list[idx + 1].val, program.vartype_list[i].name) == 0) {
                        find = 1;
                        program.cur_vartype = i;
                        break;
                    }
                }
            }
            if (!find) {
                char *msg = malloc(strlen(token_list[idx + 1].val) + 16);
                sprintf(msg, "'%s' is not a type", token_list[idx + 1].val);
                program_error(msg, program.pos_list[idx]);
                free(msg);
                return 0;
            }
        } break;
        case OP_GET_ADR: {
            int find = 0;
            var_t var = {0};
            if (token_list[idx + 1].type == TKN_ID) {
                if (program.cur_proc.size == 0) {
                    for (size_t i = 0; i < program.var_size; i++) {
                        if (strcmp(token_list[idx + 1].val, program.var_list[i].name) == 0) {
                            find = 1;
                            var = program.var_list[i];
                            break;
                        }
                    }
                } else {
                    for (size_t i = 0; i < program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_size; i++) {
                        if (strcmp(token_list[idx + 1].val, program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i].name) == 0) {
                            find = 1;
                            var = program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i];
                            break;
                        }
                    }
                }
            } else if (token_list[idx + 1].operation == OP_CREATE_VAR) {
                find = 1;
            }
            if (!find) {
                char *msg = malloc(strlen(token_list[idx + 1].val) + 16);
                sprintf(msg, "'%s' is not a var", token_list[idx + 1].val);
                program_error(msg, program.pos_list[idx]);
                free(msg);
                return 0;
            }

            if (var.constant) {
                program_error("you can't get the address of an constant", program.pos_list[idx]);
                return 0;
            }

            program.address = 1;
        } break;
        case OP_SET_VAR: {
            int find = 0;
            var_t var = {0};

            if (token_list[idx + 1].type == TKN_ID) {
                for (size_t i = 0; i < program.var_size; i++) {
                    if (strcmp(token_list[idx + 1].val, program.var_list[i].name) == 0) {
                        find = 1;
                        var = program.var_list[i];
                        break;
                        }
                }
                for (size_t i = 0; i < program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_size; i++) {
                    if (strcmp(token_list[idx + 1].val, program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i].name) == 0) {
                        find = 1;
                        var = program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i];
                        break;
                    }
                }
            } else if (token_list[idx + 1].operation == OP_CREATE_VAR) {
                find = 1;
            }
            if (!find) {
                char *msg = malloc(strlen(token_list[idx + 1].val) + 16);
                sprintf(msg, "'%s' is not a var", token_list[idx + 1].val);
                program_error(msg, program.pos_list[idx]);
                free(msg);
                return 0;
            }

            if (var.constant) {
                program_error("trying to modify a constant variable", program.pos_list[idx]);
                return 0;
            }

            if (token_list[idx + 1].type == TKN_ID && var.arr && token_list[idx + 2].operation != OP_START_INDEX) {
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
            if (!program.size_of) {
                var_t var;
                if (token_list[idx - 1].type == TKN_ID) {
                    for (size_t i = 0; i < program.var_size; i++) {
                        if (strcmp(token_list[idx - 1].val, program.var_list[i].name) == 0) {
                            program.cur_var = i;
                            find = 1;
                            var = program.var_list[i];
                            break;
                            }
                    }
                    for (size_t i = 0; i < program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_size; i++) {
                        if (strcmp(token_list[idx - 1].val, program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i].name) == 0) {
                            program.cur_var = i;
                            find = 1;
                            var = program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i];
                            break;
                        }
                    }
                }
                if (!find) {
                    char *msg = malloc(strlen(token_list[idx - 1].val) + 16);
                    sprintf(msg, "'%s' is not a var", token_list[idx - 1].val);
                    program_error(msg, program.pos_list[idx]);
                    free(msg);
                    return 0;
                }

                if (program.index) {
                    program_error("trying to use '[]' operator inside a '[]' operator", program.pos_list[idx]);
                    return 0;
                }

                if (!var.arr) {
                    program_error("'[]' can only be used in arrays", program.pos_list[idx]);
                    return 0;
                }
                program.index = 1;
            } else {
                for (size_t i = idx + 1; i < program.token_size; i++) {
                    if (program.token_list[i].operation == OP_END_INDEX) {
                        find = 1;
                        program.idx = i + 1;
                        if  (program.idx >= program.token_size) return 0;
                    }
                }
                if (!find) {
                    program_error("'[' without ']'", program.pos_list[idx]);
                    return 0;
                }
            }
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
            var_t var;
            if (token_list[idx - 1].type == TKN_ID) {
                for (size_t i = 0; i < program.var_size; i++) {
                    if (strcmp(token_list[idx - 1].val, program.var_list[i].name) == 0) {
                        program.cur_var = i;
                        find = 1;
                        var = program.var_list[i];
                        break;
                        }
                }
                for (size_t i = 0; i < program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_size; i++) {
                    if (strcmp(token_list[idx - 1].val, program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i].name) == 0) {
                        program.cur_var = i;
                        find = 1;
                        var = program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i];
                        break;
                    }
                }
            }
            if (!find) {
                char *msg = malloc(strlen(token_list[idx - 1].val) + 16);
                sprintf(msg, "'%s' is not a var", token_list[idx - 1].val);
                program_error(msg, program.pos_list[idx]);
                free(msg);
                return 0;
            }
            if (!var.arr) {
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

        if (program.cur_proc.size == 0) {
            char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 40)));
            sprintf(msg, "'%s' can only be used in a procedure", token_list[idx].val);
            program_error(msg, pos_list[idx]);
            free(msg);
            return 0;
        }
        // find var
        for (size_t i = 0; i < program.var_size; i++) {
            if (strcmp(program.var_list[i].name, token_list[idx].val) == 0) {
                token_list[idx].operation = OP_CALL_VAR;
                find = 1;
                break;
            }
        }
        for (size_t i = 0; i < program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_size; i++) {
            if (strcmp(token_list[idx].val, program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i].name)==0) {
                token_list[idx].operation = OP_CALL_VAR;
                find = 1;
                break;
            }
        }

        // find proc
        for (size_t i = 0; i < program.proc_size; i++) {
            if (strcmp(program.proc_list[i].name, token_list[idx].val) == 0) {
                token_list[idx].operation = OP_CALL_PROC;
                stack_push(&program.cur_proc, program.proc_list[i].adr);
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
    }
    case TKN_STR: {
        if (program.cur_proc.size == 0) {
            char *msg = malloc(sizeof(char) * (strlen(token_list[idx].val + 40)));
            sprintf(msg, "'%s' can only be used in a procedure", token_list[idx].val);
            program_error(msg, pos_list[idx]);
            free(msg);
            return 0;
        }
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

    program.vartype_list = malloc(sizeof(vartype_t));
    malloc_check(program.vartype_list, "malloc(program.vartype_list) in function program_init");
    program.vartype_alloc = 1;
    program.vartype_size = 0;

    program.var_list = malloc(sizeof(var_t));
    malloc_check(program.var_list, "malloc(program.var_list) in function program_init");
    program.var_alloc = 1;
    program.var_size = 0;

    program.str_list = malloc(sizeof(str_t));
    malloc_check(program.str_list, "malloc(program.str_list) in function program_init");
    program.str_alloc = 1;
    program.str_size = 0;

    program.proc_list = malloc(sizeof(proc_t));
    malloc_check(program.proc_list, "malloc(program.proc_list) in function program_init");
    program.proc_alloc = 1;
    program.proc_size = 0;

    program.condition = 0;
    program.setting = 0;
    program.address = 0;
    program.loop = 0;
    program.error = 0;
    program.proc_def = 0;
    program.has_malloc = 0;
    program.has_main = 0;
    program.size_of = 0;
    program.local_def = 0;
    program.global_def = 0;

    program.cur_proc = stack_create();

    program_add_vartype(vartype_create("byte", sizeof(char), 1));
    program_add_vartype(vartype_create("short", sizeof(short), 1));
    program_add_vartype(vartype_create("int", sizeof(int), 1));
    program_add_vartype(vartype_create("long", sizeof(long), 1));
    program_add_vartype(vartype_create("ptr", sizeof(long), 1));

    char *word = malloc(sizeof(char));
    malloc_check(word, "malloc(word) in function program_init");
    size_t word_size = 0;
    size_t word_alloc = 1;
    
    size_t line = 1;
    size_t col = 1;
    size_t col_word = 1;
    size_t str_line = 1;
    size_t str_col = 1;

    //char prv_char = 0;
    char cur_char = 0;
    char nxt_char = fgetc(f);

    int is_str = 0;

    while (cur_char != EOF) {
        //prv_char = cur_char;
        cur_char = nxt_char;
        nxt_char = fgetc(f);
        col++;
        if (!is_str) {
            if (cur_char == '"') {
                is_str = 1;
                str_line = line;
                str_col = col_word;
                continue;
            }
            if (cur_char == ' ' || cur_char == '"' || cur_char == '\t' || cur_char == '\n' || cur_char == EOF || cur_char == '[' || cur_char == ']' || cur_char == '$' || cur_char == '@' || cur_char == '!') {
                if (word_size > 0) {
                    word[word_size] = '\0';
                    program_add_token(pos_create(argv[1], line, col_word));
                    lex_word_as_token(word, 0, 0);
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
                if (cur_char == '[' || cur_char == ']' || cur_char == '$' || cur_char == '@' || cur_char == '!') {
                    word = realloc(word, sizeof(char) * 2);
                    word[0] = cur_char;
                    word[1] = '\0';
                    program_add_token(pos_create(argv[1], line, col_word));
                    lex_word_as_token(word, 0, 0); 
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
        } else {
            if (cur_char == '"') {
                is_str = 0;
                word[word_size] = '\0';
                size_t adr = 0;
                for (size_t i = 0; i < program.str_size; i++) {
                    if (strcmp(word, program.str_list[i].str) == 0) {
                        adr = program.str_list[i].adr;
                    }
                }
                program_add_token(pos_create(argv[1], str_line, str_col));
                if (adr == 0) {
                    program_add_str(str_create(word, program.idx));
                    adr = program.str_list[program.str_size - 1].adr;
                }
                lex_word_as_token(word, 1, adr);
                if (program.error) {
                    exit(1);
                }
                word = malloc(sizeof(char));
                malloc_check(word, "malloc(word) in function program_init");
                word_size = 0;
                word_alloc = 1;
                line = str_line;
                col = col_word;
                continue;
            } else if (cur_char == '\n') {
                line++;
                col = col_word = 1;
            } else {
                col_word = col;
                word_size++;
                if (word_size >= word_alloc) {
                    word_alloc *= 2;
                    word = realloc(word, sizeof(char) *word_alloc);
                    malloc_check(word, "realloc(word) in function program_init");
                }
                if (cur_char != '\\') {
                    word[word_size - 1] = cur_char;
                } else {
                    switch (nxt_char) {
                    case 'a':
                        word[word_size - 1] = '\a';
                        break;
                    case 'b':
                        word[word_size - 1] = '\b';
                        break;
                    case 'e':
                        word[word_size - 1] = '\e';
                        break;
                    case 'f':
                        word[word_size - 1] = '\f';
                        break;
                    case 'n':
                        word[word_size - 1] = '\n';
                        break;
                    case 'r':
                        word[word_size - 1] = '\r';
                        break;
                    case 't':
                        word[word_size - 1] = '\t';
                        break;
                    case 'v':
                        word[word_size - 1] = '\v';
                        break;
                    case '\\':
                        word[word_size - 1] = '\\';
                        break;
                    case '\'':
                        word[word_size - 1] = '\'';
                        break;
                    case '"':
                        word[word_size - 1] = '\"';
                        break;
                    case '?':
                        word[word_size - 1] = '\?';
                        break;
                    // TODO: add \nnn \xhh... \uhhhh \Uhhhhhhhh
                    default:
                        fprintf(stderr, "%s:%lu:%lu ERROR: unknown escape sequence: '\\%c'\n", argv[1], line, col, nxt_char);
                        program.error = 1;
                        break;
                    }
                    nxt_char = fgetc(f);
                }
            }
        }
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
    if (program.has_malloc) {
        fprintf(output, "extern malloc, free\n");
    }
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

    while (parse_current_token(program)) {
        size_t idx = program.idx;
        switch (program.token_list[idx].operation) {
        case OP_PUSH_INT: {
            fprintf(output, ";   push int\n");
            fprintf(output, "    mov rax,%s\n", program.token_list[idx].val);
            fprintf(output, "    push rax\n");
        } break;
        case OP_PUSH_STR: {
            fprintf(output, ";   push str\n");
            str_t str;
            for (size_t i = 0; i < program.str_size; i++) {
                if (program.str_list[i].adr == program.token_list[idx].jmp) {
                    str = program.str_list[i];
                }
            }
            fprintf(output, "    push %lu\n", str.len);
            fprintf(output, "    push $STR%lu\n", program.token_list[idx].jmp);
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
        case OP_STORE: {
            fprintf(output, ";   store\n");
            vartype_t vt = program.vartype_list[program.cur_vartype];
            if (vt.primitive) {
                fprintf(output, "    pop rax\n");
                fprintf(output, "    pop rbx\n");
                switch (vt.size_bytes) {
                case sizeof(char):
                    fprintf(output, "    mov byte [rbx],al\n");
                    break;
                case sizeof(short):
                    fprintf(output, "    mov word [rbx],ax\n");
                    break;
                case sizeof(int):
                    fprintf(output, "    mov dword [rbx],eax\n");
                    break;
                case sizeof(long):
                    fprintf(output, "    mov qword [rbx],rax\n");
                    break;
                }
            }
            program.idx++;
        } break;
        case OP_FETCH: {
            fprintf(output, ";   fetch\n");
            vartype_t vt = program.vartype_list[program.cur_vartype];
            if (vt.primitive) {
                fprintf(output, "    pop rbx\n");
                fprintf(output, "    xor rax,rax\n");
                switch (vt.size_bytes) {
                case sizeof(char):
                    fprintf(output, "    mov al,byte [rbx]\n");
                    break;
                case sizeof(short):
                    fprintf(output, "    mov ax,word [rbx]\n");
                    break;
                case sizeof(int):
                    fprintf(output, "    mov eax,dword [rbx]\n");
                    break;
                case sizeof(long):
                    fprintf(output, "    mov rax,qword [rbx]\n");
                    break;
                }
                fprintf(output, "    push rax\n");
            }
            program.idx++;
        } break;
        case OP_SIZEOF: {
            fprintf(output, ";   sizeof\n");
            vartype_t vt = program.vartype_list[program.cur_vartype];
            fprintf(output, "    push %lu\n", vt.size_bytes);
            program.idx++;
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
            int local = 0;
            var_t var;
            for (size_t i = 0; i < program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_size; i++) {
                if (strcmp(program.token_list[idx].val, program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i].name) == 0) {
                    local = 1;
                    var = program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i];
                    break;
                }
            }
            if (!local) {
                for (size_t i = 0; i < program.var_size; i++) {
                    if (strcmp(program.token_list[idx].val, program.var_list[i].name) == 0) {
                        var = program.var_list[i];
                        break;
                    }
                }
            }
            fprintf(output, ";   cap\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    push %lu\n", var.cap);
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
        case OP_DELETE: {
            fprintf(output, ";   delete memory\n");
            fprintf(output, "    pop rdi\n");
            fprintf(output, "    call free WRT ..plt\n");
        } break;
        case OP_MEMORY: {
            fprintf(output, ";   memory allocation\n");
            fprintf(output, "    pop rdi\n");
            fprintf(output, "    call malloc WRT ..plt\n");
            fprintf(output, "    push rax\n");
        } break;
        case OP_CALL_VAR: { 
            int local = 0;
            vartype_t l;
            var_t var;
            for (size_t i = 0; i < program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_size; i++) {
                if (strcmp(program.token_list[idx].val, program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i].name) == 0) {
                    local = 1;
                    var = program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i];
                    l = program.vartype_list[var.type];
                    break;
                }
            }
            if (!local) {
                for (size_t i = 0; i < program.var_size; i++) {
                    if (strcmp(program.token_list[idx].val, program.var_list[i].name) == 0) {
                        l = program.vartype_list[program.var_list[i].type];
                        var = program.var_list[i];
                        break;
                    }
                }
            }

            // TODO: for now 'set var' and 'get var' just supports primitive types
            if (program.setting && !var.arr && !program.index) { // set var value
                program.setting=0;
                fprintf(output, ";   set var value\n");
                fprintf(output, "    pop rax\n");
                if (l.primitive) {
                    if (!local) {
                        switch (l.size_bytes) {
                        case sizeof(char):
                            fprintf(output, "    mov byte [$VAR%lu],al\n", var.adr);
                            break;
                        case sizeof(short):
                            fprintf(output, "    mov word [$VAR%lu],ax\n", var.adr);
                            break;
                        case sizeof(int):
                            fprintf(output, "    mov dword [$VAR%lu],eax\n", var.adr);
                            break;
                        case sizeof(long):
                            fprintf(output, "    mov qword [$VAR%lu],rax\n", var.adr);
                            break;
                        }
                    } else {
                        fprintf(output, "    mov rbx,qword [$RETP]\n");
                        switch (l.size_bytes) {
                        case sizeof(char):
                            fprintf(output, "    mov byte [rbx - %lu],al\n", var.adr);
                            break;
                        case sizeof(short):
                            fprintf(output, "    mov word [rbx - %lu],ax\n", var.adr);
                            break;
                        case sizeof(int):
                            fprintf(output, "    mov dword [rbx - %lu],eax\n", var.adr);
                            break;
                        case sizeof(long):
                            fprintf(output, "    mov qword [rbx - %lu],rax\n", var.adr);
                            break;
                        }
                    }
                }
            } else if (program.address && !program.index && program.token_list[idx + 1].operation != OP_START_INDEX) { // get address
                program.address = 0;
                fprintf(output, ";   get var address\n");
                if (!local) {
                    fprintf(output, "    mov rax,$VAR%lu\n", var.adr);
                } else {
                    fprintf(output, "    mov rbx,qword [$RETP]\n");
                    fprintf(output, "    sub rbx,%lu\n", var.adr);
                    fprintf(output, "    mov rax,rbx\n");
                }
                fprintf(output, "    push rax\n");
            } else if (program.size_of && !program.index ) { // sizeof var
                program.size_of = 0;
                fprintf(output, ";   sizeof\n");
                if (!var.arr || program.token_list[idx + 1].operation == OP_START_INDEX) {
                    fprintf(output, "    push %lu\n", l.size_bytes);
                    if (program.token_list[idx + 1].operation == OP_START_INDEX) {
                        program.size_of = 1;
                    }
                } else {
                    fprintf(output, "    push %lu\n", l.size_bytes * var.cap);
                }
            } else {  // get var value
                fprintf(output, ";   get var value\n");
                if (l.primitive) {
                    if (!var.constant) {
                        fprintf(output, "    xor rax,rax\n");
                        if (!var.arr) {
                            if (!local) {
                                switch (l.size_bytes) {
                                case sizeof(char):
                                    fprintf(output, "    mov al,byte [$VAR%lu]\n", var.adr);
                                    break;
                                case sizeof(short):
                                    fprintf(output, "    mov ax,word [$VAR%lu]\n", var.adr);
                                    break;
                                case sizeof(int):
                                    fprintf(output, "    mov eax,dword [$VAR%lu]\n", var.adr);
                                    break;
                                case sizeof(long):
                                    fprintf(output, "    mov rax,qword [$VAR%lu]\n", var.adr);
                                    break;
                                }
                            } else {
                                fprintf(output, "    mov rbx,qword [$RETP]\n");
                                switch (l.size_bytes) {
                                case sizeof(char):
                                    fprintf(output, "    mov al,byte [rbx - %lu]\n", var.adr);
                                    break;
                                case sizeof(short):
                                    fprintf(output, "    mov ax,word [rbx - %lu]\n", var.adr);
                                    break;
                                case sizeof(int):
                                    fprintf(output, "    mov eax,dword [rbx - %lu]\n", var.adr);
                                    break;
                                case sizeof(long):
                                    fprintf(output, "    mov rax,qword [rbx - %lu]\n", var.adr);
                                    break;
                                }
                            }
                        } else {
                            if (!local) {
                                fprintf(output, "    mov rax, $VAR%lu\n", var.adr);
                            } else {
                                fprintf(output, "    mov rbx,qword [$RETP]\n");
                                fprintf(output, "    sub rbx,%lu\n", var.adr);
                                fprintf(output, "    mov rax,rbx\n");
                            }
                        }
                        fprintf(output, "    push rax\n");
                    } else {
                        fprintf(output, "    push $VAR%lu\n", var.adr);
                    }
                }
            }
        } break;
        case OP_END_INDEX: {
            program.index = 0;
            var_t var = program.var_list[program.cur_var];
            for (size_t i = 0; i < program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_size; i++) {
                if (strcmp(var.name, program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i].name) == 0) {
                    var = program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[i];
                    break;
                }
            }
            vartype_t l = program.vartype_list[var.type];
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
            } else if (program.address) {
                program.address = 0;
                fprintf(output, ";   get array address\n");
                fprintf(output, "    pop rax\n");
                fprintf(output, "    pop rbx\n");
                fprintf(output, "    mov rdx,%lu\n", l.size_bytes);
                fprintf(output, "    mul rdx\n");
                fprintf(output, "    lea rax,[rbx + rax]\n");
                fprintf(output, "    push rax\n");
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
        case OP_CALL_PROC: {
            fprintf(output, ";   call proc\n");
            if (strcmp(program.token_list[idx].val, "main") == 0) {
                fprintf(output, "    call main\n");
            } else {
                fprintf(output, "    call $PROC%lu\n", stack_pop(&program.cur_proc));
            }
        } break;
        case OP_CREATE_PROC: {
            program.idx++;
            int is_main = program.has_main && strcmp(program.token_list[idx + 1].val, "main") == 0;
            if (is_main) {
                fprintf(output, "global main\n");
            }
            fprintf(output, ";   create proc\n");
            if (is_main) {
                fprintf(output, "main:\n");
                fprintf(output, "    mov qword [$RETP], $RET\n");
            } else {
                fprintf(output, "$PROC%lu:\n", program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].adr);
            }
            fprintf(output, "    mov rax,qword [$RETP]\n");
            fprintf(output, "    pop qword [rax]\n");
            fprintf(output, "    add qword [$RETP],8\n");
        } break;
        case OP_DO: {
            fprintf(output, ";   do\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    test rax,rax\n");
            fprintf(output, "    jz $ADR%lu\n", program.token_list[idx].jmp);
        } break;
        case OP_ELSE: {
            fprintf(output, ";   else\n");
            fprintf(output, "    jmp $ADR%lu\n", program.token_list[idx].jmp);
            fprintf(output, "$ADR%lu:\n", program.idx);
        } break;
        case OP_LOOP: {
            fprintf(output, ";   loop\n");
            fprintf(output, "$ADR%lu:\n", program.idx);
        } break;
        case OP_END: {
            if (program.condition) {
                program.condition = 0;
                fprintf(output, ";   end\n");
                if (program.loop) {
                    program.loop = 0;
                    fprintf(output, "    jmp $ADR%lu\n", program.token_list[idx].jmp);
                }
                fprintf(output, "$ADR%lu:\n", program.idx);
            } else if (program.setting) {
                var_t var;
                int local = 0;
                if (program.local_def) {
                    program.local_def = 0;
                    local = 1;
                    var_t var = program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[program.cur_var];
                    fprintf(output, ";   create local varible\n");
                    if (!var.arr) {
                        fprintf(output, "    add qword [$RETP],%lu\n", program.vartype_list[var.type].size_bytes);
                    } else {
                        fprintf(output, "    add qword [$RETP],%lu\n", program.vartype_list[var.type].size_bytes * var.cap);
                    }
                } else if (program.global_def) {
                    var = program.var_list[program.cur_var];
                }
                vartype_t l = program.vartype_list[var.type];
                program.setting=0;
                if (!var.arr) {
                    fprintf(output, ";   set var value\n");
                    fprintf(output, "    pop rax\n");
                    if (l.primitive) {
                        if (!local) {
                            switch (l.size_bytes) {
                            case sizeof(char):
                                fprintf(output, "    mov byte [$VAR%lu],al\n", var.adr);
                                break;
                            case sizeof(short):
                                fprintf(output, "    mov word [$VAR%lu],ax\n", var.adr);
                                break;
                            case sizeof(int):
                                fprintf(output, "    mov dword [$VAR%lu],eax\n", var.adr);
                                break;
                            case sizeof(long):
                                fprintf(output, "    mov qword [$VAR%lu],rax\n", var.adr);
                                break;
                            }
                        } else {
                            fprintf(output, "    mov rbx,qword [$RETP]\n");
                            switch (l.size_bytes) {
                            case sizeof(char):
                                fprintf(output, "    mov byte [rbx - %lu],al\n", var.adr);
                                break;
                            case sizeof(short):
                                fprintf(output, "    mov word [rbx - %lu],ax\n", var.adr);
                                break;
                            case sizeof(int):
                                fprintf(output, "    mov dword [rbx - %lu],eax\n", var.adr);
                                break;
                            case sizeof(long):
                                fprintf(output, "    mov qword [rbx - %lu],rax\n", var.adr);
                                break;
                            }
                        }
                    }
                } else {
                    fprintf(output, ";   set array value\n");
                    fprintf(output, "    mov rcx,%lu\n", var.cap - 1);
                    fprintf(output, "$ADR%lu:\n", program.idx);
                    fprintf(output, "    mov rax,rcx\n");
                    fprintf(output, "    mov rdx,%lu\n", l.size_bytes);
                    fprintf(output, "    mul rdx\n");
                    if (!local) {
                        fprintf(output, "    lea rax,[$VAR%lu + rax]\n", var.adr);
                    } else {
                        // TODO: maybe a bug
                        fprintf(output, "    mov rdx,qword [RETP - %lu]\n", var.adr);
                        fprintf(output, "    lea rax,[rdx + rax]\n");
                    }
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
                    fprintf(output, "    jge $ADR%lu\n", program.idx);
                }
            } else if (program.global_def) {
                program.global_def = 0;
            } else if (program.local_def) {
                program.local_def = 0;
                var_t var = program.proc_list[program.cur_proc.stack[program.cur_proc.size - 1]].var_list[program.cur_var];
                fprintf(output, ";   create local varible\n");
                if (!var.arr) {
                    fprintf(output, "    add qword [$RETP],%lu\n", program.vartype_list[var.type].size_bytes);
                } else {
                    fprintf(output, "    add qword [$RETP],%lu\n", program.vartype_list[var.type].size_bytes * var.cap);
                }
            } else if (program.cur_proc.size != 0) {
                proc_t *proc = &(program.proc_list[stack_pop(&program.cur_proc)]);
                for (size_t i = 0; i < proc->var_size; i++) {
                    free(proc->var_list[i].name);
                }
                proc->var_size = 0;
                fprintf(output, ";   end proc\n");
                fprintf(output, "    sub qword [$RETP],%lu\n", proc->local_var_capacity + 8);
                fprintf(output, "    mov rax,qword [$RETP]\n");
                fprintf(output, "    push qword [rax]\n");
                if (strcmp(proc->name, "main") == 0) {
                    fprintf(output, "    xor rax,rax\n");
                }
                fprintf(output, "    ret\n");
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
    fprintf(output, "segment .bss\n");
    for (size_t i = 0; i < program.var_size; i++) {
        if (program.var_list[i].constant) continue;
        vartype_t l = program.vartype_list[program.var_list[i].type];
        size_t alloc = program.var_list[i].cap;
        if (l.primitive) {
            switch (l.size_bytes) {
            case sizeof(char):
                fprintf(output, "$VAR%lu: resb %lu\n", program.var_list[i].adr, alloc);
                break;
            case sizeof(short):
                fprintf(output, "$VAR%lu: resw %lu\n", program.var_list[i].adr, alloc);
                break;
            case sizeof(int):
                fprintf(output, "$VAR%lu: resd %lu\n", program.var_list[i].adr, alloc);
                break;
            case sizeof(long):
                fprintf(output, "$VAR%lu: resq %lu\n", program.var_list[i].adr, alloc);
                break;
            default:
                break;
            }
        }
    }
    fprintf(output, "$RET: resb %u\n", RET_STACK_CAP);
    fprintf(output, "$RETP: resq 1\n");
    fprintf(output, "segment .data\n");
    for (size_t i = 0; i < program.str_size; i++) {
        str_t str = program.str_list[i];
        fprintf(output, "$STR%lu: db ", str.adr);
        for (size_t i = 0; i < str.len; i++) {
            fprintf(output, "0x%x", str.str[i]);
            if (i < str.len - 1) {
                fprintf(output, ",");
            } else {
                fprintf(output, "\n");
            }
        }
    }
    for (size_t i = 0; i < program.var_size; i++) {
        if (!program.var_list[i].constant) continue;
        vartype_t l = program.vartype_list[program.var_list[i].type];
        if (l.primitive) {
            switch (l.size_bytes) {
            case sizeof(char):
                fprintf(output, "$VAR%lu: equ %d\n", program.var_list[i].adr, program.var_list[i].const_val.b8);
                break;
            case sizeof(short):
                fprintf(output, "$VAR%lu: equ %d\n", program.var_list[i].adr, program.var_list[i].const_val.b16);
                break;
            case sizeof(int):
                fprintf(output, "$VAR%lu: equ %u\n", program.var_list[i].adr, program.var_list[i].const_val.b32);
                break;
            case sizeof(long):
                fprintf(output, "$VAR%lu: equ %lu\n", program.var_list[i].adr, program.var_list[i].const_val.b64);
                break;
            default:
                break;
            }
        }
    }

    if (!program.has_main) {
        fprintf(stderr, "ERROR: program without a main entry point\n");
        exit(1);
    }

    fclose(output);
    system("nasm -felf64 -g output.asm -o output.o");
    system("gcc -no-pie -o output output.o");
}

void program_quit() {
    stack_destroy(program.cur_proc);
    for (size_t i = 0; i < program.token_size; i++) {
        free(program.token_list[i].val);
    }
    for (size_t i = 0; i < program.vartype_size; i++) {
        free(program.vartype_list[i].name);
    }
    for (size_t i = 0; i < program.var_size; i++) {
        free(program.var_list[i].name);
    }
    for (size_t i = 0; i < program.proc_size; i++) {
        free(program.proc_list[i].var_list);
    }
    free(program.pos_list);
    free(program.token_list);
    free(program.vartype_list);
    free(program.var_list);
    free(program.str_list);
    free(program.proc_list);
}

int main(int argc, char **argv) {
    program_init(argc, argv);
    generate_assembly_x86_64_linux();
    program_quit();
    return 0;
}

