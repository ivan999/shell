#include "cmdshell.h"
#include "dynarr.h"
#include "separators.h"

#include <stdarg.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define SIZEADD_PIDS 8
#define SIZEADD_STR 64

#define STDIN 0
#define STDOUT 1

static const char env_home[] = "HOME";
static const char cd_word[] = "cd";

static const char start_mess[] = "shell";
static const char fault_mess[] = "fault";
static const char info_mess[] = "info";

static const char unexp_token[] = "unexpected token";
static const char redir_stds[] = "redirection";
static const char open_file[] = "open file";
static const char exec_proc[] = "execute program";
static const char cd_cmd[] = "change directory";

static const char started_proc[] = "started";
static const char finished_proc[] = "finished";

static const char str_format[] = "\"%s\"";
static const char int_format[] = "[%d]";

#define MESS_FAULT 1
#define MESS_INFO 2

static void print_message(
    int flag, const char *str, const char *format, ...)
{
    va_list vl;
    FILE *stream = stderr;
    const char *mess = fault_mess;
    if(flag & MESS_INFO) {
        mess = info_mess;
        stream = stdout;
    }
    va_start(vl, format);
    fprintf(stream, "%s: %s: %s: ", start_mess, mess, str);
    vfprintf(stream, format, vl);
    fputc('\n', stream);
    va_end(vl);
}

struct step_toks {
    int start, end;
    const char *last_sep, *sep;
};

static void init_step_toks(struct step_toks *step)
{
    step->end = -1;
    step->start = -1;
    step->sep = "";
    step->last_sep = "";
}

static void print_token_fault(const char *err, const struct step_toks *step)
{
    if(*err == 0)
        ;
    else if(!step->sep &&
        strcmp(step->last_sep, separators[backgrnd]) == 0)
        ;
    else
        print_message(MESS_FAULT, unexp_token, str_format, err);
}

static int do_step_toks(struct step_toks *step, const struct lex_state *state)
{
    const char *err;
    if(!step->sep)
        return 0;
    if(*step->sep)
        step->last_sep = step->sep;
    (step->end)++;
    step->start = step->end;
    for( ; ; ) {
        const struct token *tok;
        if(step->end == state->len) {
            step->sep = NULL;
            err = step->last_sep;
            break;
        }
        tok = state->tokens + step->end;
        step->sep = tok->lexeme;
        if(tok->type == separator) {
            err = step->sep;
            break;
        }
        (step->end)++;
    }
    if(step->start == step->end) {
        print_token_fault(err, step);
        return 0;
    }
    return 1;
}

struct open_params {
    int who, mode;
};

#define WRITE_MODE O_WRONLY|O_CREAT

static struct open_params *get_open_params(const char *sep)
{
    static struct open_params res;
    res.who = STDOUT;
    res.mode = WRITE_MODE;
    if(strcmp(sep, separators[rdr_read]) == 0) {
        res.who = STDIN;
        res.mode = O_RDONLY;
    } else if(strcmp(sep, separators[rdr_write]) == 0)
        res.mode |= O_TRUNC;
    else if(strcmp(sep, separators[rdr_append]) == 0)
        res.mode |= O_APPEND;
    else
        return NULL;
    return &res;
}

struct command {
    int pgid;
    int fdin, fdout;
    int *pids;
    int len, size;
};

static void init_command(struct command *cmd)
{
    cmd->pgid = 0;
    cmd->fdin = STDIN;
    cmd->fdout = STDOUT;
    INIT_DYNARR(cmd->pids, cmd->len, cmd->size);
}

#define OPEN_PERMS 0666

static int redir_command(char *const *words,
    struct command *cmd, const struct open_params *op)
{
    int *fd = NULL;
    if(words[1]) {
        print_message(MESS_FAULT, unexp_token, str_format, words[1]);
        return 0;
    }
    if(op->who == STDIN)
        fd = &cmd->fdin;
    else if(op->who == STDOUT)
        fd = &cmd->fdout;
    if(*fd != STDOUT && *fd != STDIN) {
        print_message(MESS_FAULT, redir_stds, str_format, words[0]);
        return 0;
    }
    *fd = open(words[0], op->mode, OPEN_PERMS);
    if(*fd == -1) {
        print_message(MESS_FAULT, open_file, str_format, words[0]);
        return 0;
    }
    return 1;
}

#define RESET_FD(FD, STD) \
    if(FD != STD) { \
        close(FD); \
        FD = STD; \
    }

static void reset_fds(struct command *cmd)
{
    RESET_FD(cmd->fdin, STDIN);
    RESET_FD(cmd->fdout, STDOUT);
}

static int change_dir(char *const *argv)
{
    int res;
    const char *path = argv[1];
    if(!path)
        path = getenv(env_home);
    res = chdir(path);
    if(res == -1) {
        print_message(MESS_FAULT, cd_cmd, str_format, path);
        return 0;
    }
    return 1;
}

static void add_to_pids(struct command *cmd, int add)
{
    SIZEMOD_DYNARR(cmd->pids, int, cmd->len, cmd->size, SIZEADD_PIDS);
    (cmd->pids)[cmd->len] = add;
    (cmd->len)++;
}

static int do_exec(char *const *argv, struct command *cmd)
{
    int pid;
    fflush(stderr);
    pid = fork();
    if(pid == -1) {
        perror("fork");
        return 0;
    }
    if(pid == 0) {
        pid = getpid();
        if(cmd->pgid == 0)
            setpgid(pid, pid);
        else
            setpgid(pid, cmd->pgid);
        dup2(cmd->fdin, STDIN);
        dup2(cmd->fdout, STDOUT);
        reset_fds(cmd);
        execvp(*argv, argv);
        print_message(MESS_FAULT, exec_proc, str_format, *argv);
        fflush(stderr);
        _exit(1);
    }
    if(cmd->pgid == 0) {
        cmd->pgid = pid;
        tcsetpgrp(STDIN, pid);
    }
    add_to_pids(cmd, pid);
    return 1;
}

static int exec_command(char *const *argv, struct command *cmd)
{
    int res;
    if(strcmp(*argv, cd_word) == 0)
        res = change_dir(argv);
    else
        res = do_exec(argv, cmd); 
    reset_fds(cmd);
    return res;
}

static int exec_conveyor(char *const *argv, struct command *cmd)
{
    int res, fds[2];
    if(cmd->fdout != STDOUT) {
        print_message(MESS_FAULT, redir_stds, str_format, *argv);
        return 0;
    }
    pipe(fds);
    cmd->fdout = fds[1];
    res = exec_command(argv, cmd);
    cmd->fdin = fds[0];
    return res;
}

static void reset_pgid(struct command *cmd)
{
    static struct sigaction sigact;
    sigaction(SIGTTOU, NULL, &sigact);
    sigact.sa_handler = SIG_IGN;
    sigaction(SIGTTOU, &sigact, NULL);
    tcsetpgrp(STDIN, getpgid(0));
    cmd->pgid = 0;
    sigact.sa_handler = SIG_DFL;
    sigaction(SIGTTOU, &sigact, NULL);
}

static int exec_backgrnd(char *const *argv, struct command *cmd)
{
    int i, res = exec_command(argv, cmd);
    for(i = 0; i < cmd->len; i++) 
        print_message(MESS_INFO, started_proc, int_format, (cmd->pids)[i]);
    free(cmd->pids);
    INIT_DYNARR(cmd->pids, cmd->len, cmd->size);
    reset_pgid(cmd);
    return res;
}

static char **copy_lex_toks(const struct lex_state *state)
{
    int i;
    char **res = malloc(sizeof(char**) * (state->len+1));
    for(i = 0; i < state->len; i++) {
        const struct token *tok = state->tokens + i;
        res[i] = tok->lexeme;
    }
    res[i] = NULL;
    return res;
}

static void finish_command(struct command *cmd)
{
    int i;
    reset_fds(cmd);
    for(i = 0; i < cmd->len; i++)
        wait4((cmd->pids)[i], NULL, 0, NULL);
    reset_pgid(cmd);
    free(cmd->pids);
}

static void clear_zombies()
{
    int res;
    for( ; ; ) {
        res = wait4(-1, NULL, WNOHANG, NULL);
        if(res <= 0)
            break;
        print_message(MESS_INFO, finished_proc, int_format, res);
    }
}

struct lex_state *read_tokens(FILE *stream)
{
    int len, size;
    char *str, *ptr;
    static struct lex_state res;
    init_lex_state(&res);
    INIT_DYNARR(str, len, size);
    for( ; ; ) {
        RESIZE_DYNARR(str, char, size, SIZEADD_STR);
        ptr = fgets(str+len, SIZEADD_STR, stream);
        if(!ptr) {
            free(str);
            free_lex_state(&res);
            return NULL;
        }
        len += strlen(ptr);
        lexer(&res, ptr);
        if(str[len-1] == '\n' && last_tok_finished(&res))
            break;
    }
    free(str);
    return &res;
}

void cmdshell(const struct lex_state *state)
{
    int res = 1, argv = 0;
    static struct command cmd;
    static struct step_toks step;
    char **toks = copy_lex_toks(state);
    clear_zombies();
    init_command(&cmd);
    init_step_toks(&step);
    while(do_step_toks(&step, state)) {
        struct open_params *op = get_open_params(step.last_sep);
        toks[step.end] = NULL;
        if(op)
            res = redir_command(toks+step.start, &cmd, op);
        if(!res)
            break;
        if(!step.sep)
            res = exec_command(toks+argv, &cmd);
        else if(strcmp(step.sep, separators[backgrnd]) == 0)
            res = exec_backgrnd(toks+argv, &cmd);
        else if(strcmp(step.sep, separators[conveyor]) == 0)
            res = exec_conveyor(toks+argv, &cmd);
        else
            continue;
        if(!res)
            break;
        argv = step.end + 1;
    }
    free(toks);
    finish_command(&cmd);
}
