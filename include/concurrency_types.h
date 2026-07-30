#ifndef P101_SYNC_CHECK_TYPES_H
#define P101_SYNC_CHECK_TYPES_H

#include <p101_tool_event/event.h>
#include <stdbool.h>
#include <stddef.h>

enum
{
    P101_SYNC_CHECK_NAME_SIZE    = 160,
    P101_SYNC_CHECK_MAX_HELD     = 4096,
    P101_SYNC_CHECK_MAX_WAITS    = 4096,
    P101_SYNC_CHECK_MAX_EDGES    = 8192,
    P101_SYNC_CHECK_MAX_FINDINGS = 256
};

enum p101_sync_check_finding_kind
{
    P101_SYNC_CHECK_LOCK_ORDER_CYCLE = 0,
    P101_SYNC_CHECK_WAIT_CYCLE,
    P101_SYNC_CHECK_JOIN_CYCLE,
    P101_SYNC_CHECK_INCOMPLETE_STREAM,
    P101_SYNC_CHECK_CAPACITY
};

struct p101_sync_check_finding
{
    enum p101_sync_check_finding_kind kind;
    size_t                             sequence;
    char                               first[P101_SYNC_CHECK_NAME_SIZE];
    char                               second[P101_SYNC_CHECK_NAME_SIZE];
};

struct p101_sync_check_held
{
    bool   active;
    char   thread[P101_SYNC_CHECK_NAME_SIZE];
    char   resource[P101_SYNC_CHECK_NAME_SIZE];
    size_t sequence;
};

struct p101_sync_check_wait
{
    bool   active;
    bool   join;
    char   thread[P101_SYNC_CHECK_NAME_SIZE];
    char   resource[P101_SYNC_CHECK_NAME_SIZE];
    char   target[P101_SYNC_CHECK_NAME_SIZE];
    size_t sequence;
};

struct p101_sync_check_edge
{
    char   from[P101_SYNC_CHECK_NAME_SIZE];
    char   to[P101_SYNC_CHECK_NAME_SIZE];
    size_t sequence;
};

struct p101_sync_check_model
{
    struct p101_sync_check_held         held[P101_SYNC_CHECK_MAX_HELD];
    struct p101_sync_check_wait         waits[P101_SYNC_CHECK_MAX_WAITS];
    struct p101_sync_check_edge         lock_edges[P101_SYNC_CHECK_MAX_EDGES];
    struct p101_sync_check_finding      findings[P101_SYNC_CHECK_MAX_FINDINGS];
    size_t                               held_count;
    size_t                               wait_count;
    size_t                               lock_edge_count;
    size_t                               finding_count;
    size_t                               malformed;
    size_t                               unsupported;
    struct p101_tool_event_stream_health health;
};

#endif
