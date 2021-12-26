#ifndef SSOL_H_
#define SSOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TOKENS 
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

token_t *token_create();
void token_update(token_t *token, int type, char *val);
void token_destroy(token_t *token);

// LEXER
typedef struct {
    char **word_list;
    size_t word_size;
    size_t word_alloc;

    token_t *cur_token;
    size_t idx;
    int condition;
} lexer_t;

lexer_t *lexer_create(char *program);
void lexer_destroy(lexer_t *lexer);

#endif // SSOL_H_
