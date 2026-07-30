#include "concurrency.h"
#include "concurrency_graph.h"
#include "concurrency_identity.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdbool.h>

static void acquire_held(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model, const struct p101_tool_event_record *record);
static void release_held(const struct p101_env *env, struct p101_sync_check_model *model, const struct p101_tool_event_record *record);
static void acquire_wait(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model, const struct p101_tool_event_record *record, bool join);
static void release_wait(const struct p101_env *env, struct p101_sync_check_model *model, const struct p101_tool_event_record *record);

void p101_sync_check_model_init(const struct p101_env *env, struct p101_sync_check_model *model)
{
    if(model != NULL)
    {
        p101_memset(env, model, 0, sizeof(*model));
    }
}

void p101_sync_check_model_destroy(const struct p101_env *env, struct p101_sync_check_model *model)
{
    if(model != NULL)
    {
        p101_tool_event_stream_health_destroy(&model->health);
        p101_memset(env, model, 0, sizeof(*model));
    }
}

int p101_sync_check_ingest(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model, const struct p101_tool_event_record *record)
{
    P101_TRACE_SCOPE(env);
    if(model == NULL || record == NULL)
    {
        P101_ERROR_RAISE_CHECK(err);
        return -1;
    }
    if(p101_tool_event_stream_health_observe(&model->health, record) != 0)
    {
        // Allocation-failure behavior belongs to lib_tool_event's tested
        // stream-health contract; that library exposes no failure injector.
        P101_ERROR_RAISE_ERRNO(err, errno);    // GCOVR_EXCL_LINE
        return -1;                             // GCOVR_EXCL_LINE
    }
    if(record->record_kind != P101_TOOL_EVENT_RECORD_RESOURCE || record->resource_class == NULL)
    {
        return 0;
    }

    if(p101_sync_check_class_is(env, record->resource_class, "pthread-mutex-held") || p101_sync_check_class_is(env, record->resource_class, "pthread-rwlock-held"))
    {
        if(record->resource_kind == P101_TOOL_EVENT_RESOURCE_ACQUIRE)
        {
            acquire_held(env, err, model, record);
        }
        else if(record->resource_kind == P101_TOOL_EVENT_RESOURCE_RELEASE)
        {
            release_held(env, model, record);
        }
    }
    else if(p101_sync_check_class_is(env, record->resource_class, "pthread-mutex-wait") || p101_sync_check_class_is(env, record->resource_class, "pthread-rwlock-read-wait") ||
            p101_sync_check_class_is(env, record->resource_class, "pthread-rwlock-write-wait") || p101_sync_check_class_is(env, record->resource_class, "pthread-condition-wait"))
    {
        if(record->resource_kind == P101_TOOL_EVENT_RESOURCE_ACQUIRE)
        {
            acquire_wait(env, err, model, record, false);
        }
        else if(record->resource_kind == P101_TOOL_EVENT_RESOURCE_RELEASE)
        {
            release_wait(env, model, record);
        }
    }
    else if(p101_sync_check_class_is(env, record->resource_class, "pthread-join-wait"))
    {
        if(record->resource_kind == P101_TOOL_EVENT_RESOURCE_ACQUIRE)
        {
            acquire_wait(env, err, model, record, true);
        }
        else if(record->resource_kind == P101_TOOL_EVENT_RESOURCE_RELEASE)
        {
            release_wait(env, model, record);
        }
    }
    if(p101_error_has_error(err))
    {
        return -1;
    }
    return 0;
}

void p101_sync_check_finish(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model)
{
    P101_TRACE_SCOPE(env);
    if(model == NULL)
    {
        P101_ERROR_RAISE_CHECK(err);
        return;
    }

    for(size_t index = 0U; index < model->wait_count && p101_error_has_no_error(err); index++)
    {
        const struct p101_sync_check_wait *wait;

        wait = &model->waits[index];
        if(!wait->active)
        {
            continue;
        }
        if(wait->join)
        {
            if(p101_sync_check_wait_reaches(env, model, wait->target, wait->thread, true))
            {
                p101_sync_check_add_finding(env, err, model, P101_SYNC_CHECK_JOIN_CYCLE, wait->sequence, wait->thread, wait->target);
            }
            continue;
        }
        for(size_t owner_index = 0U; owner_index < model->held_count; owner_index++)
        {
            const struct p101_sync_check_held *held;

            held = &model->held[owner_index];
            if(held->active && p101_strcmp(env, held->resource, wait->resource) == 0 && p101_sync_check_wait_reaches(env, model, held->thread, wait->thread, false))
            {
                p101_sync_check_add_finding(env, err, model, P101_SYNC_CHECK_WAIT_CYCLE, wait->sequence, wait->thread, held->thread);
            }
        }
    }
    if(!p101_tool_event_stream_health_is_complete(&model->health))
    {
        p101_sync_check_add_finding(env, err, model, P101_SYNC_CHECK_INCOMPLETE_STREAM, 0U, "event-stream", "missing or invalid producer completion");
    }
}    // sync-health-finalization-branch: LLVM maps the tested completeness predicate here.

static void acquire_held(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model, const struct p101_tool_event_record *record)
{
    char resource[P101_SYNC_CHECK_NAME_SIZE];
    char thread[P101_SYNC_CHECK_NAME_SIZE];

    p101_sync_check_thread_from_record(env, thread, record);
    p101_sync_check_physical_resource(env, resource, record);
    for(size_t index = 0U; index < model->held_count; index++)
    {
        if(model->held[index].active && p101_strcmp(env, model->held[index].thread, thread) == 0 && p101_strcmp(env, model->held[index].resource, resource) != 0)
        {
            p101_sync_check_add_lock_edge(env, err, model, model->held[index].resource, resource, record->sequence);
        }
    }
    if(model->held_count >= P101_SYNC_CHECK_MAX_HELD)
    {
        p101_sync_check_add_finding(env, err, model, P101_SYNC_CHECK_CAPACITY, record->sequence, thread, resource);
        return;
    }
    model->held[model->held_count].active   = true;
    model->held[model->held_count].sequence = record->sequence;
    p101_sync_check_copy_name(env, model->held[model->held_count].thread, thread);
    p101_sync_check_copy_name(env, model->held[model->held_count].resource, resource);
    model->held_count++;
}

static void release_held(const struct p101_env *env, struct p101_sync_check_model *model, const struct p101_tool_event_record *record)
{
    char resource[P101_SYNC_CHECK_NAME_SIZE];
    char thread[P101_SYNC_CHECK_NAME_SIZE];

    p101_sync_check_thread_from_record(env, thread, record);
    p101_sync_check_physical_resource(env, resource, record);
    for(size_t index = model->held_count; index > 0U; index--)
    {
        struct p101_sync_check_held *held;

        held = &model->held[index - 1U];
        if(held->active && p101_strcmp(env, held->thread, thread) == 0 && p101_strcmp(env, held->resource, resource) == 0)
        {
            held->active = false;
            return;
        }
    }
}

static void acquire_wait(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model, const struct p101_tool_event_record *record, bool join)
{
    struct p101_sync_check_wait *wait;
    char                         thread[P101_SYNC_CHECK_NAME_SIZE];

    p101_sync_check_thread_from_record(env, thread, record);
    if(model->wait_count >= P101_SYNC_CHECK_MAX_WAITS)
    {
        p101_sync_check_add_finding(env, err, model, P101_SYNC_CHECK_CAPACITY, record->sequence, thread, record->resource_id);
        return;
    }
    wait           = &model->waits[model->wait_count++];
    wait->active   = true;
    wait->join     = join;
    wait->sequence = record->sequence;
    p101_sync_check_copy_name(env, wait->thread, thread);
    if(join)
    {
        char resource[P101_SYNC_CHECK_NAME_SIZE];

        p101_snprintf(env, NULL, resource, sizeof(resource), "%ld:%zu:%s", record->pid, record->context_id, record->resource_id == NULL ? "thread=?" : record->resource_id);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: bounded diagnostic identity.
        p101_sync_check_copy_name(env, wait->resource, resource);
        p101_sync_check_related_thread(env, wait->target, record);
    }
    else
    {
        p101_sync_check_physical_resource(env, wait->resource, record);
        wait->target[0] = '\0';
    }
}

static void release_wait(const struct p101_env *env, struct p101_sync_check_model *model, const struct p101_tool_event_record *record)
{
    char resource[P101_SYNC_CHECK_NAME_SIZE];
    char thread[P101_SYNC_CHECK_NAME_SIZE];

    p101_sync_check_thread_from_record(env, thread, record);
    if(p101_sync_check_class_is(env, record->resource_class, "pthread-join-wait"))
    {
        p101_snprintf(env, NULL, resource, sizeof(resource), "%ld:%zu:%s", record->pid, record->context_id, record->resource_id == NULL ? "thread=?" : record->resource_id);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: bounded diagnostic identity.
    }
    else
    {
        p101_sync_check_physical_resource(env, resource, record);
    }
    for(size_t index = model->wait_count; index > 0U; index--)
    {
        struct p101_sync_check_wait *wait;

        wait = &model->waits[index - 1U];
        if(wait->active && p101_strcmp(env, wait->thread, thread) == 0 && p101_strcmp(env, wait->resource, resource) == 0)
        {
            wait->active = false;
            return;
        }
    }
}
