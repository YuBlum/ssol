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
        //OP_REPEAT,
        //OP_BREAK,
        OP_END,
        OP_COUNT
    } operation;
    char *val;
    size_t jmp;
} token_t;

typedef struct {
    token_t **token_list;
    pos_t **pos_list;
    size_t token_size;
    size_t pos_alloc;

    size_t idx;
    int error;
    int condition;
    int loop;
} program_t;

char token_name[TKN_COUNT][256] = {
    "TKN_ID",
    "TKN_KEYWORD",
    "TKN_INTRINSIC",
    "TKN_INT",
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

void lexer_add_word(program_t *program, char ***word_list, char *word, pos_t *pos) {
    program->token_size++;
    if (program->token_size >= program->pos_alloc) {
        program->pos_alloc *= 2;
        *word_list = realloc(*word_list, sizeof(char *) *program->pos_alloc);
        program->pos_list = realloc(program->pos_list, sizeof(pos_t *) *program->pos_alloc);
    }

    (*word_list)[program->token_size - 1] = word;
    program->pos_list[program->token_size - 1]  = pos;
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
    } else if (strcmp(word_list[idx], "end") == 0) {
        token_update(program->token_list[idx], TKN_KEYWORD, OP_END, word_list[idx]);
    } else if (word_is_int(word_list[idx])) {
        token_update(program->token_list[idx], TKN_INT, OP_PUSH_INT, word_list[idx]);
    } else {
        char *msg = malloc(strlen(word_list[idx]) + 32);
        sprintf(msg, "undefined word '%s'", word_list[idx]);
        program_error(program, msg, program->pos_list[idx]);
        free(msg);
        return 0;
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

    if (token_list[idx]->type == TKN_KEYWORD) {
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
        case OP_END: {
            size_t end_count = 0;
            int found_open = 0;
            for (size_t i = idx - 1; i >= 0; i--) {
                if (token_list[i]->type != TKN_KEYWORD) continue;
                if (idx == 0 || (long)i < 0) break;
                if (token_list[i]->operation == OP_END) end_count++;
                if ((token_list[i]->operation == OP_IF  && token_list[i - 1]->operation != OP_ELSE) || token_list[i]->operation == OP_LOOP) {
                    if (end_count > 0) {
                        end_count--;
                    } else {
                        found_open = 1;
                        if (token_list[i]->operation == OP_LOOP) {
                            program->loop = 1;
                            token_list[idx]->jmp = i;
                        }
                        break;
                    }
                }
            }
            if (!found_open) {
                program_error(program, "end without a if or a loop", pos_list[idx]);
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
    malloc_check(program, "malloc(program) in function lexer_create");

    program->idx = 0;

    char **word_list = malloc(sizeof(char *));
    malloc_check(program, "malloc(word_list) in function lexer_create");

    program->pos_list = malloc(sizeof(pos_t *));
    program->pos_alloc = 1;
    program->token_size = 0;
    malloc_check(program, "malloc(program->pos_list) in function lexer_create");

    program->condition = 0;
    program->loop = 0;
    program->error = 0;

    char *word = malloc(sizeof(char));
    malloc_check(program, "malloc(word) in function lexer_create");
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
                lexer_add_word(program, &word_list, word, pos_create(argv[1], line, col_word));
                word = malloc(sizeof(char));
                malloc_check(program, "malloc(word) in function lexer_create");
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
            malloc_check(program, "realloc(word) in function lexer_create");
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
        case OP_PUSH_INT:
            fprintf(output, ";   push int\n");
            fprintf(output, "    push %s\n", program->token_list[idx]->val);
            break;
        case OP_PLUS:
            fprintf(output, ";   add int\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    add rbx,rax\n");
            fprintf(output, "    push rbx\n");
            break;
        case OP_MINUS:
            fprintf(output, ";   sub int\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    sub rbx,rax\n");
            fprintf(output, "    push rbx\n");
            break;
        case OP_MUL:
            fprintf(output, ";   mul int\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    mul rbx\n");
            fprintf(output, "    push rax\n");
            break;
        case OP_DIV:
            fprintf(output, ";   div int\n");
            fprintf(output, "    xor rdx,rdx\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    div rbx\n");
            fprintf(output, "    push rax\n");
            break;
        case OP_MOD:
            fprintf(output, ";   div int\n");
            fprintf(output, "    xor rdx,rdx\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    div rbx\n");
            fprintf(output, "    push rdx\n");
            break;
        case OP_PRINT:
            fprintf(output, ";   print int\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    call _print\n");
            break;
        case OP_DUP:
            fprintf(output, ";   dup\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    push rax\n");
            fprintf(output, "    push rax\n");
            break;
        case OP_DROP:
            fprintf(output, ";   drop\n");
            fprintf(output, "    pop rax\n");
            break;
        case OP_EQUALS:
            fprintf(output, ";   equals\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmove rcx,rdx\n");
            fprintf(output, "    push rcx\n");
            break;
        case OP_GREATER:
            fprintf(output, ";   greater\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmovg rcx,rdx\n");
            fprintf(output, "    push rcx\n");
            break;
        case OP_MINOR:
            fprintf(output, ";   minor\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmovl rcx,rdx\n");
            fprintf(output, "    push rcx\n");
            break;
        case OP_EQGREATER:
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
            break;
        case OP_EQMINOR:
            fprintf(output, ";   eqminor\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmovle rcx,rdx\n");
            fprintf(output, "    push rcx\n");
            break;
        case OP_NOTEQUALS:
            fprintf(output, ";   not\n");
            fprintf(output, "    mov rcx,0\n");
            fprintf(output, "    mov rdx,1\n");
            fprintf(output, "    pop rbx\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    cmp rax,rbx\n");
            fprintf(output, "    cmovne rcx,rdx\n");
            fprintf(output, "    push rcx\n");
            break;
        case OP_DO:
            fprintf(output, ";   do\n");
            fprintf(output, "    pop rax\n");
            fprintf(output, "    test rax,rax\n");
            fprintf(output, "    jz ADR%ld\n", program->token_list[idx]->jmp);
            break;
        case OP_ELSE:
            fprintf(output, ";   else\n");
            fprintf(output, "    jmp ADR%ld\n", program->token_list[idx]->jmp);
            fprintf(output, "ADR%ld:\n", program->idx);
            break;
        case OP_LOOP:
            fprintf(output, ";   loop\n");
            fprintf(output, "ADR%ld:\n", program->idx);
            break;
        case OP_END:
            fprintf(output, ";   end\n");
            if (program->loop) {
                program->loop = 0;
                fprintf(output, "    jmp ADR%ld\n", program->token_list[idx]->jmp);
            }
            fprintf(output, "ADR%ld:\n", program->idx);
            break;
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
    fclose(output);
    system("nasm -felf64 output.asm -o output.o");
    system("ld -o output output.o");
}

void program_destroy(program_t *program) {
    for (size_t i = 0; i < program->token_size; i++) {
        pos_destroy(program->pos_list[i]);
        token_destroy(program->token_list[i]);
    }
    free (program->pos_list);
    free (program->token_list);
    free(program);
}

int main(int argc, char **argv) {
    program_t *program = program_create(argc, argv);
    generate_assembly_x86_64_linux(program);
    program_destroy(program);
    return 0;
}

