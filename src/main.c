#include "concurrency.h"
#include "concurrency_runner.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_stdio.h>
#include <stdbool.h>

static void usage(const struct p101_env *env, struct p101_error *err, const char *program);

int main(int argc, char *argv[])
{
    struct p101_error *err;
    struct p101_env   *env;
    FILE              *stream;
    bool               json;
    int                ret_val;
    int                index;

    ret_val = P101_SYNC_CHECK_EXIT_TROUBLE;
    err     = p101_error_create(false);
    env     = p101_env_create(err, NULL);
    stream  = stdin;
    json    = false;
    index   = 1;
    while(index < argc && argv[index][0] == '-')
    {
        if(p101_strcmp(env, argv[index], "-j") == 0)
        {
            json = true;
        }
        else if(p101_strcmp(env, argv[index], "-h") == 0 || p101_strcmp(env, argv[index], "--help") == 0)
        {
            usage(env, err, argv[0]);
            ret_val = P101_SYNC_CHECK_EXIT_CLEAN;
            goto done;
        }
        else if(p101_strcmp(env, argv[index], "--") == 0)
        {
            index++;
            break;
        }
        else
        {
            usage(env, err, argv[0]);
            goto done;
        }
        index++;
    }
    if(index < argc)
    {
        stream = p101_fopen(env, err, argv[index++], "r");
    }
    if(index != argc || stream == NULL || p101_error_has_error(err))
    {
        goto done;
    }

    ret_val = p101_sync_check_analyze(env, err, stream, json);

done:
    if(stream != NULL && stream != stdin)
    {
        p101_fclose(env, err, stream);
    }
    if(p101_error_has_error(err))
    {
        p101_fprintf(env, NULL, stderr, "p101-sync-check: %s\n", p101_error_get_message(err));    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: final diagnostic cannot replace the primary error.
        ret_val = P101_SYNC_CHECK_EXIT_TROUBLE;
    }
    p101_env_destroy(env);
    p101_error_destroy(err);
    return ret_val;
}

static void usage(const struct p101_env *env, struct p101_error *err, const char *program)
{
    p101_fprintf(env, err, stderr, "Usage: %s [-j] [resource.log]\n", program);
    p101_fputs(env, err, "Analyze p101 pthread wait, ownership, lock-order, and join events. Reads stdin when no file is supplied.\n", stderr);
}
