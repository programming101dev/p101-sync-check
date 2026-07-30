#ifndef P101_SYNC_CHECK_IDENTITY_H
#define P101_SYNC_CHECK_IDENTITY_H

#include "concurrency_types.h"
#include <p101_env/env.h>
#include <p101_tool_event/event.h>
#include <stdbool.h>

bool p101_sync_check_class_is(const struct p101_env *env, const char *actual, const char *expected);
void p101_sync_check_thread_from_record(const struct p101_env *env, char output[P101_SYNC_CHECK_NAME_SIZE], const struct p101_tool_event_record *record);
void p101_sync_check_physical_resource(const struct p101_env *env, char output[P101_SYNC_CHECK_NAME_SIZE], const struct p101_tool_event_record *record);
void p101_sync_check_related_thread(const struct p101_env *env, char output[P101_SYNC_CHECK_NAME_SIZE], const struct p101_tool_event_record *record);
void p101_sync_check_copy_name(const struct p101_env *env, char output[P101_SYNC_CHECK_NAME_SIZE], const char *text);

#endif    // P101_SYNC_CHECK_IDENTITY_H
