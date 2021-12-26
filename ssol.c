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
        TKN_INT,
        TKN_PLUS,
        TKN_MINUS,
        TKN_MUL,
        TKN_DIV,
        TKN_MOD,
        TKN_PRINT,
        TKN_EQUALS,
        TKN_IF,
        TKN_DO,
        TKN_END,
        TKN_COUNT
    } type;
    char *val;
    size_t jmp;
} token_t;

typedef struct {
    char **word_list;
    pos_t **pos_list;
    size_t word_size;
    size_t word_alloc;

    int error;
    
    token_t *cur_token;
    size_t idx;
    int condition;
} lexer_t;



char token_name[TKN_COUNT][256] = {
    "TKN_ID",
    "TKN_INT",
    "TKN_PLUS",
    "TKN_MINUS",
    "TKN_MUL",
    "TKN_DIV",
    "TKN_MOD",
   "TKN_PRINT"
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

void token_update(token_t *token, int type, char *val) {
    token->type = type;
    if (val != NULL) {
        if (type != TKN_INT) {
            fprintf(stderr, "[ERROR] %s can't have a value\n", token_name[type]);
            exit(1);
        }
        if (token->val != NULL) {
            if (strlen(val) > strlen(token->val)) {
                token->val = realloc(token->val, strlen(val));
                malloc_check(token->val, "realloc(token->val) in function token_update");
            }
        } else {
            token->val = malloc(strlen(val));
            malloc_check(token->val, "malloc(token->val) in function token_update");
        }
        strcpy(token->val, val);
    } else if (token->val != NULL) {
        free(token->val);
        token->val = NULL;
    }
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

void lexer_add_word(lexer_t *lexer, char *word, pos_t *pos) {
    lexer->word_size++;
    if (lexer->word_size >= lexer->word_alloc) {
        lexer->word_alloc *= 2;
        lexer->word_list = realloc(lexer->word_list, sizeof(char *) *lexer->word_alloc);
        lexer->pos_list = realloc(lexer->pos_list, sizeof(pos_t *) *lexer->word_alloc);
    }

    lexer->word_list[lexer->word_size - 1] = word;
    lexer->pos_list[lexer->word_size - 1]  = pos;
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

lexer_t *lexer_create(char *program, char *file) {
    lexer_t *lexer = malloc(sizeof(lexer_t));
    malloc_check(lexer, "malloc(lexer) in function lexer_create");

    lexer->cur_token = token_create();
    lexer->idx = 0;

    lexer->word_list = malloc(sizeof(char *));
    malloc_check(lexer, "malloc(lexer->word_list) in function lexer_create");
    lexer->pos_list = malloc(sizeof(pos_t *));
    malloc_check(lexer, "malloc(lexer->pos_list) in function lexer_create");
    lexer->word_size = 0;
    lexer->word_alloc = 1;

    lexer->condition = 0;
    lexer->error = 0;

    char *word = malloc(sizeof(char));
    malloc_check(lexer, "malloc(word) in function lexer_create");
    size_t word_size = 0;
    size_t word_alloc = 1;
    
    size_t line = 1;
    size_t col = 1;
    size_t col_word = 1;

    for (size_t i = 0; program[i] != 0; i++) {
        col++;
        if (program[i] == ' ' || program[i] == '\t' || program[i] == '\n') {
            if (word_size > 0) {
                word[word_size] = '\0';
                lexer_add_word(lexer, word, pos_create(file, line, col_word));
                word = malloc(sizeof(char));
                malloc_check(lexer, "malloc(word) in function lexer_create");
                word_size = 0;
                word_alloc = 1;
             }
             if (program[i] == '\n') {
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
            malloc_check(lexer, "realloc(word) in function lexer_create");
        }
        word[word_size - 1] = program[i];
    }

    return lexer;
}

void lexer_error(lexer_t *lexer, char *msg, pos_t *pos) {
    fprintf(stderr, "[ERROR] (%s:%ld:%ld) %s\n", pos->file, pos->line, pos->col, msg);
    lexer->error = 1;
}

int lexer_advance(lexer_t *lexer) {
    if (lexer->idx >= lexer->word_size) return 0;
    if (strcmp(lexer->word_list[lexer->idx], "+") == 0) {
        token_update(lexer->cur_token, TKN_PLUS, NULL);
    } else if (strcmp(lexer->word_list[lexer->idx], "-") == 0) {
        token_update(lexer->cur_token, TKN_MINUS, NULL);
    } else if (strcmp(lexer->word_list[lexer->idx], "*") == 0) {
        token_update(lexer->cur_token, TKN_MUL, NULL);
    } else if (strcmp(lexer->word_list[lexer->idx], "/") == 0) {
        token_update(lexer->cur_token, TKN_DIV, NULL);
    } else if (strcmp(lexer->word_list[lexer->idx], "%") == 0) {
        token_update(lexer->cur_token, TKN_MOD, NULL);
    } else if (strcmp(lexer->word_list[lexer->idx], "=") == 0) {
        token_update(lexer->cur_token, TKN_EQUALS, NULL);
    } else if (strcmp(lexer->word_list[lexer->idx], "print") == 0) {
        token_update(lexer->cur_token, TKN_PRINT, NULL);
    } else if (strcmp(lexer->word_list[lexer->idx], "do") == 0) {
        if (lexer->condition) {
            size_t jmp = 0;
            size_t if_count = 0;
            for (size_t i = lexer->idx + 1; i < lexer->word_size; i++) {
                if (strcmp(lexer->word_list[i], "if") == 0) if_count++;
                if (strcmp(lexer->word_list[i], "end") == 0) {
                    if (if_count > 0) {
                        if_count--;
                    } else {
                        jmp = i;
                        break;
                    }
                }
            }
            if (jmp == 0) {
                lexer_error(lexer, "if without a end", lexer->pos_list[lexer->idx]);
                return 0;
            }
            token_update(lexer->cur_token, TKN_DO, NULL);
            lexer->cur_token->jmp = jmp;
            lexer->condition = 0;
        } else {
            lexer_error(lexer, "do without a if", lexer->pos_list[lexer->idx]);
            return 0;
        }
    } else if (strcmp(lexer->word_list[lexer->idx], "if") == 0) {
        token_update(lexer->cur_token, TKN_IF, NULL);
        lexer->condition = 1;
    } else if (strcmp(lexer->word_list[lexer->idx], "end") == 0) {
        token_update(lexer->cur_token, TKN_END, NULL);
    } else if (word_is_int(lexer->word_list[lexer->idx])) {
        token_update(lexer->cur_token, TKN_INT, lexer->word_list[lexer->idx]);
    } else {
        char *msg = malloc(strlen(lexer->word_list[lexer->idx]) + 32);
        sprintf(msg, "undefined word '%s'", lexer->word_list[lexer->idx]);
        lexer_error(lexer, msg, lexer->pos_list[lexer->idx]);
        free(msg);
        return 0;
    }

//    if (lexer->cur_token->type != -1)
//        print_token(lexer->cur_token);
 
    lexer->idx++;
    return 1;
}

void parse_tokens(lexer_t *lexer) {
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
    while (lexer_advance(lexer)) {
        switch (lexer->cur_token->type) {
        case TKN_INT:
            fprintf(output, ";  push int\n");
            fprintf(output, "   push %s\n", lexer->cur_token->val);
            break;
        case TKN_PLUS:
            fprintf(output, ";  add int\n");
            fprintf(output, "   pop rax\n");
            fprintf(output, "   pop rbx\n");
            fprintf(output, "   add rbx,rax\n");
            fprintf(output, "   push rbx\n");
            break;
        case TKN_MINUS:
            fprintf(output, ";  sub int\n");
            fprintf(output, "   pop rax\n");
            fprintf(output, "   pop rbx\n");
            fprintf(output, "   sub rbx,rax\n");
            fprintf(output, "   push rbx\n");
            break;
        case TKN_MUL:
            fprintf(output, ";  mul int\n");
            fprintf(output, "   pop rbx\n");
            fprintf(output, "   pop rax\n");
            fprintf(output, "   mul rbx\n");
            fprintf(output, "   push rax\n");
            break;
        case TKN_DIV:
            fprintf(output, ";  div int\n");
            fprintf(output, "   xor rdx,rdx\n");
            fprintf(output, "   pop rbx\n");
            fprintf(output, "   pop rax\n");
            fprintf(output, "   div rbx\n");
            fprintf(output, "   push rax\n");
            break;
        case TKN_MOD:
            fprintf(output, ";  div int\n");
            fprintf(output, "   xor rdx,rdx\n");
            fprintf(output, "   pop rbx\n");
            fprintf(output, "   pop rax\n");
            fprintf(output, "   div rbx\n");
            fprintf(output, "   push rdx\n");
            break;
        case TKN_PRINT:
            fprintf(output, ";  print int\n");
            fprintf(output, "   pop rax\n");
            fprintf(output, "   call _print\n");
            break;
        case TKN_EQUALS:
            fprintf(output, ";  equals\n");
            fprintf(output, "   mov rcx,0\n");
            fprintf(output, "   mov rdx,1\n");
            fprintf(output, "   pop rax\n");
            fprintf(output, "   pop rbx\n");
            fprintf(output, "   cmp rax,rbx\n");
            fprintf(output, "   cmove rcx,rdx\n");
            fprintf(output, "   push rcx\n");
            break;
        case TKN_DO:
            fprintf(output, ";  do\n");
            fprintf(output, "   pop rax\n");
            fprintf(output, "   test rax,rax\n");
            fprintf(output, "   jz ADR%ld\n", lexer->cur_token->jmp);
            break;
        case TKN_END:
            fprintf(output, ";  end\n");
            fprintf(output, "ADR%ld:\n", lexer->idx - 1);
        default:
            break;
        }
    }
    if (lexer->error) {
        exit(1);
    }
    fprintf(output, ";  exit program\n");
    fprintf(output, "   mov rax,60\n");
    fprintf(output, "   mov rdi,0\n");
    fprintf(output, "   syscall\n");
    fclose(output);
    system("nasm -felf64 output.asm -o output.o");
    system("ld -o output output.o");
}

void lexer_destroy(lexer_t *lexer) {
    token_destroy(lexer->cur_token);
    for (size_t i = 0; i < lexer->word_size; i++) {
        pos_destroy(lexer->pos_list[i]);
        free(lexer->word_list[i]);
    }
    free (lexer->word_list);
    free (lexer->pos_list);
    free(lexer);
}

char *make_program(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "[ERROR] File not provided\n[INFO] ssol needs one parameter, the .ssol file\n");
        exit(1);
    }
    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        fprintf(stderr, "[ERROR] File '%s' dont exists\n", argv[1]);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    size_t prog_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *program = malloc(prog_size);
    malloc_check(program, "malloc(program) in function make_program");
    fread(program, 1, prog_size, f);
    fclose(f);
    return program;
}

int main(int argc, char **argv) {
    char *program = make_program(argc, argv);
    lexer_t *lexer = lexer_create(program, argv[1]);
    parse_tokens(lexer);
    lexer_destroy(lexer);
    return 0;
}

