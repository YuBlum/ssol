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
        OP_PRINT,
        OP_DUP,
        OP_DROP,
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
} label_t;

typedef struct {
    token_t **token_list;
    pos_t **pos_list;
    size_t token_size;
    size_t pos_alloc;

    labeltype_t **labeltype_list;
    size_t labeltype_size;
    size_t labeltype_alloc;

    label_t **label_list;
    size_t label_size;
    size_t label_alloc;

    size_t idx;
    int error;
    int condition;
    int loop;
    int setting;
} program_t;

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

pos_t *pos_create(char *file, size_t line, size_t col) {
    pos_t *pos = malloc(sizeof(pos_t));
    malloc_check(pos, "in function pos_create");
    pos->file = malloc(strlen(file) + 1);
    strcpy(pos->file, file);
    pos->line = line;
    pos->col = col;
    return pos;
}

void pos_destroy(pos_t *pos) {
    free(pos->file);
    free(pos);
}

token_t *token_create() {
    token_t *token = malloc(sizeof(token_t));
    malloc_check(token, "in function token_create");
    token->type = -1;
    token->val = NULL;
    return token;
}

void token_update(token_t *token, int type, int operation, char *val) {
    token->type = type;
    token->operation = operation;
    if (token->val != NULL) {
        if (strlen(val) > strlen(token->val)) {
            token->val = realloc(token->val, (strlen(val) + 1) * sizeof(*token->val));
            malloc_check(token->val, "realloc(token->val) in function token_update");
        }
    } else {
        token->val = malloc((strlen(val) + 1) * sizeof(*token->val));
        malloc_check(token->val, "malloc(token->val) in function token_update");
    }
    strcpy(token->val, val);
}

void token_destroy(token_t *token) {
    free(token->val);
    free(token);
}

void print_token(token_t *token) {
    if (token->val != NULL) {
        printf("[%s, %s]\n", token_name[token->type], token->val);
    } else {
        printf("[%s]\n", token_name[token->type]);
    }
}

labeltype_t *labeltype_create(char *name, size_t size_bytes, int primitive) {
    labeltype_t *labeltype = malloc(sizeof(labeltype_t));
    malloc_check(labeltype, "in function labeltype_create");
    labeltype->name = malloc(sizeof(*labeltype->name) * (strlen(name) + 1));
    strcpy(labeltype->name, name);
    labeltype->size_bytes = size_bytes;
    labeltype->primitive = primitive;
    return labeltype;
}

void labeltype_destroy(labeltype_t *labeltype) {
    free(labeltype->name);
    free(labeltype);
}

size_t find_labeltype(program_t *program, char *name) {
    size_t find = 0;
    for (size_t i = 0; i < program->labeltype_size; i++) {
        if (strcmp(program->labeltype_list[i]->name, name) == 0) {
            find = i + 1;
            break;
        }
    }
    return find;
}

label_t *label_create(program_t *program, char *name, char *type_name) {
    size_t type;
    type = find_labeltype(program, type_name);
    if (!type) return NULL;
    type--;
    label_t *label = malloc(sizeof(label_t));
    malloc_check(label, "in function label_create");
    label->name = malloc(sizeof(*label->name) * (strlen(name) + 1));
    strcpy(label->name, name);
    label->type = type;
    return label;
}

void label_destroy(label_t *label) {
    free(label->name);
    free(label);
}

void program_add_word(program_t *program, char ***word_list, char *word, pos_t *pos) {
    program->token_size++;
    if (program->token_size >= program->pos_alloc) {
        program->pos_alloc *= 2;
        *word_list = realloc(*word_list, sizeof(char *) *program->pos_alloc);
        program->pos_list = realloc(program->pos_list, sizeof(pos_t *) *program->pos_alloc);
    }

    (*word_list)[program->token_size - 1] = word;
    program->pos_list[program->token_size - 1]  = pos;
}

void program_add_labeltype(program_t *program, labeltype_t *labeltype) {
    program->labeltype_size++;
    if (program->labeltype_size >= program->labeltype_alloc) {
        program->labeltype_alloc *= 2;
        program->labeltype_list = realloc(program->labeltype_list, sizeof(labeltype_t *) *program->labeltype_alloc);
    }
    program->labeltype_list[program->labeltype_size - 1] = labeltype;
}

void program_add_label(program_t *program, label_t *label) {
    program->label_size++;
    if (program->label_size >= program->label_alloc) {
        program->label_alloc *= 2;
        program->label_list = realloc(program->label_list, sizeof(label_t *) *program->label_alloc);
    }
    program->label_list[program->label_size - 1] = label;
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

void program_error(program_t *program, char *msg, pos_t *pos) {
    fprintf(stderr, "[ERROR] (%s:%ld:%ld) %s\n", pos->file, pos->line, pos->col, msg);
    program->error = 1;
}

// = -> rax -> label
int lex_word_as_token(program_t *program, char **word_list) {
    size_t idx = program->idx;
    if (idx >= program->token_size) return 0;
    if (strcmp(word_list[idx], "+") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_PLUS, word_list[idx]);
    } else if (strcmp(word_list[idx], "-") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_MINUS, word_list[idx]);
    } else if (strcmp(word_list[idx], "*") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_MUL, word_list[idx]);
    } else if (strcmp(word_list[idx], "/") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_DIV, word_list[idx]);
    } else if (strcmp(word_list[idx], "%") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_MOD, word_list[idx]);
    } else if (strcmp(word_list[idx], "=") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_SET_LABEL, word_list[idx]);
    } else if (strcmp(word_list[idx], "==") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_EQUALS, word_list[idx]);
    } else if (strcmp(word_list[idx], "!=") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_NOTEQUALS, word_list[idx]);
    } else if (strcmp(word_list[idx], ">") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_GREATER, word_list[idx]);
    } else if (strcmp(word_list[idx], "<") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_MINOR, word_list[idx]);
    } else if (strcmp(word_list[idx], ">=") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_EQGREATER, word_list[idx]);
    } else if (strcmp(word_list[idx], "<=") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_EQMINOR, word_list[idx]);
    } else if (strcmp(word_list[idx], "not") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_NOT, word_list[idx]);
    } else if (strcmp(word_list[idx], "print") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_PRINT, word_list[idx]);
    } else if (strcmp(word_list[idx], "dup") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_DUP, word_list[idx]);
    } else if (strcmp(word_list[idx], "drop") == 0) {
        token_update(program->token_list[idx], TKN_INTRINSIC, OP_DROP, word_list[idx]);
    } else if (strcmp(word_list[idx], "do") == 0) {
        token_update(program->token_list[idx], TKN_KEYWORD, OP_DO, word_list[idx]);
    } else if (strcmp(word_list[idx], "if") == 0) {
        token_update(program->token_list[idx], TKN_KEYWORD, OP_IF, word_list[idx]);
    } else if (strcmp(word_list[idx], "else") == 0) {
        token_update(program->token_list[idx], TKN_KEYWORD, OP_ELSE, word_list[idx]);
    } else if (strcmp(word_list[idx], "loop") == 0) {
        token_update(program->token_list[idx], TKN_KEYWORD, OP_LOOP, word_list[idx]);
    } else if (strcmp(word_list[idx], "label") == 0) {
        token_update(program->token_list[idx], TKN_KEYWORD, OP_CREATE_LABEL, word_list[idx]);
    } else if (strcmp(word_list[idx], "end") == 0) {
        token_update(program->token_list[idx], TKN_KEYWORD, OP_END, word_list[idx]);
    } else if (find_labeltype(program, word_list[idx])) {
        token_update(program->token_list[idx], TKN_TYPE, -1, word_list[idx]);
    } else if (word_is_int(word_list[idx])) {
        token_update(program->token_list[idx], TKN_INT, OP_PUSH_INT, word_list[idx]);
    } else {
        token_update(program->token_list[idx], TKN_ID, -1, word_list[idx]);
    }
//    if (program->token_list[idx]->type != -1)
//        print_token(program->token_list[idx]);
    program->idx++;
    return 1;
}

int parse_current_token(program_t *program) {
    size_t idx = program->idx;
    if (idx >= program->token_size) return 0;
    token_t **token_list = program->token_list;
    pos_t **pos_list = program->pos_list;

    switch (token_list[idx]->type) {
    case TKN_KEYWORD: {
        switch(token_list[idx]->operation) {
        case OP_DO: {
            if (program->condition) {
                size_t jmp = 0;
                size_t if_count = 0;
                for (size_t i = idx + 1; i < program->token_size; i++) {
                    if (token_list[i]->type != TKN_KEYWORD) continue;
                    if (token_list[i]->operation == OP_IF && token_list[i - 1]->operation != OP_ELSE) if_count++;
                    if (token_list[i]->operation == OP_END) {
                        if (if_count > 0) {
                            if_count--;
                        } else {
                            jmp = i;
                            break;
                        }
                    } else if (token_list[i]->operation == OP_ELSE) {
                        if (if_count == 0) {
                            jmp = i;
                            break;
                        }
                    }
                }
                if (jmp == 0) {
                    if (program->loop)
                        program_error(program, "loop without a end", pos_list[idx]);
                    else
                        program_error(program, "if without a end", pos_list[idx]);
                    return 0;
                }
                token_list[idx]->jmp = jmp;
                program->condition = 0;
                program->loop = 0;
            } else {
                program_error(program, "do without a if or loop", pos_list[idx]);
                return 0;
            }
        } break;
        case OP_IF: {
            program->condition = 1;
            if (token_list[idx]->operation == OP_DO) {
                program_error(program, "if condition can't be empty", program->pos_list[idx]);
                return 0;
            }
        } break;
        case OP_ELSE: {
            size_t jmp = 0;
            size_t if_count = 0;
            for (size_t i = idx + 1; i < program->token_size; i++) {
                if (token_list[i]->type != TKN_KEYWORD) continue;
                if ((token_list[i]->operation == OP_IF && token_list[i - 1]->operation != OP_ELSE) && i > idx + 1) if_count++;
                if (token_list[i]->operation == OP_END) {
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
                if (token_list[i]->operation == OP_END) if_count++;
                if (token_list[i]->operation == OP_IF && (i == 0 || token_list[i]->operation != OP_ELSE)) {
                    if (if_count > 0) {
                        if_count--;
                    } else {
                        found_if = 1;
                        break;
                    }
                }
            }
            if (!found_if) {
                program_error(program, "else without a if", pos_list[idx]);
                return 0;
            }
            if (jmp == 0) {
                program_error(program, "else without a end", pos_list[idx]);
                return 0;
            }
            token_list[idx]->jmp = jmp;
        } break;
        case OP_LOOP: {
            program->condition = 1;
            program->loop = 1;
            if (token_list[idx]->operation == OP_DO) {
                program_error(program, "loop condition can't be empty", pos_list[idx]);
                return 0;
            }
        } break;
        case OP_CREATE_LABEL: {
            program->idx++;
            idx = program->idx;
            if (token_list[idx]->type != TKN_ID) {
                char *msg = malloc(sizeof(char) * ((strlen(token_list[idx]->val) * 2) + strlen(token_name[token_list[idx]->type]) + 50));
                sprintf(msg, "trying to define label '%s', but '%s' is %s", token_list[idx]->val, token_list[idx]->val, token_name[token_list[idx]->type]);
                program_error(program, msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            char *name = token_list[idx]->val;
            program->idx++;
            idx = program->idx;
            if (token_list[idx]->type != TKN_TYPE) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx]->val) * 2 + 50));
                sprintf(msg, "trying to define label as type '%s', but '%s' is not a type is a %s", token_list[idx]->val, token_list[idx]->val, token_name[token_list[idx]->type]);
                program_error(program, msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            char *type = token_list[idx]->val;
            label_t *label = label_create(program, name, type);
            if (label == NULL) {
                char *msg = malloc(sizeof(char) * (strlen(token_list[idx]->val) + 25));
                sprintf(msg, "type '%s' don't exists", token_list[idx]->val);
                program_error(program, msg, pos_list[idx]);
                free(msg);
                return 0;
            }
            program_add_label(program, label);
        } break;
        case OP_END: {
            size_t end_count = 0;
            int found_open = 0;
            for (size_t i = idx - 1; i >= 0; i--) {
                if (token_list[i]->type != TKN_KEYWORD) continue;
                if (idx == 0 || (long)i < 0) break;
                if (token_list[i]->operation == OP_END) end_count++;
                if ((token_list[i]->operation == OP_IF  && token_list[i - 1]->operation != OP_ELSE) || token_list[i]->operation == OP_LOOP || token_list[i]->operation == OP_CREATE_LABEL) {
                    if (end_count > 0) {
                        end_count--;
                    } else {
                        found_open = 1;
                        program->condition = token_list[i]->operation != OP_CREATE_LABEL;
                        if (token_list[i]->operation == OP_LOOP) {
                            program->loop = 1;
                            token_list[idx]->jmp = i;
                        }
                        break;
                    }
                }
            }
            if (!found_open) {
                program_error(program, "end without a opening", pos_list[idx]);
                return 0;
            }
        } break;
        default: {
            char *msg = malloc(strlen(token_list[idx]->val) + 32);
            sprintf(msg, "undefined keyword '%s'", token_list[idx]->val);
            program_error(program, msg, program->pos_list[idx]);
            free(msg);
            return 0;
        } break;
        }
    } break;
    case TKN_INTRINSIC: {
        switch(token_list[idx]->operation) {
        case OP_SET_LABEL: {
            int find = 0;
            if (token_list[idx + 1]->type == TKN_ID) {
                // TODO: check if ID is not a label
                find = 1;
            }
            if (!find) {
                char *msg = malloc(strlen(token_list[idx + 1]->val) + 16);
                sprintf(msg, "'%s' is not a label", token_list[idx + 1]->val);
                program_error(program, msg, program->pos_list[idx]);
                free(msg);
                return 0;
            }
            program->setting = 1;
        } break;
        default:
            break;
        }
    } break;
    case TKN_ID: {
        int find = 0;
        
        // find label
        for (size_t i = 0; i < program->label_size; i++) {
            if (strcmp(program->label_list[i]->name, token_list[idx]->val) == 0) {
                token_list[idx]->operation = OP_CALL_LABEL;
                find = 1;
                break;
            }
        }

        if (!find) {
            char *msg = malloc(strlen(token_list[idx]->val) + 20);
            sprintf(msg, "undefined word '%s'", token_list[idx]->val);
            program_error(program, msg, program->pos_list[idx]);
            free(msg);
            return 0;
        }
    } break;
    default:
        break;
    }

    return 1;
}

program_t *program_create(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "[ERROR] File not provided\n[INFO] ssol needs one parameter, the .ssol file\n");
        exit(1);
    }
    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        fprintf(stderr, "[ERROR] File '%s' don't exists or can't be openned\n", argv[1]);
        exit(1);
    }

    program_t *program = malloc(sizeof(program_t));
    malloc_check(program, "malloc(program) in function program_create");

    program->idx = 0;

    char **word_list = malloc(sizeof(char *));
    malloc_check(program, "malloc(word_list) in function program_create");

    program->pos_list = malloc(sizeof(pos_t *));
    malloc_check(program, "malloc(program->pos_list) in function program_create");
    program->pos_alloc = 1;
    program->token_size = 0;

    program->labeltype_list = malloc(sizeof(labeltype_t *));
    malloc_check(program, "malloc(program->labeltype_list) in function program_create");
    program->labeltype_alloc = 1;
    program->labeltype_size = 0;

    program->label_list = malloc(sizeof(label_t *));
    malloc_check(program, "malloc(program->label_list) in function program_create");
    program->label_alloc = 1;
    program->label_size = 0;

    program->condition = 0;
    program->setting = 0;
    program->loop = 0;
    program->error = 0;

    program_add_labeltype(program, labeltype_create("byte", sizeof(char), 1));
    program_add_labeltype(program, labeltype_create("short", sizeof(short), 1));
    program_add_labeltype(program, labeltype_create("int", sizeof(int), 1));
    program_add_labeltype(program, labeltype_create("long", sizeof(long), 1));

    char *word = malloc(sizeof(char));
    malloc_check(program, "malloc(word) in function program_create");
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
        if (cur_char == ' ' || cur_char == '\t' || cur_char == '\n' || cur_char == EOF) {
            if (word_size > 0) {
                word[word_size] = '\0';
                program_add_word(program, &word_list, word, pos_create(argv[1], line, col_word));
                word = malloc(sizeof(char));
                malloc_check(program, "malloc(word) in function program_create");
                word_size = 0;
                word_alloc = 1;
             }
             if (cur_char == '\n') {
                line++;
                col = col_word = 1;
             } else {
                col_word = col;
             }
             continue;
        }
        word_size++;
        if (word_size >= word_alloc) {
            word_alloc *= 2;
            word = realloc(word, sizeof(char) *word_alloc);
            malloc_check(program, "realloc(word) in function program_create");
        }
        word[word_size - 1] = cur_char;
    }
    free(word);
    fclose(f);

    program->token_list = malloc(sizeof(token_t) * program->token_size);
    do {
        if (program->idx > 0)
            free(word_list[program->idx - 1]);

        if (program->idx < program->token_size)
            program->token_list[program->idx] = token_create();
    } while (lex_word_as_token(program, word_list));
    free(word_list);
    if (program->error) {
        exit(1);
    }

    program->idx = 0;

    return program;
}

void generate_assembly_x86_64_linux(program_t *program) {
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
        size_t idx = program->idx;
        switch (program->token_list[idx]->operation) {
        case OP_PUSH_INT: {
            fprintf(output, ";   push int\n");
            fprintf(output, "    push %s\n", program->token_list[idx]->val);
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
        case OP_DROP: {
            fprintf(output, ";   drop\n");
            fprintf(output, "    pop rax\n");
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
            labeltype_t *l = NULL;
            for (size_t i = 0; i < program->label_size; i++) {
                if (strcmp(program->token_list[idx]->val, program->label_list[i]->name) == 0) {
                    l = program->labeltype_list[program->label_list[i]->type];
                    break;
                }
            }
            // TODO: for now 'set label' and 'get label' just supports primitive types
            if (program->setting) { // set label value
                program->setting=0;
                fprintf(output, ";   set label value\n");
                fprintf(output, "    pop rax\n");
                if (l->primitive) {
                    switch (l->size_bytes) {
                    case sizeof(char):
                        fprintf(output, "    mov byte [%s],al\n", program->token_list[idx]->val);
                        break;
                    case sizeof(short):
                        fprintf(output, "    mov word [%s],ax\n", program->token_list[idx]->val);
                        break;
                    case sizeof(int):
                        fprintf(output, "    mov dword [%s],eax\n", program->token_list[idx]->val);
                        break;
                    case sizeof(long):
                        fprintf(output, "    mov qword [%s],rax\n", program->token_list[idx]->val);
                        break;
                    }
                }
            } else { // get label value
                fprintf(output, ";   get label value\n");
                if (l->primitive) {
                    fprintf(output, "    xor rax,rax\n");
                    switch (l->size_bytes) {
                    case sizeof(char):
                        fprintf(output, "    mov al,byte [%s]\n", program->token_list[idx]->val);
                        break;
                    case sizeof(short):
                        fprintf(output, "    mov ax,word [%s]\n", program->token_list[idx]->val);
                        break;
                    case sizeof(int):
                        fprintf(output, "    mov eax,dword [%s]\n", program->token_list[idx]->val);
                        break;
                    case sizeof(long):
                        fprintf(output, "    mov rax,qword [%s]\n", program->token_list[idx]->val);
                        break;
                    }
                    fprintf(output, "    push rax\n");
                }
            }
        } break;
        case OP_DO: {
            fprintf(output, ";   do\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    test rax,rax\n");
            fprintf(output, "    jz ADR%ld\n", program->token_list[idx]->jmp);
        } break;
        case OP_ELSE: {
            fprintf(output, ";   else\n");
            fprintf(output, "    jmp ADR%ld\n", program->token_list[idx]->jmp);
            fprintf(output, "ADR%ld:\n", program->idx);
        } break;
        case OP_LOOP: {
            fprintf(output, ";   loop\n");
            fprintf(output, "ADR%ld:\n", program->idx);
        } break;
        case OP_END: {
            if (program->condition) {
                program->condition = 0;
                fprintf(output, ";   end\n");
                if (program->loop) {
                    program->loop = 0;
                    fprintf(output, "    jmp ADR%ld\n", program->token_list[idx]->jmp);
                }
                fprintf(output, "ADR%ld:\n", program->idx);
            }
        } break;
        default:
            break;
        }
        program->idx++;
    }
    if (program->error) {
        exit(1);
    }
    fprintf(output, ";  exit program_code\n");
    fprintf(output, "   mov rax,60\n");
    fprintf(output, "   mov rdi,0\n");
    fprintf(output, "   syscall\n");
    fprintf(output, "segment .bss\n");
    for (size_t i = 0; i < program->label_size; i++) {
        labeltype_t *lt = program->labeltype_list[program->label_list[i]->type];
        if (lt->primitive) {
            switch (lt->size_bytes) {
            case sizeof(char):
                fprintf(output, "%s: resb 1\n", program->label_list[i]->name);
                break;
            case sizeof(short):
                fprintf(output, "%s: resw 1\n", program->label_list[i]->name);
                break;
            case sizeof(int):
                fprintf(output, "%s: resd 1\n", program->label_list[i]->name);
                break;
            case sizeof(long):
                fprintf(output, "%s: resq 1\n", program->label_list[i]->name);
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

void program_destroy(program_t *program) {
    for (size_t i = 0; i < program->token_size; i++) {
        pos_destroy(program->pos_list[i]);
        token_destroy(program->token_list[i]);
    }
    for (size_t i = 0; i < program->labeltype_size; i++) {
        labeltype_destroy(program->labeltype_list[i]);
    }
    for (size_t i = 0; i < program->label_size; i++) {
        label_destroy(program->label_list[i]);
    }
    free(program->pos_list);
    free(program->token_list);
    free(program->labeltype_list);
    free(program->label_list);
    free(program);
}

int main(int argc, char **argv) {
    program_t *program = program_create(argc, argv);
    generate_assembly_x86_64_linux(program);
    program_destroy(program);
    return 0;
}

