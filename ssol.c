#include "ssol.h"

static char token_name[TKN_COUNT][256] = {
    "TKN_ID",
    "TKN_INT",
    "TKN_PLUS",
    "TKN_PRINT"
};

token_t *token_create() {
    token_t *token = malloc(sizeof(token_t));
    token->type = -1;
    token->val = NULL;
    return token;
}

void token_update(token_t *token, int type, char *val) {
    token->type = type;
    if (val != NULL) {
        if (type != TKN_INT) {
            printf("ERROR: %s can't have a value.\n", token_name[type]);
        }
        if (token->val != NULL) {
            if (strlen(val) > strlen(token->val))
                token->val = realloc(token->val, strlen(val));
        } else {
            token->val = malloc(strlen(val));
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

static void print_token(token_t *token) {
    if (token->val != NULL) {
        printf("[%s, %s]\n", token_name[token->type], token->val);
    } else {
        printf("[%s]\n", token_name[token->type]);
    }
}

static void lexer_add_word(lexer_t *lexer, char *word) {
    lexer->word_size++;
    if (lexer->word_size >= lexer->word_alloc) {
        lexer->word_alloc *= 2;
        lexer->word_list = realloc(lexer->word_list, sizeof(char *) *lexer->word_alloc);
    }

    lexer->word_list[lexer->word_size - 1] = word;
}

static int word_is_int(char *word) {
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

lexer_t *lexer_create(char *program) {
    lexer_t *lexer = malloc(sizeof(lexer_t));

    lexer->cur_token = token_create();
    lexer->idx = 0;

    lexer->word_list = malloc(sizeof(char *));
    lexer->word_size = 0;
    lexer->word_alloc = 1;

    char *word = malloc(sizeof(char));
    size_t word_size = 0;
    size_t word_alloc = 1;

    for (size_t i = 0; program[i] != 0; i++) {
        if (program[i] == ' ' || program[i] == '\t' || program[i] == '\n') {

            if (word_size > 0) {
                word[word_size + 1] = '\0';
                lexer_add_word(lexer, word);
                word = malloc(sizeof(char));
                word_size = 0;
                word_alloc = 1;
             }
             continue;
        }
         word_size++;
        if (word_size >= word_alloc) {
            word_alloc *= 2;
            word = realloc(word, sizeof(char) *word_alloc);
        }
        word[word_size - 1] = program[i];
    }

    return lexer;
}

int lexer_advance(lexer_t *lexer) {
    if (lexer->idx >= lexer->word_size) return 0;
    if (strcmp(lexer->word_list[lexer->idx], "+") == 0) {
        token_update(lexer->cur_token, TKN_PLUS, NULL);
    } else if (strcmp(lexer->word_list[lexer->idx], "print") == 0) {
        token_update(lexer->cur_token, TKN_PRINT, NULL);
    } else if (word_is_int(lexer->word_list[lexer->idx])) {
        token_update(lexer->cur_token, TKN_INT, lexer->word_list[lexer->idx]);
    } else {
        token_update(lexer->cur_token, -1, NULL);
    }

    if (lexer->cur_token->type != -1)
        print_token(lexer->cur_token);
 
    lexer->idx++;
    return 1;
}

void parse_tokens(lexer_t *lexer) {
    FILE *output = fopen("output.asm", "w");
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
        case TKN_PRINT:
            fprintf(output, ";  print int\n");
            fprintf(output, "   pop rax\n");
            fprintf(output, "   call _print\n");
        default:
            break;
        }
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
        free(lexer->word_list[i]);
    }
    free (lexer->word_list);
    free(lexer);
}

int main(int argc, char **argv) {
    lexer_t *lexer = lexer_create(
            "10 print\n"
            "20 print\n"
            "10 20 + print\n"
        );
    parse_tokens(lexer);
    lexer_destroy(lexer);
    return 0;
}

