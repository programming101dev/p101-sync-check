#ifndef P101_SYNC_CHECK_GRAPH_H
#define P101_SYNC_CHECK_GRAPH_H

#include "concurrency_types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void        p101_sync_check_add_lock_edge(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model, const char *from, const char *to, size_t sequence);
bool        p101_sync_check_wait_reaches(const struct p101_env *env, const struct p101_sync_check_model *model, const char *from, const char *target, bool joins_only);
const char *p101_sync_check_owner_of(const struct p101_env *env, const struct p101_sync_check_model *model, const char *resource);
void        p101_sync_check_add_finding(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model, enum p101_sync_check_finding_kind kind, size_t sequence, const char *first, const char *second);

#endif
