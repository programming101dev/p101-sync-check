#include "concurrency_graph.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>

static bool edge_reaches(const struct p101_env *env, const struct p101_sync_check_edge *edges, size_t edge_count, const char *from, const char *target);
static void copy_name(const struct p101_env *env, char output[P101_SYNC_CHECK_NAME_SIZE], const char *text);

void p101_sync_check_add_lock_edge(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model, const char *from, const char *to, size_t sequence)
{
    for(size_t index = 0U; index < model->lock_edge_count; index++)
    {
        if(p101_strcmp(env, model->lock_edges[index].from, from) == 0 && p101_strcmp(env, model->lock_edges[index].to, to) == 0)
        {
            return;
        }
    }
    if(edge_reaches(env, model->lock_edges, model->lock_edge_count, to, from))
    {
        p101_sync_check_add_finding(env, err, model, P101_SYNC_CHECK_LOCK_ORDER_CYCLE, sequence, from, to);
    }
    if(model->lock_edge_count >= P101_SYNC_CHECK_MAX_EDGES)
    {
        p101_sync_check_add_finding(env, err, model, P101_SYNC_CHECK_CAPACITY, sequence, from, to);
        return;
    }
    copy_name(env, model->lock_edges[model->lock_edge_count].from, from);
    copy_name(env, model->lock_edges[model->lock_edge_count].to, to);
    model->lock_edges[model->lock_edge_count].sequence = sequence;
    model->lock_edge_count++;
}

bool p101_sync_check_wait_reaches(const struct p101_env *env, const struct p101_sync_check_model *model, const char *from, const char *target, bool joins_only)
{
    bool reached[P101_SYNC_CHECK_MAX_WAITS] = {false};

    if(p101_strcmp(env, from, target) == 0)
    {
        return true;
    }
    for(size_t index = 0U; index < model->wait_count; index++)
    {
        const struct p101_sync_check_wait *wait;

        wait = &model->waits[index];
        if(!wait->active || (joins_only && !wait->join) || p101_strcmp(env, wait->thread, from) != 0)
        {
            continue;
        }
        reached[index] = true;
    }
    for(size_t pass = 0U; pass < model->wait_count; pass++)
    {
        bool changed;

        changed = false;
        for(size_t index = 0U; index < model->wait_count; index++)
        {
            const struct p101_sync_check_wait *wait;

            if(!reached[index])
            {
                continue;
            }
            wait = &model->waits[index];
            {
                size_t owner_count;

                owner_count = model->held_count;
                if(wait->join)
                {
                    owner_count = 1U;
                }
                for(size_t owner_index = 0U; owner_index < owner_count; owner_index++)
                {
                    const char *next_thread;

                    if(wait->join)
                    {
                        next_thread = wait->target;
                    }
                    else
                    {
                        const struct p101_sync_check_held *held;

                        held = &model->held[owner_index];
                        if(!held->active || p101_strcmp(env, held->resource, wait->resource) != 0)
                        {
                            continue;
                        }
                        next_thread = held->thread;
                    }
                    if(p101_strcmp(env, next_thread, target) == 0)
                    {
                        return true;
                    }
                    for(size_t next = 0U; next < model->wait_count; next++)
                    {
                        const struct p101_sync_check_wait *candidate;

                        candidate = &model->waits[next];
                        if(!reached[next] && candidate->active && (!joins_only || candidate->join) && p101_strcmp(env, candidate->thread, next_thread) == 0)
                        {
                            reached[next] = true;
                            changed       = true;
                        }
                    }
                }
            }
        }
        if(!changed)
        {
            break;
        }
    }    // GCOVR_EXCL_LINE -- the loop always returns or reaches its explicit fixed-point break.
    return false;
}

const char *p101_sync_check_owner_of(const struct p101_env *env, const struct p101_sync_check_model *model, const char *resource)
{
    for(size_t index = model->held_count; index > 0U; index--)
    {
        if(model->held[index - 1U].active && p101_strcmp(env, model->held[index - 1U].resource, resource) == 0)
        {
            return model->held[index - 1U].thread;
        }
    }
    return NULL;
}

void p101_sync_check_add_finding(const struct p101_env *env, struct p101_error *err, struct p101_sync_check_model *model, enum p101_sync_check_finding_kind kind, size_t sequence, const char *first, const char *second)
{
    struct p101_sync_check_finding *finding;

    for(size_t index = 0U; index < model->finding_count; index++)
    {
        if(model->findings[index].kind == kind && p101_strcmp(env, model->findings[index].first, first) == 0 && p101_strcmp(env, model->findings[index].second, second) == 0)
        {
            return;
        }
    }
    if(model->finding_count >= P101_SYNC_CHECK_MAX_FINDINGS)
    {
        P101_ERROR_RAISE_USER(err, "Too many concurrency findings.", 1);
        return;
    }
    finding           = &model->findings[model->finding_count++];
    finding->kind     = kind;
    finding->sequence = sequence;
    copy_name(env, finding->first, first);
    copy_name(env, finding->second, second);
}

static bool edge_reaches(const struct p101_env *env, const struct p101_sync_check_edge *edges, size_t edge_count, const char *from, const char *target)
{
    bool reached[P101_SYNC_CHECK_MAX_EDGES] = {false};

    if(p101_strcmp(env, from, target) == 0)
    {
        return true;
    }
    for(size_t index = 0U; index < edge_count; index++)
    {
        if(p101_strcmp(env, edges[index].from, from) == 0)
        {
            reached[index] = true;
        }
    }
    for(size_t pass = 0U; pass < edge_count; pass++)
    {
        bool changed;

        changed = false;
        for(size_t index = 0U; index < edge_count; index++)
        {
            if(!reached[index])
            {
                continue;
            }
            if(p101_strcmp(env, edges[index].to, target) == 0)
            {
                return true;
            }
            for(size_t next = 0U; next < edge_count; next++)
            {
                if(!reached[next] && p101_strcmp(env, edges[next].from, edges[index].to) == 0)
                {
                    reached[next] = true;
                    changed       = true;
                }
            }
        }
        if(!changed)
        {
            break;
        }
    }    // GCOVR_EXCL_LINE -- the loop always returns or reaches its explicit fixed-point break.
    return false;
}

static void copy_name(const struct p101_env *env, char output[P101_SYNC_CHECK_NAME_SIZE], const char *text)
{
    p101_snprintf(env, NULL, output, P101_SYNC_CHECK_NAME_SIZE, "%s", text == NULL ? "?" : text);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: bounded in-memory diagnostic copy.
}
