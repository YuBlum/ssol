/*
Copyright (c) 2019 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
---------------------------------------------------------------------------------------
The library can be finded in here: https://github.com/nothings/stb/blob/master/stb_ds.h
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libgen.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define RET_STACK_CAP 65536 // 64kb

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
        // INTRINSICS
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
        OP_OVER,
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
        OP_SYSCALL0,
        OP_SYSCALL1,
        OP_SYSCALL2,
        OP_SYSCALL3,
        OP_SYSCALL4,
        OP_SYSCALL5,
        OP_SYSCALL6,
        // KEYWORDS
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
        OP_IMPORT,
        OP_EXPORT,
        //OP_REPEAT,
        //OP_BREAK,
        OP_END,
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
    char *type;
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
    size_t file_num;
    struct { char *key; var_t value; } *vars;
    size_t local_var_capacity;
} proc_t;

typedef struct {
    size_t file_num;
    char *file_name;
    char **file_path;

    token_t *tokens;
    pos_t *positions;

    struct { char *key; vartype_t value; } *types;
    struct { char *key; var_t value; } *vars;
    struct { char *key; str_t value; } *strs;
    struct { char *key; proc_t value; } *procs;

    struct { char *key; size_t *value; } *exports;
    size_t *imports;

    size_t idx;
    int error;
    int condition;
    int loop;
    int setting;
    int address;
    int exporting;
    int index;
    int size_of;
    int proc_def;
    int local_def;
    int global_def;
    int has_malloc;
    size_t idx_amount;
    char *cur_var;
    char *prv_var;
    char *cur_vartype;
    char **cur_proc;
} program_t;

program_t program;
int has_main_in_files = 0;

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
    proc.adr = shlenu(program.procs);
    proc.vars = NULL;
    proc.local_var_capacity = 0;
    proc.file_num = program.file_num;
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

var_t var_create(char *name, char *type_name, int arr, size_t cap) {
    if (shgetp_null(program.types, type_name) == NULL) return (var_t){.name=NULL};
    var_t var;
    var.name = malloc(sizeof(*var.name) * (strlen(name) + 1));
    malloc_check(var.name, "malloc(var.name) in function var_create");
    strcpy(var.name, name);
    var.type = type_name;
    var.arr = arr;
    var.cap = cap;
    var.constant = 0;
    var.adr = shlenu(program.vars);
    return var;
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
    if (idx >= arrlenu(program.tokens)) return 0;

    if (is_str) {
        token_set(&program.tokens[idx], TKN_STR, OP_PUSH_STR, word);
        program.tokens[idx].jmp = adr;
    } else if (strcmp(word, "+") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_PLUS, word);
    } else if (strcmp(word, "-") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_MINUS, word);
    } else if (strcmp(word, "*") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_MUL, word);
    } else if (strcmp(word, "/") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_DIV, word);
    } else if (strcmp(word, "%") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_MOD, word);
    } else if (strcmp(word, ">>") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SHR, word);
    } else if (strcmp(word, "<<") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SHL, word);
    } else if (strcmp(word, "&") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_BAND, word);
    } else if (strcmp(word, "|") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_BOR, word);
    } else if (strcmp(word, "~") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_BNOT, word);
    } else if (strcmp(word, "^") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_XOR, word);
    } else if (strcmp(word, "=") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SET_VAR, word);
    } else if (strcmp(word, "$") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_GET_ADR, word);
    } else if (strcmp(word, "!") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_STORE, word);
    } else if (strcmp(word, "@") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_FETCH, word);
    } else if (strcmp(word, "sizeof") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SIZEOF, word);
    } else if (strcmp(word, "==") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_EQUALS, word);
    } else if (strcmp(word, "!=") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_NOTEQUALS, word);
    } else if (strcmp(word, ">") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_GREATER, word);
    } else if (strcmp(word, "<") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_MINOR, word);
    } else if (strcmp(word, ">=") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_EQGREATER, word);
    } else if (strcmp(word, "<=") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_EQMINOR, word);
    } else if (strcmp(word, "not") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_NOT, word);
    } else if (strcmp(word, "print") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_PRINT, word);
    } else if (strcmp(word, "dup") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_DUP, word);
    } else if (strcmp(word, "swap") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SWAP, word);
    } else if (strcmp(word, "rot") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_ROT, word);
    } else if (strcmp(word, "over") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_OVER, word);
    } else if (strcmp(word, "drop") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_DROP, word);
    } else if (strcmp(word, "cap") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_CAP, word);
    } else if (strcmp(word, "[") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_START_INDEX, word);
    } else if (strcmp(word, "]") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_END_INDEX, word);
    } else if (strcmp(word, "syscall0") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SYSCALL0, word);
    } else if (strcmp(word, "syscall1") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SYSCALL1, word);
    } else if (strcmp(word, "syscall2") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SYSCALL2, word);
    } else if (strcmp(word, "syscall3") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SYSCALL3, word);
    } else if (strcmp(word, "syscall4") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SYSCALL4, word);
    } else if (strcmp(word, "syscall5") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SYSCALL5, word);
    } else if (strcmp(word, "syscall6") == 0) {
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_SYSCALL6, word);
    } else if (strcmp(word, "memory") == 0) {
        if (program.has_malloc == 0) program.has_malloc = 1;
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_MEMORY, word);
    } else if (strcmp(word, "delete") == 0) {
        if (program.has_malloc == 0) program.has_malloc = 1;
        token_set(&program.tokens[idx], TKN_INTRINSIC, OP_DELETE, word);
    } else if (strcmp(word, "do") == 0) {
        token_set(&program.tokens[idx], TKN_KEYWORD, OP_DO, word);
    } else if (strcmp(word, "if") == 0) {
        token_set(&program.tokens[idx], TKN_KEYWORD, OP_IF, word);
    } else if (strcmp(word, "else") == 0) {
        token_set(&program.tokens[idx], TKN_KEYWORD, OP_ELSE, word);
    } else if (strcmp(word, "loop") == 0) {
        token_set(&program.tokens[idx], TKN_KEYWORD, OP_LOOP, word);
    } else if (strcmp(word, "var") == 0) {
        token_set(&program.tokens[idx], TKN_KEYWORD, OP_CREATE_VAR, word);
    } else if (strcmp(word, "const") == 0) {
        token_set(&program.tokens[idx], TKN_KEYWORD, OP_CREATE_CONST, word);
    } else if (strcmp(word, "proc") == 0) {
        token_set(&program.tokens[idx], TKN_KEYWORD, OP_CREATE_PROC, word);
    } else if (strcmp(word, "import") == 0) {
        token_set(&program.tokens[idx], TKN_KEYWORD, OP_IMPORT, word);
    } else if (strcmp(word, "export") == 0) {
        token_set(&program.tokens[idx], TKN_KEYWORD, OP_EXPORT, word);
    } else if (strcmp(word, "end") == 0) {
        token_set(&program.tokens[idx], TKN_KEYWORD, OP_END, word);
    } else if (shgetp_null(program.types, word) != NULL) {
        token_set(&program.tokens[idx], TKN_TYPE, -1, word);
    } else if (word_is_int(word)) {
        token_set(&program.tokens[idx], TKN_INT, OP_PUSH_INT, word);
    } else {
        token_set(&program.tokens[idx], TKN_ID, -1, word);
    }
//    if (program.tokens[idx].type != -1)
//        print_token(program.tokens[idx]);
    program.idx++;
    return 1;
}

int parse_current_token() {
    size_t idx = program.idx;
    if (idx >= arrlenu(program.tokens)) return 0;
    token_t *tokens = program.tokens;
    pos_t *positions = program.positions;

    switch (tokens[idx].type) {
    case TKN_KEYWORD: {
        switch(tokens[idx].operation) {
        case OP_DO: {
            if (arrlenu(program.cur_proc) == 0) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 40)));
                sprintf(msg, "'%s' can only be used in a procedure", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            if (program.condition) {
                size_t jmp = 0;
                size_t if_count = 0;
                for (size_t i = idx + 1; i < arrlenu(program.tokens); i++) {
                    if (tokens[i].type != TKN_KEYWORD) continue;
                    if ((tokens[i].operation == OP_IF && tokens[i - 1].operation != OP_ELSE) || tokens[i].operation == OP_LOOP || tokens[i].operation == OP_CREATE_VAR ||  tokens[i].operation == OP_CREATE_CONST || tokens[i].operation == OP_CREATE_PROC) if_count++;
                    if (tokens[i].operation == OP_END) {
                        if (if_count > 0) {
                            if_count--;
                        } else {
                            jmp = i;
                            break;
                        }
                    } else if (tokens[i].operation == OP_ELSE && !program.loop) {
                        if (if_count == 0) {
                            jmp = i;
                            break;
                        }
                    }
                }
                if (jmp == 0) {
                    if (program.loop)
                        program_error("loop without a end", positions[idx]);
                    else
                        program_error("if without a end", positions[idx]);
                    return 0;
                }
                tokens[idx].jmp = jmp;
                program.loop = 0;
                program.condition = 0;
            } else {
                program_error("do without a if or loop", positions[idx]);
                return 0;
            }
        } break;
        case OP_IF: {
            if (arrlenu(program.cur_proc) == 0) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 40)));
                sprintf(msg, "'%s' can only be used in a procedure", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }

            program.condition = 1;
            if (tokens[idx].operation == OP_DO) {
                program_error("if condition can't be empty", program.positions[idx]);
                return 0;
            }
        } break;
        case OP_ELSE: {
            if (arrlenu(program.cur_proc) == 0) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 40)));
                sprintf(msg, "'%s' can only be used in a procedure", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }

            size_t jmp = 0;
            size_t if_count = 0;
            for (size_t i = idx + 1; i < arrlenu(program.tokens); i++) {
                if (tokens[i].type != TKN_KEYWORD) continue;
                if (((tokens[i].operation == OP_IF && tokens[i - 1].operation != OP_ELSE) && i > idx + 1) || tokens[i].operation == OP_LOOP || tokens[i].operation == OP_CREATE_VAR||  tokens[i].operation == OP_CREATE_CONST || tokens[i].operation == OP_CREATE_PROC) if_count++;
                if (tokens[i].operation == OP_END) {
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
                if (tokens[i].operation == OP_END) if_count++;
                if (tokens[i].operation == OP_LOOP || tokens[i].operation == OP_CREATE_VAR||  tokens[i].operation == OP_CREATE_CONST || tokens[i].operation == OP_CREATE_PROC) if_count--;
                if (tokens[i].operation == OP_IF && (i == 0 || tokens[i].operation != OP_ELSE)) {
                    if (if_count > 0) {
                        if_count--;
                    } else {
                        found_if = 1;
                        break;
                    }
                }
            }
            if (!found_if) {
                program_error("else without a if", positions[idx]);
                return 0;
            }
            if (jmp == 0) {
                program_error("else without a end", positions[idx]);
                return 0;
            }
            tokens[idx].jmp = jmp;
        } break;
        case OP_LOOP: {
            if (arrlenu(program.cur_proc) == 0) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 40)));
                sprintf(msg, "'%s' can only be used in a procedure", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }

            program.condition = 1;
            program.loop = 1;
            if (tokens[idx].operation == OP_DO) {
                program_error("loop condition can't be empty", positions[idx]);
                return 0;
            }
        } break;
        case OP_CREATE_VAR: {
            // get var name
            program.idx++;
            idx = program.idx;
            if (arrlenu(program.tokens) == program.idx) {
                program_error("variable definition is invalid", positions[idx-1]);
                return 0;
            }
            if (tokens[idx].type != TKN_ID) {
                char *msg = malloc(sizeof(char) * ((strlen(tokens[idx].val) * 2) + strlen(token_name[tokens[idx].type]) + 50));
                sprintf(msg, "trying to define var '%s', but '%s' is %s", tokens[idx].val, tokens[idx].val, token_name[tokens[idx].type]);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            if (arrlenu(program.cur_proc) == 0) {
                if (shgetp_null(program.vars, tokens[idx].val) != NULL) {
                    char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 30)));
                    sprintf(msg, "trying to redefine var '%s'", tokens[idx].val);
                    program_error(msg, positions[idx]);
                    free(msg);
                    return 0;
                }
            } else {
                proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
                if (shgetp_null(p.vars, tokens[idx].val) != NULL) {
                    char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 30)));
                    sprintf(msg, "trying to redefine var '%s'", tokens[idx].val);
                    program_error(msg, positions[idx]);
                    free(msg);
                    return 0;
                }
            }
            if (shgetp_null(program.procs, tokens[idx].val) != NULL) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 30)));
                sprintf(msg, "trying to redefine proc '%s' as var", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            char *name = tokens[idx].val;
            // get var type
            program.idx++;
            idx = program.idx;
            if (arrlenu(program.tokens) == program.idx) {
                program_error("variable definition is invalid", positions[idx-1]);
                return 0;
            }
            if (tokens[idx].type != TKN_TYPE) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val) * 2 + 50));
                sprintf(msg, "trying to define var as type '%s', but '%s' is not a type is a %s", tokens[idx].val, tokens[idx].val, token_name[tokens[idx].type]);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            char *type = tokens[idx].val;
            // verify if var is an array
            size_t end = 0;
            size_t *stack = NULL;
            for (size_t i = idx + 1; i < arrlenu(program.tokens); i++) {
                int invalid = 0;
                if (tokens[i].type == TKN_KEYWORD) {
                    if (tokens[i].operation == OP_END) {
                        end = i + 1;
                        break;
                    } else {
                        invalid = 1;
                    }
                } else if (tokens[i].type == TKN_INTRINSIC) {
                    switch (tokens[i].operation) {
                    case OP_PLUS: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a + b);
                    } break;
                    case OP_MINUS: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a - b);
                    } break;
                    case OP_MUL: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a * b);
                    } break;
                    case OP_DIV: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a / b);
                    } break;
                    case OP_MOD: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a % b);
                    } break;
                    case OP_SHR: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a >> b);
                    } break;
                    case OP_SHL: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a << b);
                    } break;
                    case OP_BAND: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a & b);
                    } break;
                    case OP_BOR: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a | b);
                    } break;
                    case OP_BNOT: {
                        size_t a = arrpop(stack);
                        arrput(stack, ~a);
                    } break;
                    case OP_XOR: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a ^ b);
                    } break;
                    case OP_SIZEOF: {
                        vartype_t vt;
                        int find_vt = 0;
                        if (shgetp_null(program.types, tokens[i + 1].val) != NULL) {
                            find_vt = 1;
                            vt = shget(program.types, tokens[i + 1].val);
                        }
                        if (find_vt) {
                            arrput(stack, vt.size_bytes);
                        } else {
                            invalid = 1;
                        }
                    } break;
                    default: {
                        invalid = 1;
                    } break;
                    }
                } else if (tokens[i].type == TKN_INT) {
                    arrput(stack, atol(tokens[i].val)); 
                } else if (tokens[i].type == TKN_ID) {
                    var_t var;
                    int find_v = 0;
                    if (shgetp_null(program.vars, tokens[i].val) != NULL) {
                        find_v = 1;
                        var = shget(program.vars, tokens[i].val);
                    }
                    if (find_v && var.constant && shget(program.types, var.type).primitive) {
                        switch (shget(program.types, var.type).size_bytes) {
                        case sizeof(char):
                            arrput(stack, var.const_val.b8);
                            break;
                        case sizeof(short):
                            arrput(stack, var.const_val.b16);
                            break;
                        case sizeof(int):
                            arrput(stack, var.const_val.b32);
                            break;
                        case sizeof(long):
                            arrput(stack, var.const_val.b64);
                            break;
                        }
                    } else {
                        invalid = 1;
                    }
                } else {
                    invalid = 1;
                }
                if (invalid) {
                    char *msg = malloc(sizeof(char) * (strlen(tokens[i].val) + 40));
                    sprintf(msg, "'%s' is not valid in a array definition", tokens[i].val);
                    program_error(msg, positions[idx]);
                    free(msg);
                    return 0;
                }
            }
            if (end  == 0) {
                program_error("creating var without a end", positions[idx]);
                return 0;
            }
            var_t var;
            if (arrlenu(stack) == 0) {
                var = var_create(name, type, 0, 1);
            } else if (arrlenu(stack) == 1) {
                var = var_create(name, type, 1, arrpop(stack));
            } else {
                arrfree(stack);
                program_error("array definition can only have one constant value", positions[idx]);
                return 0;
            }
            arrfree(stack);
            if (var.name == NULL) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val) + 25));
                sprintf(msg, "type '%s' don't exists", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            if (arrlenu(program.cur_proc) == 0) {
                shput(program.vars, var.name, var);
                if (program.setting) {
                    program.cur_var = var.name;
                }
                program.global_def = 1;
                program.local_def = 0;
            } else {
                proc_t *proc = &(shgetp_null(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1])->value); 
                shput(proc->vars, var.name, var);
                var_t *v = &(shgetp_null(proc->vars, var.name)->value);
                size_t add_offset = shget(program.types, v->type).size_bytes;
                if (v->arr) {
                    add_offset *= v->cap;
                }
                v->adr = 0;
                for (size_t i = 0; i < shlenu(proc->vars); i++) {
                    proc->vars[i].value.adr += add_offset;
                }
                proc->local_var_capacity += add_offset;
                program.cur_var = var.name;
                program.local_def = 1;
                program.global_def = 0;
            }
            program.idx = end - 1;
        } break;
        case OP_CREATE_CONST: {
            // get var name
            if (arrlenu(program.cur_proc) != 0) {
                program_error("can't define a constant inside a procedure", positions[idx-1]);
                return 0;
            }
            program.idx++;
            idx = program.idx;
            if (arrlenu(program.tokens) == program.idx) {
                program_error("constant definition is invalid", positions[idx-1]);
                return 0;
            }
            if (tokens[idx].type != TKN_ID) {
                char *msg = malloc(sizeof(char) * ((strlen(tokens[idx].val) * 2) + strlen(token_name[tokens[idx].type]) + 50));
                sprintf(msg, "trying to define const '%s', but '%s' is %s", tokens[idx].val, tokens[idx].val, token_name[tokens[idx].type]);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            if (shgetp_null(program.vars, tokens[idx].val) != NULL) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 30)));
                sprintf(msg, "trying to redefine const '%s'", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            if (shgetp_null(program.procs, tokens[idx].val) != NULL) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 30)));
                sprintf(msg, "trying to redefine proc '%s' as const", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            char *name = tokens[idx].val;
            // get var type
            program.idx++;
            idx = program.idx;
            if (arrlenu(program.tokens) == program.idx) {
                program_error("constant definition is invalid", positions[idx-1]);
                return 0;
            }
            if (tokens[idx].type != TKN_TYPE) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val) * 2 + 50));
                sprintf(msg, "trying to define const as type '%s', but '%s' is not a type is a %s", tokens[idx].val, tokens[idx].val, token_name[tokens[idx].type]);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            char *type = tokens[idx].val;
            // TODO: for now constants can't be arrays and just support primitive types
            // get const value
            size_t end = 0;
            size_t *stack = NULL;
            for (size_t i = idx + 1; i < arrlenu(program.tokens); i++) {
                int invalid = 0;
                if (tokens[i].type == TKN_KEYWORD) {
                    if (tokens[i].operation == OP_END) {
                        end = i + 1;
                        break;
                    } else {
                        invalid = 1;
                    }
                } else if (tokens[i].type == TKN_INTRINSIC) {
                    switch (tokens[i].operation) {
                    case OP_PLUS: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a + b);
                    } break;
                    case OP_MINUS: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a - b);
                    } break;
                    case OP_MUL: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a * b);
                    } break;
                    case OP_DIV: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a / b);
                    } break;
                    case OP_MOD: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a % b);
                    } break;
                    case OP_SHR: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a >> b);
                    } break;
                    case OP_SHL: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a << b);
                    } break;
                    case OP_BAND: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a & b);
                    } break;
                    case OP_BOR: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a | b);
                    } break;
                    case OP_BNOT: {
                        size_t a = arrpop(stack);
                        arrput(stack, ~a);
                    } break;
                    case OP_XOR: {
                        size_t b = arrpop(stack);
                        size_t a = arrpop(stack);
                        arrput(stack, a ^ b);
                    } break;
                    case OP_SIZEOF: {
                        vartype_t vt;
                        int find_vt = 0;
                        if (shgetp_null(program.types, tokens[i + 1].val) != NULL) {
                            find_vt = 1;
                            vt = shget(program.types, tokens[i + 1].val);
                        }
                        if (find_vt) {
                            arrput(stack, vt.size_bytes);
                            i++;
                        } else {
                            invalid = 1;
                        }
                    } break;
                    default: {
                        invalid = 1;
                    } break;
                    }
                } else if (tokens[i].type == TKN_INT) {
                    arrput(stack, atol(tokens[i].val)); 
                } else if (tokens[i].type == TKN_ID) {
                    var_t var;
                    int find_v = 0;
                    if (shgetp_null(program.vars, tokens[i].val) != NULL) {
                        find_v = 1;
                        var = shget(program.vars, tokens[i].val);
                    }
                    if (find_v && var.constant && shget(program.types, var.type).primitive) {
                        switch (shget(program.types, var.type).size_bytes) {
                        case sizeof(char):
                            arrput(stack, var.const_val.b8);
                            break;
                        case sizeof(short):
                            arrput(stack, var.const_val.b16);
                            break;
                        case sizeof(int):
                            arrput(stack, var.const_val.b32);
                            break;
                        case sizeof(long):
                            arrput(stack, var.const_val.b64);
                            break;
                        }
                    } else {
                        invalid = 1;
                    }
                } else {
                    invalid = 1;
                }
                if (invalid) {
                    char *msg = malloc(sizeof(char) * (strlen(tokens[i].val) + 40));
                    sprintf(msg, "'%s' is not valid in a const definition", tokens[i].val);
                    program_error(msg, positions[idx]);
                    free(msg);
                    return 0;
                }
            }
            if (end  == 0) {
                program_error("creating const without a end", positions[idx]);
                return 0;
            }
            var_t var;
            if (arrlenu(stack) == 1) {
                var = var_create(name, type, 0, 1);
                vartype_t vt = shget(program.types, var.type);
                var.constant = 1;
                if (vt.primitive) {
                    switch (vt.size_bytes) {
                    case sizeof(char):
                        var.const_val.b8 = arrpop(stack);
                        break;
                    case sizeof(short):
                        var.const_val.b16 = arrpop(stack);
                        break;
                    case sizeof(int):
                        var.const_val.b32 = arrpop(stack);
                        break;
                    case sizeof(long):
                        var.const_val.b64 = arrpop(stack);
                        break;
                    }
                }
            } else {
                char *msg = malloc(sizeof(char) * 92);
                sprintf(msg, "const definition can only have 1 constant value, but got %lu", arrlenu(stack));
                program_error(msg, positions[idx]);
                free(msg);
                arrfree(stack);
                return 0;
            }
            arrfree(stack);
            if (var.name == NULL) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val) + 25));
                sprintf(msg, "type '%s' don't exists", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            shput(program.vars, var.name, var);
            program.idx = end - 1;
            program.cur_var = var.name;
        } break;
        case OP_CREATE_PROC: {
            
            if (arrlenu(program.cur_proc) != 0) {
                program_error("trying to define a procedure inside a procedure", positions[idx-1]);
                return 0;
            }
            // get proc name
            idx = program.idx + 1;
            if (arrlenu(program.tokens) == program.idx) {
                program_error("procedure definition is invalid", positions[idx-1]);
                return 0;
            }
            if (tokens[idx].type != TKN_ID) {
                char *msg = malloc(sizeof(char) * ((strlen(tokens[idx].val) * 2) + strlen(token_name[tokens[idx].type]) + 50));
                sprintf(msg, "trying to define proc '%s', but '%s' is %s", tokens[idx].val, tokens[idx].val, token_name[tokens[idx].type]);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            if (shgetp_null(program.vars, tokens[idx].val) != NULL) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 30)));
                sprintf(msg, "trying to redefine var '%s' as proc", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
            }
            if (shgetp_null(program.procs, tokens[idx].val) != NULL) {
                char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 30)));
                sprintf(msg, "trying to redefine proc '%s'", tokens[idx].val);
                program_error(msg, positions[idx]);
                free(msg);
                return 0;
            }
            char *name = tokens[idx].val;
            if (strcmp(name, "main") == 0) {
                has_main_in_files++;
            }
            if (has_main_in_files > 1) program_error("multiple definition of main", positions[idx]);
            proc_t proc = proc_create(name);
            shput(program.procs, proc.name, proc);
            arrput(program.cur_proc, proc.name);
            program.proc_def = 1;
            size_t end = 0;
            size_t end_count = 0;
            for (size_t i = idx + 1; i < arrlenu(program.tokens); i++) {
                if (tokens[i].type != TKN_KEYWORD) continue;
                if ((tokens[i].operation == OP_IF && tokens[i - 1].operation != OP_ELSE) || tokens[i].operation == OP_LOOP || tokens[i].operation == OP_CREATE_VAR ||  tokens[i].operation == OP_CREATE_CONST || tokens[i].operation == OP_CREATE_PROC) end_count++;
                if (tokens[i].operation == OP_END) {
                    if (end_count > 0) {
                        end_count--;
                    } else {
                        end = i;
                        break;
                    }
                }
            }
            if (end == 0) {
                program_error("proc without a end", positions[idx]);
                return 0;
            }
            //program.procs[program.proc_size - 1].end = end;
        } break;
        case OP_EXPORT: {
            int end = 0;
            // TODO: for now exports only accepts procedures
            if (shgetp_null(program.exports, program.file_name) != NULL) {
                program_error("'export' already exists for this file", positions[idx]);
                return 0;
            }
            if (idx == arrlenu(tokens)) {
                program_error("'export' without a end", positions[idx]);
                return 0;
            }
            if (tokens[idx + 1].operation == OP_END) {
                program_error("'export' is empty", positions[idx]);
                return 0;
            }
            size_t *export = NULL;
            arrput(export, program.file_num);
            for (size_t i = idx + 1; i < arrlenu(program.tokens); i++) {
                if (tokens[i].operation == OP_END) {
                    end = i;
                    break;
                } else if ((shgetp_null(program.procs, tokens[i].val) == NULL || shget(program.procs, tokens[i].val).file_num != program.file_num)) {
                    char *msg = malloc(sizeof(char) * (strlen(tokens[i].val) + 40));
                    sprintf(msg, "'%s' is not valid in export", tokens[i].val);
                    program_error(msg, positions[idx]);
                    free(msg);
                    return 0;
                }
                arrput(export, shget(program.procs, tokens[i].val).adr);
            }
            if (end == 0) {
                program_error("'export' without a end", positions[idx]);
                return 0;
            }
            shput(program.exports, program.file_name, export);
            program.idx = end;
        } break;
        case OP_IMPORT: {
            if (idx == arrlenu(tokens) || tokens[idx + 1].type != TKN_STR) {
                program_error("'import' without a file path", positions[idx + 1]);
                return 0;
            }
            if (shgetp_null(program.exports, tokens[idx + 1].val) == NULL) {
                char *msg = malloc(sizeof(char) * (28 + strlen(tokens[idx + 1].val)));
                sprintf(msg, "'%s' is not a valid file", tokens[idx + 1].val);
                program_error(msg, positions[idx + 1]);
                free(msg);
                return 0;
            }
            arrput(program.imports, shget(program.exports, tokens[idx + 1].val)[0]);
        } break;
        case OP_END: {
            size_t end_count = 0;
            int found_open = 0;
            for (long i = idx - 1; i >= 0; i--) {
                if (tokens[i].type != TKN_KEYWORD) continue;
                if (idx == 0 || (long)i < 0) break;
                if (tokens[i].operation == OP_END) end_count++;
                if ((tokens[i].operation == OP_IF  && tokens[i - 1].operation != OP_ELSE) || tokens[i].operation == OP_LOOP || tokens[i].operation == OP_CREATE_VAR || tokens[i].operation == OP_CREATE_PROC || tokens[i].operation == OP_EXPORT) {
                    if (end_count > 0) {
                        end_count--;
                    } else {
                        found_open = 1;
                        program.condition = tokens[i].operation != OP_CREATE_VAR && tokens[i].operation != OP_CREATE_PROC && tokens[i].operation != OP_EXPORT;
                        if (tokens[i].operation == OP_LOOP) {
                            program.loop = 1;
                            tokens[idx].jmp = i;
                        }
                        break;
                    }
                }
            }
            if (!found_open) {
                program_error("end without a opening", positions[idx]);
                return 0;
            }
        } break;
        default: {
            char *msg = malloc(strlen(tokens[idx].val) + 32);
            sprintf(msg, "undefined keyword '%s'", tokens[idx].val);
            program_error(msg, program.positions[idx]);
            free(msg);
            return 0;
        } break;
        }
    } break;
    case TKN_INTRINSIC: {
        if (arrlenu(program.cur_proc) == 0 && (tokens[idx].operation != OP_PLUS && tokens[idx].operation != OP_MINUS && tokens[idx].operation != OP_MUL && tokens[idx].operation != OP_DIV && tokens[idx].operation != OP_MOD && tokens[idx].operation != OP_SHR && tokens[idx].operation != OP_SHL && tokens[idx].operation != OP_BAND && tokens[idx].operation != OP_BOR && tokens[idx].operation != OP_BNOT && tokens[idx].operation != OP_XOR)) {
            char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 40)));
            sprintf(msg, "'%s' can only be used in a procedure", tokens[idx].val);
            program_error(msg, positions[idx]);
            free(msg);
            return 0;
        }
        switch(tokens[idx].operation) {
        case OP_SIZEOF: {
            int find = 0;
             if (tokens[idx + 1].type == TKN_ID) {
                 proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
                 if (shgetp_null(program.vars, tokens[idx + 1].val) != NULL || shgetp_null(p.vars, tokens[idx + 1].val) != NULL) {
                     find = 1;
                 }
            }
            if (find) {
                program.size_of = 1;
                program.tokens[idx].operation = -1;
                break;
            }
        }
        case OP_FETCH:
        case OP_STORE: {
            int find = 0;

            if (shgetp_null(program.types, tokens[idx + 1].val) != NULL) {
                find = 1;
                program.cur_vartype = tokens[idx + 1].val;
            }
            if (!find) {
                char *msg = malloc(strlen(tokens[idx + 1].val) + 16);
                sprintf(msg, "'%s' is not a type", tokens[idx + 1].val);
                program_error(msg, program.positions[idx]);
                free(msg);
                return 0;
            }
        } break;
        case OP_GET_ADR: {
            int find = 0;
            var_t var = {0};
            if (tokens[idx + 1].type == TKN_ID) {
                int local = 0;
                proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
                if (shgetp_null(p.vars, tokens[idx + 1].val) != NULL) {
                    local = 1;
                    find = 1;
                    var = shget(p.vars, tokens[idx + 1].val);
                }
                if (!local) {
                     if (shgetp_null(program.vars, tokens[idx + 1].val) != NULL) {
                         find = 1;
                         var = shget(program.vars, tokens[idx + 1].val);
                     }
                }
            } else if (tokens[idx + 1].operation == OP_CREATE_VAR) {
                find = 1;
            }
            if (!find) {
                char *msg = malloc(strlen(tokens[idx + 1].val) + 16);
                sprintf(msg, "'%s' is not a var", tokens[idx + 1].val);
                program_error(msg, program.positions[idx]);
                free(msg);
                return 0;
            }

            if (var.constant) {
                program_error("you can't get the address of an constant", program.positions[idx]);
                return 0;
            }

            program.address = 1;
        } break;
        case OP_SET_VAR: {
            int find = 0;
            var_t var = {0};

            if (tokens[idx + 1].type == TKN_ID) {
                if (shgetp_null(program.vars, tokens[idx + 1].val) != NULL) {
                    find = 1;
                    var = shget(program.vars, tokens[idx + 1].val);
                }
                proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
                if (shgetp_null(p.vars, tokens[idx + 1].val) != NULL) {
                    find = 1;
                    var = shget(p.vars, tokens[idx + 1].val);
                }
            } else if (tokens[idx + 1].operation == OP_CREATE_VAR) {
                find = 1;
            }
            if (!find) {
                char *msg = malloc(strlen(tokens[idx + 1].val) + 16);
                sprintf(msg, "'%s' is not a var", tokens[idx + 1].val);
                program_error(msg, program.positions[idx]);
                free(msg);
                return 0;
            }

            if (var.constant) {
                program_error("trying to modify a constant variable", program.positions[idx]);
                return 0;
            }

            if (tokens[idx + 1].type == TKN_ID && var.arr && tokens[idx + 2].operation != OP_START_INDEX) {
                char *msg = malloc(strlen(tokens[idx + 1].val) + 16);
                sprintf(msg, "'%s' value is not changeable", tokens[idx + 1].val);
                program_error(msg, program.positions[idx]);
                free(msg);
                return 0;
            }
            program.setting = 1;
        } break;
        case OP_START_INDEX: {
            int find = 0;
            if (!program.size_of) {
                var_t var;
                if (tokens[idx - 1].type == TKN_ID) {
                    if (shgetp_null(program.vars, tokens[idx - 1].val) != NULL) {
                        program.cur_var = tokens[idx - 1].val;
                        find = 1;
                        var = shget(program.vars, tokens[idx - 1].val);
                    }
                    proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
                    if (shgetp_null(p.vars, tokens[idx - 1].val) != NULL) {
                        program.cur_var = tokens[idx - 1].val;
                        find = 1;
                        var = shget(p.vars, tokens[idx -1].val);
                        program.local_def = 1;
                    }
                }
                if (!find) {
                    char *msg = malloc(strlen(tokens[idx - 1].val) + 16);
                    sprintf(msg, "'%s' is not a var", tokens[idx - 1].val);
                    program_error(msg, program.positions[idx]);
                    free(msg);
                    return 0;
                }

                if (program.index) {
                    program_error("trying to use '[]' operator inside a '[]' operator", program.positions[idx]);
                    return 0;
                }

                if (!var.arr) {
                    program_error("'[]' can only be used in arrays", program.positions[idx]);
                    return 0;
                }
                program.index = 1;
            } else {
                for (size_t i = idx + 1; i < arrlenu(program.tokens); i++) {
                    if (program.tokens[i].operation == OP_END_INDEX) {
                        find = 1;
                        program.idx = i + 1;
                        if  (program.idx >= arrlenu(program.tokens)) return 0;
                    }
                }
                if (!find) {
                    program_error("'[' without ']'", program.positions[idx]);
                    return 0;
                }
            }
        } break;
        case OP_END_INDEX: {
            if (!program.index) {
                program_error("']' without a '['", program.positions[idx]);
                return 0;
            }
            if (program.idx_amount != 1) {
                char *msg = malloc(strlen(tokens[idx - 1].val) + 72);
                sprintf(msg, "'[]' needs to receive just 1 value, but get '%lu'", program.idx_amount);
                program_error(msg, program.positions[idx]);
                free(msg);
                return 0;
            }
            if (program.index) 
                program.idx_amount--;
        } break;
        case OP_CAP: {
            int find = 0;
            var_t var;
            if (tokens[idx - 1].type == TKN_ID) {
                if (shgetp_null(program.vars, tokens[idx - 1].val) != NULL) {
                    program.prv_var = program.cur_var;
                    program.cur_var = tokens[idx - 1].val;
                    find = 1;
                    var = shget(program.vars, tokens[idx - 1].val);
                }
                proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
                if (shgetp_null(p.vars, tokens[idx - 1].val) != NULL) {
                    program.prv_var = program.cur_var;
                    program.cur_var = tokens[idx - 1].val;
                    find = 1;
                    var = shget(p.vars, tokens[idx -1].val);
                }
            }
            if (!find) {
                char *msg = malloc(strlen(tokens[idx - 1].val) + 16);
                sprintf(msg, "'%s' is not a var", tokens[idx - 1].val);
                program_error(msg, program.positions[idx]);
                free(msg);
                return 0;
            }
            if (!var.arr) {
                program_error("'cap' can only be used in arrays", program.positions[idx]);
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
        case OP_OVER:
            if (program.index) {
                if (program.idx_amount > 0) program.idx_amount--;
                program.idx_amount+=2;
            }
            break;
        case OP_SWAP:
            if (program.index && program.idx_amount == 0) {
                program.idx_amount++;
            }
            break;
        case OP_ROT: // TODO: maybe this not work because rotate might need +2 to the program.idx_amount instead of +1
            if (program.index && program.idx_amount == 0) {
                program.idx_amount+=2;
            } else if (program.index && program.idx_amount == 1) {
                program.idx_amount++;
            }
            break;
        default:
            break;
        }
    } break;
    case TKN_ID: {
        int find = 0;

        if (arrlenu(program.cur_proc) == 0) {
            char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 40)));
            sprintf(msg, "'%s' can only be used in a procedure", tokens[idx].val);
            program_error(msg, positions[idx]);
            free(msg);
            return 0;
        }
        // find var
        if (shgetp_null(program.vars, tokens[idx].val) != NULL) {
            tokens[idx].operation = OP_CALL_VAR;
            find = 1;
        }
        proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
        if (shgetp_null(p.vars, tokens[idx].val) != NULL) {
            tokens[idx].operation = OP_CALL_VAR;
            find = 1;
        }
        // find proc
        if (shgetp_null(program.procs, tokens[idx].val) != NULL) {
            if (shget(program.procs, tokens[idx].val).file_num == program.file_num) {
                tokens[idx].operation = OP_CALL_PROC;
                arrput(program.cur_proc, tokens[idx].val);
                find = 1;
                break;
            }
            if (!find) {
                for (size_t i = 0; i < arrlenu(program.imports); i++) {
                    if (shget(program.procs, tokens[idx].val).file_num == program.imports[i]) {
                        tokens[idx].operation = OP_CALL_PROC;
                        arrput(program.cur_proc, tokens[idx].val);
                        find = 1;
                        break;
                    }
                }
            }
        }
        if (!find) {
            char *msg = malloc(strlen(tokens[idx].val) + 20);
            sprintf(msg, "undefined word '%s'", tokens[idx].val);
            program_error(msg, program.positions[idx]);
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
        if (arrlen(program.cur_proc) == 0) {
            char *msg = malloc(sizeof(char) * (strlen(tokens[idx].val + 40)));
            sprintf(msg, "'%s' can only be used in a procedure", tokens[idx].val);
            program_error(msg, positions[idx]);
            free(msg);
            return 0;
        }
    } break;
    default:
        break;
    }

    return 1;
}

void lex_file(char *file_name) {
    FILE *f = fopen(file_name, "r");
    if (f == NULL) {
        fprintf(stderr, "[ERROR] FIle '%s' don't exist's or is not writable\n", file_name);
        exit(1);
    }

    char *word = malloc(sizeof(char));
    malloc_check(word, "malloc(word) in function lex_file");
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
    int is_char = 0;
    int is_cmt = 0;

    while (cur_char != EOF) {
        //prv_char = cur_char;
        cur_char = nxt_char;
        nxt_char = fgetc(f);
        col++;
        if (!is_cmt) {
            if (!is_str) {
                if (cur_char == '"' || cur_char == '\'') {
                    is_str = 1;
                    str_line = line;
                    str_col = col_word;
                    if (cur_char == '\'') is_char = 1;
                    continue;
                }
                if (cur_char == ' ' || cur_char == '"' || cur_char == '\'' || cur_char == '\t' || cur_char == '\n' || cur_char == EOF || cur_char == '[' || cur_char == ']' || cur_char == '$' || cur_char == '@' || (cur_char == '!' && nxt_char != '=') || (cur_char == '/' && nxt_char == '/')) {
                    if (word_size > 0) {
                        word[word_size] = '\0';
                        arrput(program.tokens, token_create());
                        arrput(program.positions, pos_create(file_name, line, col_word));
                        lex_word_as_token(word, 0, 0);
                        if (program.error) {
                            exit(1);
                        }
                        word = malloc(sizeof(char));
                        malloc_check(word, "malloc(word) in function lex_file");
                        word_size = 0;
                        word_alloc = 1;
                    }
                    if (cur_char == '\n') {
                       line++;
                       col = col_word = 1;
                    } else if (cur_char == '/' && nxt_char == '/') {
                       line++;
                       col = col_word = 1;
                       is_cmt = 1;
                    } else {
                       col_word = col;
                    }
                    if (cur_char == '[' || cur_char == ']' || cur_char == '$' || cur_char == '@' || (cur_char == '!' && nxt_char != '=')) {
                        word = realloc(word, sizeof(char) * 2);
                        word[0] = cur_char;
                        word[1] = '\0';
                        arrput(program.tokens, token_create());
                        arrput(program.positions, pos_create(file_name, line, col_word));
                        lex_word_as_token(word, 0, 0); 
                        if (program.error) {
                            exit(1);
                        }
                        word = malloc(sizeof(char));
                        malloc_check(word, "malloc(word) in function lex_file");
                        word_size = 0;
                        word_alloc = 1;
                    }
                    continue;
                }
                word_size++;
                if (word_size >= word_alloc) {
                    word_alloc *= 2;
                    word = realloc(word, sizeof(char) *word_alloc);
                    malloc_check(word, "realloc(word) in function lex_file");
                }
                word[word_size - 1] = cur_char;
            } else {
                if ((cur_char == '"' && !is_char) || (cur_char == '\'' && is_char)) {
                    is_str = 0;
                    if (is_char) {
                        is_char = 0;
                        if (word_size != 1) {
                            fprintf(stderr, "%s:%lu:%lu ERROR: invalid character: '%s'\n", file_name, line, col, word);
                            exit(1);
                        }
                        int c = word[0];
                        free(word);
                        word = malloc(sizeof(char) * 5);
                        sprintf(word, "%d", c);

                        arrput(program.tokens, token_create());
                        arrput(program.positions, pos_create(file_name, str_line, str_col));
                        lex_word_as_token(word, 0, 0);
                    } else {
                        word[word_size] = '\0';
                        size_t adr = 0;
                        if (shgetp_null(program.strs, word) != NULL) {
                            adr = shget(program.strs, word).adr;
                        }
                        arrput(program.tokens, token_create());
                        arrput(program.positions, pos_create(file_name, str_line, str_col));
                        if (adr == 0) {
                            shput(program.strs, word, str_create(word, program.idx));
                            adr = shget(program.strs, word).adr;
                        }
                        lex_word_as_token(word, 1, adr);
                    }
                    if (program.error) {
                        exit(1);
                    }
                    word = malloc(sizeof(char));
                    malloc_check(word, "malloc(word) in function lex_file");
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
                        malloc_check(word, "realloc(word) in function lex_file");
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
                            fprintf(stderr, "%s:%lu:%lu ERROR: unknown escape sequence: '\\%c'\n", file_name, line, col, nxt_char);
                            exit(1);
                            break;
                        }
                        nxt_char = fgetc(f);
                    }
                }
            }
        } else if (cur_char == '\n') {
            is_cmt = 0;
        }
    }
    free(word);
    fclose(f);
}

void file_open(int file_num, char *file_path, int start) {
    program.file_num = file_num;
    arrput(program.file_path, malloc(strlen(file_path) + 1));
    strcpy(program.file_path[arrlenu(program.file_path) - 1], file_path);
    program.file_name = basename(program.file_path[arrlenu(program.file_path) - 1]);
    program.idx = start;


    program.types = NULL;
    program.vars = NULL;
    program.strs = NULL;
    program.imports = NULL;

    program.condition = 0;
    program.setting = 0;
    program.address = 0;
    program.exporting = 0;
    program.loop = 0;
    program.error = 0;
    program.proc_def = 0;
    program.has_malloc = 0;
    program.size_of = 0;
    program.local_def = 0;
    program.global_def = 0;

    program.cur_proc = NULL;

    vartype_t vt;
    vt = vartype_create("byte", sizeof(char), 1);
    shput(program.types, vt.name, vt);

    vt = vartype_create("short", sizeof(short), 1);
    shput(program.types, vt.name, vt);

    vt = vartype_create("int", sizeof(int), 1);
    shput(program.types, vt.name, vt);
    
    vt = vartype_create("long", sizeof(long), 1);
    shput(program.types, vt.name, vt);

    vt = vartype_create("ptr", sizeof(void *), 1);
    shput(program.types, vt.name, vt);

    lex_file(program.file_path[arrlenu(program.file_path) - 1]);

//    for (size_t i = 0; i < arrlenu(program.tokens); i++) {
//        printf("token: %s, val: %s\n", token_name[program.tokens[i].type], program.tokens[i].val);
//    }

    program.idx = start;
}

void generate_assembly_x86_64_linux() {
    char *asmfile = malloc(sizeof(char) * 37);
    sprintf(asmfile, "file%lu.asm", program.file_num);
    FILE *output = fopen(asmfile, "w");
    free(asmfile);
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

    if (program.file_num > 1) {
        fprintf(output, "extern $RET, $RETP\n");
    }
    while (parse_current_token(program)) {
        size_t idx = program.idx;
        switch (program.tokens[idx].operation) {
        case OP_PUSH_INT: {
            fprintf(output, ";   push int\n");
            fprintf(output, "    mov rax,%s\n", program.tokens[idx].val);
            fprintf(output, "    push rax\n");
        } break;
        case OP_PUSH_STR: {
            fprintf(output, ";   push str\n");
            str_t str;
            str = shget(program.strs, program.tokens[idx].val);
            fprintf(output, "    push %lu\n", str.len);
            fprintf(output, "    push $STR%lu\n", program.tokens[idx].jmp);
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
            vartype_t vt = shget(program.types, program.cur_vartype);
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
            vartype_t vt = shget(program.types, program.cur_vartype);
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
            vartype_t vt = shget(program.types, program.cur_vartype);
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
         case OP_OVER: {
            fprintf(output, ";   over\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rcx\n");
            fprintf(output, "    push rcx\n");
            fprintf(output, "    push rbx\n");
            fprintf(output, "    push rax\n");
            fprintf(output, "    push rcx\n");
        } break;
        case OP_DROP: {
            fprintf(output, ";   drop\n");
            fprintf(output, "    pop rax\n");
        } break;
        case OP_CAP: {
            int local = 0;
            var_t var;

            proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
            if (shgetp_null(p.vars, program.tokens[idx].val) != NULL) {
                local = 1;
                var = shget(p.vars, program.tokens[idx].val);
            }
            if (!local) {
                if (shgetp_null(program.vars, program.tokens[idx].val) != NULL) {
                    var = shget(program.vars, program.tokens[idx].val);
                }
            }
            fprintf(output, ";   cap\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    push %lu\n", var.cap);
            program.cur_var = program.prv_var;
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
            vartype_t l; // TODO: change the name to 'vt' to be consistant
            var_t var;
            proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
            if (shgetp_null(p.vars, program.tokens[idx].val) != NULL) {
                local = 1;
                var = shget(p.vars, program.tokens[idx].val);
                l = shget(program.types, var.type);
            }
            if (!local) {
                if (shgetp_null(program.vars, program.tokens[idx].val) != NULL) {
                    var = shget(program.vars, program.tokens[idx].val);
                    l = shget(program.types, var.type);
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
            } else if (program.address && !program.index && program.tokens[idx + 1].operation != OP_START_INDEX) { // get address
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
                if (!var.arr || program.tokens[idx + 1].operation == OP_START_INDEX) {
                    fprintf(output, "    push %lu\n", l.size_bytes);
                    if (program.tokens[idx + 1].operation == OP_START_INDEX) {
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
            proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
            var_t var = program.local_def ? shget(p.vars, program.cur_var) : shget(program.vars, program.cur_var);
            program.local_def = 0;
            vartype_t l = shget(program.types, var.type);
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
            if (strcmp(program.tokens[idx].val, "main") == 0) {
                fprintf(output, "    call main\n");
            } else {
                fprintf(output, "    call $PROC%lu\n", shget(program.procs, arrpop(program.cur_proc)).adr);
            }
        } break;
        case OP_CREATE_PROC: {
            program.idx++;
            int is_main = has_main_in_files && strcmp(program.tokens[idx + 1].val, "main") == 0;
            fprintf(output, ";   create proc\n");
            if (is_main) {
                fprintf(output, "global main\n");
                fprintf(output, "main:\n");
                fprintf(output, "    mov qword [$RETP], $RET\n");
            } else {
                fprintf(output, "global $PROC%lu\n", shget(program.procs, program.cur_proc[arrlen(program.cur_proc) - 1]).adr);
                fprintf(output, "$PROC%lu:\n", shget(program.procs, program.cur_proc[arrlen(program.cur_proc) - 1]).adr);
            }
            fprintf(output, "    mov rax,qword [$RETP]\n");
            fprintf(output, "    pop qword [rax]\n");
            fprintf(output, "    add qword [$RETP],8\n");
        } break;
        case OP_DO: {
            fprintf(output, ";   do\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    test rax,rax\n");
            fprintf(output, "    jz $ADR%lu\n", program.tokens[idx].jmp);
        } break;
        case OP_ELSE: {
            fprintf(output, ";   else\n");
            fprintf(output, "    jmp $ADR%lu\n", program.tokens[idx].jmp);
            fprintf(output, "$ADR%lu:\n", program.idx);
        } break;
        case OP_LOOP: {
            fprintf(output, ";   loop\n");
            fprintf(output, "$ADR%lu:\n", program.idx);
        } break;
        case OP_IMPORT: {
            program.idx++;
            fprintf(output, ";   import\n");
            size_t *export = shget(program.exports, program.tokens[program.idx].val);
            for (size_t i = 1; i < arrlenu(export); i++) {
                fprintf(output, "extern $PROC%lu\n", export[i]);
            }
        } break;
        case OP_END: {
            if (program.condition) {
                program.condition = 0;
                fprintf(output, ";   end\n");
                if (program.loop) {
                    program.loop = 0;
                    fprintf(output, "    jmp $ADR%lu\n", program.tokens[idx].jmp);
                }
                fprintf(output, "$ADR%lu:\n", program.idx);
            } else if (program.setting) {
                var_t var;
                int local = 0;
                if (program.local_def) {
                    program.local_def = 0;
                    local = 1;
                    proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
                    var = shget(p.vars, program.cur_var);
                    fprintf(output, ";   create local varible\n");
                    if (!var.arr) {
                        fprintf(output, "    add qword [$RETP],%lu\n", shget(program.types, var.type).size_bytes);
                    } else {
                        fprintf(output, "    add qword [$RETP],%lu\n", shget(program.types, var.type).size_bytes * var.cap);
                    }
                } else if (program.global_def) {
                    var = shget(program.vars, program.cur_var);
                }
                vartype_t l = shget(program.types, var.type);
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
                        fprintf(output, "    mov rdx,qword [RETP]\n");
                        fprintf(output, "    sub rdx,%lu\n", var.adr);
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
                proc_t p = shget(program.procs, program.cur_proc[arrlenu(program.cur_proc) - 1]);
                var_t var = shget(p.vars, program.cur_var);
                fprintf(output, ";   create local varible\n");
                if (!var.arr) {
                    fprintf(output, "    add qword [$RETP],%lu\n", shget(program.types, var.type).size_bytes);
                } else {
                    fprintf(output, "    add qword [$RETP],%lu\n", shget(program.types, var.type).size_bytes * var.cap);
                }
            } else if (arrlen(program.cur_proc) != 0) {
                proc_t *proc = &(shgetp_null(program.procs, arrpop(program.cur_proc))->value);
                for (size_t i = 0; i < arrlenu(proc->vars); i++) {
                    free(proc->vars[i].value.name);
                }
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
    for (size_t i = 0; i < shlen(program.vars); i++) {
        if (program.vars[i].value.constant) continue;
        vartype_t l = shget(program.types, program.vars[i].value.type);
        size_t alloc = program.vars[i].value.cap;
        if (l.primitive) {
            switch (l.size_bytes) {
            case sizeof(char):
                fprintf(output, "$VAR%lu: resb %lu\n", program.vars[i].value.adr, alloc);
                break;
            case sizeof(short):
                fprintf(output, "$VAR%lu: resw %lu\n", program.vars[i].value.adr, alloc);
                break;
            case sizeof(int):
                fprintf(output, "$VAR%lu: resd %lu\n", program.vars[i].value.adr, alloc);
                break;
            case sizeof(long):
                fprintf(output, "$VAR%lu: resq %lu\n", program.vars[i].value.adr, alloc);
                break;
            default:
                break;
            }
        }
    }
    if (program.file_num == 0) {
        fprintf(output, "global $RET, $RETP\n");
        fprintf(output, "$RET: resb %u\n", RET_STACK_CAP);
        fprintf(output, "$RETP: resq 1\n");
    } else {
        fprintf(output, "extern $RET, $RETP\n");
    }
    fprintf(output, "segment .data\n");
    for (size_t i = 0; i < shlenu(program.strs); i++) {
        str_t str = program.strs[i].value;
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
    for (size_t i = 0; i < shlen(program.vars); i++) {
        if (!program.vars[i].value.constant) continue;
        vartype_t l = shget(program.types, program.vars[i].value.type);
        if (l.primitive) {
            switch (l.size_bytes) {
            case sizeof(char):
                fprintf(output, "$VAR%lu: equ %d\n", program.vars[i].value.adr, program.vars[i].value.const_val.b8);
                break;
            case sizeof(short):
                fprintf(output, "$VAR%lu: equ %d\n", program.vars[i].value.adr, program.vars[i].value.const_val.b16);
                break;
            case sizeof(int):
                fprintf(output, "$VAR%lu: equ %u\n", program.vars[i].value.adr, program.vars[i].value.const_val.b32);
                break;
            case sizeof(long):
                fprintf(output, "$VAR%lu: equ %lu\n", program.vars[i].value.adr, program.vars[i].value.const_val.b64);
                break;
            default:
                break;
            }
        }
    }


    fclose(output);
    char *cmd = malloc(sizeof(char) * 104);
    sprintf(cmd,"nasm -felf64 -g file%lu.asm -o file%lu.o", program.file_num, program.file_num); 
    system(cmd);
    free(cmd);
}

void file_close() {
    arrfree(program.cur_proc);
    for (size_t i = 0; i < shlenu(program.types); i++) {
        free(program.types[i].value.name);
    }
    for (size_t i = 0; i < shlenu(program.vars); i++) {
        free(program.vars[i].value.name);
    }
    shfree(program.types);
    shfree(program.vars);
    shfree(program.strs);
    arrfree(program.imports);
}

void program_init() {
    program.procs = NULL;
    program.exports = NULL;
    program.tokens = NULL;
    program.positions = NULL;
    program.file_path = NULL;
}

void program_generate_obj_files(int argc, char **argv, char *std, char *file, char *link) {
    strcpy(link, "gcc -no-pie -o output");
    for (size_t i = 0; i < argc; i++) {
        file_open(i, i == 0 ? std : argv[i], arrlenu(program.tokens));
        generate_assembly_x86_64_linux();
        file_close();
        sprintf(file, " file%lu.o", i);
        strcat(link, file);
    }
}

void program_finish(char *file, char *link, char *std) {
    for (size_t i = 0; i < arrlenu(program.tokens); i++) {
        free(program.tokens[i].val);
    }
    for (size_t i = 0; i < arrlenu(program.file_path); i++) {
        free(program.file_path[i]);
    }
    for (size_t i = 0; i < shlenu(program.procs); i++) {
        for (size_t j = 0; j < shlenu(program.procs[i].value.vars); j++) {
            free(program.procs[i].value.vars[j].value.name);
        }
        shfree(program.procs[i].value.vars);
    }
    for (size_t i = 0; i < shlenu(program.exports); i++) {
        arrfree(program.exports[i].value);
    }
    arrfree(program.tokens);
    arrfree(program.positions);
    arrfree(program.file_path);
    shfree(program.procs);
    shfree(program.exports);

    if (!has_main_in_files) {
        fprintf(stderr, "ERROR: program without a main entry point\n");
        exit(1);
    }
    system(link);
    free(link);
    free(file);
    free(std);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "[ERROR] File not provided\n[INFO] ssol needs at least one file path\n");
        exit(1);
    }
    char *file = malloc(sizeof(char) * 38);
    char *file_path = malloc(strlen(argv[0]) + 1);
    char *link = malloc(sizeof(char) * (40 * (argc + 1)));
    strcpy(file_path, argv[0]);
    dirname(file_path);
    char *std_path = malloc(strlen(file_path) + 14);
    strcpy(std_path, file_path);
    strcat(std_path, "/std/std.ssol");
    free(file_path);

    program_init();
    program_generate_obj_files(argc, argv, std_path, file, link);
    program_finish(file, link, std_path);
    return 0;
}

