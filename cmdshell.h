#ifndef CMDSHELL_H_SENTRY
#define CMDSHELL_H_SENTRY

#include "lexer.h"
#include <stdio.h>

struct lex_state *read_tokens(FILE *stream);

void cmdshell(const struct lex_state *state);

#endif
