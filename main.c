#include "cmdshell.h"

int main()
{
    struct lex_state *state;
    for( ; ; ) {
        fputs("> ", stdout);
        state = read_tokens(stdin);
        if(!state)
            break;
        cmdshell(state);
        free_lex_state(state);
    }
    puts("exit");
    return 0;
}
