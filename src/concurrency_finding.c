#include "concurrency_finding.h"

const char *p101_sync_check_finding_id(enum p101_sync_check_finding_kind kind)
{
    static const char *const ids[] = {"P101-SYNC-001", "P101-SYNC-002", "P101-SYNC-003", "P101-SYNC-900", "P101-SYNC-901"};

    return (size_t)kind < sizeof(ids) / sizeof(ids[0]) ? ids[kind] : "P101-SYNC-000";
}

const char *p101_sync_check_finding_message(enum p101_sync_check_finding_kind kind)
{
    static const char *const messages[] = {"lock-order graph contains a cycle", "live wait-for graph contains a deadlock cycle", "thread join graph contains a cycle", "event stream is incomplete", "analysis capacity was exceeded"};

    return (size_t)kind < sizeof(messages) / sizeof(messages[0]) ? messages[kind] : "unknown synchronization finding";
}
