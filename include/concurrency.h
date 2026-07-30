#ifndef P101_SYNC_CHECK_CONCURRENCY_H
#define P101_SYNC_CHECK_CONCURRENCY_H

#include "concurrency_types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void        p101_sync_check_model_init(const struct p101_env *env, struct p101_sync_check_model *model);
void        p101_sync_check_model_destroy(const struct p101_env *env, struct p101_sync_check_model *model);
int         p101_sync_check_ingest(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model, const struct p101_tool_event_record *record);
void        p101_sync_check_finish(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model);
#endif
