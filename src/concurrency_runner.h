#ifndef P101_SYNC_CHECK_RUNNER_H
#define P101_SYNC_CHECK_RUNNER_H

#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stdio.h>

enum
{
    P101_SYNC_CHECK_EXIT_CLEAN    = 0,
    P101_SYNC_CHECK_EXIT_FINDINGS = 1,
    P101_SYNC_CHECK_EXIT_TROUBLE  = 2
};

int p101_sync_check_analyze(const struct p101_env *env, struct p101_error *err, FILE *stream, bool json);

#endif
