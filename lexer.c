#include "lexer.h"
#include "dynarr.h"
#include "separators.h"

#include <ctype.h>
#include <string.h>

#define SIZEADD_TOKEN 16
#define SIZEADD_LEXSTATE 16

#define QUOTE_CHAR '"'
#define ESCAPING_CHAR '\\'

static void init_token(struct token *tok)
{
    INIT_DYNARR(tok->lexeme, tok->len, tok->size);
    tok->type = string;
}

static void add_char_token(struct token *tok, char c)
{
    (tok->len)++;
    SIZEMOD_DYNARR(tok->lexeme, char, 
        tok->len, tok->size, SIZEADD_TOKEN);
    (tok->lexeme)[tok->len-1] = c;
    (tok->lexeme)[tok->len] = 0;
}

static void add_cur_lexstate(struct lex_state *state)
{
    SIZEMOD_DYNARR(state->tokens, struct token, 
        state->len, state->size, SIZEADD_LEXSTATE);
    *(state->tokens + state->len) = state->cur;
    (state->len)++;
}

static int is_in_separators(const char *str, int len)
{
    const char *const *tmp = separators;
    while(*tmp) {
        if(strncmp(str, *tmp, len) == 0)
            return 1;
        tmp++;
    } 
    return 0;
}

#define BUFSIZE_SEPADD 10

static int check_sep_add(const struct token *tok, char c)
{
    static char buf[BUFSIZE_SEPADD];
    strncpy(buf, tok->lexeme, tok->len);
    buf[tok->len] = c;
    return is_in_separators(buf, tok->len+1);
}

#define IN_QUOTES 1
#define IN_ESCAPING 2

#define ADD_IN_QUOTES(CUR, STATE, C) \
    if(C == QUOTE_CHAR) \
        (STATE)->flags &= ~IN_QUOTES; \
    else \
        add_char_token(CUR, C)

#define ADD_QUOTE(CUR, STATE) \
    add_char_token(CUR, 0); \
    ((CUR)->len)--; \
    (STATE)->flags |= IN_QUOTES

#define ADD_WHITESPACE(CUR, STATE) \
    if((CUR)->lexeme) { \
        add_cur_lexstate(STATE); \
        init_token(CUR); \
    }

#define ADD_SEPARATOR(CUR, STATE, C) \
    ADD_WHITESPACE(CUR, STATE); \
    add_char_token(CUR, C); \
    (CUR)->type = separator

void init_lex_state(struct lex_state *state)
{
    state->flags = 0;
    init_token(&state->cur);
    INIT_DYNARR(state->tokens, state->len, state->size);
}

void lexer(struct lex_state *state, const char *add)
{
    struct token *cur = &state->cur;
    while(*add) {
        switch(state->cur.type) {
        case string:
            if(state->flags & IN_ESCAPING) {
                add_char_token(cur, *add);
                state->flags &= ~IN_ESCAPING;
            } else if(*add == ESCAPING_CHAR) {
                state->flags |= IN_ESCAPING;
            } else if(state->flags & IN_QUOTES) {
                ADD_IN_QUOTES(cur, state, *add);
            } else if(*add == QUOTE_CHAR) {
                ADD_QUOTE(cur, state);
            } else if(isspace(*add)) {
                ADD_WHITESPACE(cur, state);
            } else if(is_in_separators(add, 1)) {
                ADD_SEPARATOR(cur, state, *add);
            } else 
                add_char_token(cur, *add);
            add++;
            break;
        case separator:
            if(check_sep_add(cur, *add)) {
                add_char_token(cur, *add);
                add++;
            } else {
                add_cur_lexstate(state);
                init_token(cur);
            }
        }
    }
}

int last_tok_finished(const struct lex_state *state)
{
    return !state->cur.lexeme;
}

void free_lex_state(struct lex_state *state)
{
    int i;
    for(i = 0; i < state->len; i++) {
        struct token *tok = state->tokens + i;
        free(tok->lexeme);
    }
    free(state->cur.lexeme);
    free(state->tokens);
}
