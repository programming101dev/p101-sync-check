#include "concurrency_identity.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>

bool p101_sync_check_class_is(const struct p101_env *env, const char *actual, const char *expected)
{
    return (actual != NULL && p101_strcmp(env, actual, expected) == 0) != 0;
}

void p101_sync_check_thread_from_record(const struct p101_env *env, char output[P101_SYNC_CHECK_NAME_SIZE], const struct p101_tool_event_record *record)
{
    p101_snprintf(env,
                  NULL,
                  output,
                  P101_SYNC_CHECK_NAME_SIZE,
                  "%ld:%zu:%s",
                  record->pid,
                  record->context_id,
                  record->metadata == NULL || record->metadata[0] == '\0' ? "thread=?" : record->metadata);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: bounded diagnostic identity.
}

void p101_sync_check_physical_resource(const struct p101_env *env, char output[P101_SYNC_CHECK_NAME_SIZE], const struct p101_tool_event_record *record)
{
    char        local[P101_SYNC_CHECK_NAME_SIZE];
    const char *separator;
    size_t      length;

    separator = record->resource_id == NULL ? NULL : p101_strchr(env, record->resource_id, '@');
    length    = separator == NULL ? p101_strlen(env, record->resource_id == NULL ? "?" : record->resource_id) : (size_t)(separator - record->resource_id);
    if(length >= P101_SYNC_CHECK_NAME_SIZE)
    {
        length = P101_SYNC_CHECK_NAME_SIZE - 1U;
    }
    p101_memcpy(env, local, record->resource_id == NULL ? "?" : record->resource_id, length);
    local[length] = '\0';
    p101_snprintf(env, NULL, output, P101_SYNC_CHECK_NAME_SIZE, "%ld:%zu:%s", record->pid, record->context_id, local);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: bounded diagnostic identity.
}

void p101_sync_check_related_thread(const struct p101_env *env, char output[P101_SYNC_CHECK_NAME_SIZE], const struct p101_tool_event_record *record)
{
    p101_snprintf(env,
                  NULL,
                  output,
                  P101_SYNC_CHECK_NAME_SIZE,
                  "%ld:%zu:%s",
                  record->pid,
                  record->context_id,
                  record->related_id == NULL || record->related_id[0] == '\0' ? "thread=?" : record->related_id);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: bounded diagnostic identity.
}

void p101_sync_check_copy_name(const struct p101_env *env, char output[P101_SYNC_CHECK_NAME_SIZE], const char *text)
{
    p101_snprintf(env, NULL, output, P101_SYNC_CHECK_NAME_SIZE, "%s", text == NULL ? "?" : text);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: bounded in-memory diagnostic copy.
}
