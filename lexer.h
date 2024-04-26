#ifndef LEXER_H_SENTRY
#define LEXER_H_SENTRY

enum toktype {
    string,
    separator
};

struct token {
    enum toktype type;
    char *lexeme;
    int len, size;
};

struct lex_state {
    int flags;
    struct token cur;
    struct token *tokens;
    int len, size;
};

void init_lex_state(struct lex_state *state);

void lexer(struct lex_state *state, const char *add);

int last_tok_finished(const struct lex_state *state);

void free_lex_state(struct lex_state *state);

#endif
