#include "concurrency.h"
#include "concurrency_finding.h"
#include "concurrency_graph.h"
#include "concurrency_identity.h"
#include "concurrency_runner.h"
#include "unity.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_tool_event/event.h>
#include <stdio.h>

void p101_sync_check_test_json_string(const struct p101_env *env, struct p101_error *err, const char *text);

static struct p101_error *error;
static struct p101_env   *env;

static int fail_calloc(const struct p101_env *candidate_env, const char *call_name, void *user_data)
{
    (void)user_data;
    return p101_strcmp(candidate_env, call_name, "calloc") == 0 ? ENOMEM : 0;
}

void setUp(void)
{
    error = p101_error_create(false);
    env   = p101_env_create(error, NULL);
}

void tearDown(void)
{
    p101_env_destroy(env);
    p101_error_destroy(error);
}

static void resource_for_pid(struct p101_sync_check_model *model, long pid, size_t sequence, p101_tool_event_resource_kind kind, const char *class_name, const char *id, const char *related, const char *thread)
{
    struct p101_tool_event_record record = {0};

    record.version        = P101_TOOL_EVENT_LOG_VERSION;
    record.run_id         = "sync-test";
    record.record_kind    = P101_TOOL_EVENT_RECORD_RESOURCE;
    record.pid            = pid;
    record.context_id     = 1U;
    record.sequence       = sequence;
    record.resource_kind  = kind;
    record.resource_class = (char *)class_name;
    record.resource_id    = (char *)id;
    record.related_id     = (char *)related;
    record.metadata       = (char *)thread;
    TEST_ASSERT_EQUAL_INT(0, p101_sync_check_ingest(env, error, model, &record));
}

static void resource(struct p101_sync_check_model *model, size_t sequence, p101_tool_event_resource_kind kind, const char *class_name, const char *id, const char *related, const char *thread)
{
    resource_for_pid(model, 1, sequence, kind, class_name, id, related, thread);
}

static void complete_for_pid(struct p101_sync_check_model *model, long pid, size_t sequence)
{
    struct p101_tool_event_record record = {0};

    record.version          = P101_TOOL_EVENT_LOG_VERSION;
    record.run_id           = "sync-test";
    record.record_kind      = P101_TOOL_EVENT_RECORD_COMPLETE;
    record.pid              = pid;
    record.context_id       = 1U;
    record.sequence         = sequence;
    record.events_attempted = sequence - 1U;
    TEST_ASSERT_EQUAL_INT(0, p101_sync_check_ingest(env, error, model, &record));
}

static void complete(struct p101_sync_check_model *model, size_t sequence)
{
    complete_for_pid(model, 1, sequence);
}

static void test_lock_order_cycle(void)
{
    struct p101_sync_check_model model;

    p101_sync_check_model_init(env, &model);
    resource(&model, 1U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "A@thread=one", NULL, "thread=one");
    resource(&model, 2U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "B@thread=one", NULL, "thread=one");
    resource(&model, 3U, P101_TOOL_EVENT_RESOURCE_RELEASE, "pthread-mutex-held", "B@thread=one", NULL, "thread=one");
    resource(&model, 4U, P101_TOOL_EVENT_RESOURCE_RELEASE, "pthread-mutex-held", "A@thread=one", NULL, "thread=one");
    resource(&model, 5U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "B@thread=two", NULL, "thread=two");
    resource(&model, 6U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "A@thread=two", NULL, "thread=two");
    complete(&model, 7U);
    p101_sync_check_finish(env, error, &model);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_size_t(1U, model.finding_count);
    TEST_ASSERT_EQUAL_INT(P101_SYNC_CHECK_LOCK_ORDER_CYCLE, model.findings[0].kind);
    p101_sync_check_model_destroy(env, &model);
}

static void test_live_wait_cycle(void)
{
    struct p101_sync_check_model model;

    p101_sync_check_model_init(env, &model);
    resource(&model, 1U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "A@thread=one", NULL, "thread=one");
    resource(&model, 2U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "B@thread=two", NULL, "thread=two");
    resource(&model, 3U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-wait", "B@thread=one", NULL, "thread=one");
    resource(&model, 4U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-wait", "A@thread=two", NULL, "thread=two");
    complete(&model, 5U);
    p101_sync_check_finish(env, error, &model);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(1U, model.finding_count);
    TEST_ASSERT_EQUAL_INT(P101_SYNC_CHECK_WAIT_CYCLE, model.findings[0].kind);
    p101_sync_check_model_destroy(env, &model);
}

static void test_join_cycle(void)
{
    struct p101_sync_check_model model;

    p101_sync_check_model_init(env, &model);
    resource(&model, 1U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-join-wait", "thread=one", "thread=two", "thread=one");
    resource(&model, 2U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-join-wait", "thread=two", "thread=one", "thread=two");
    complete(&model, 3U);
    p101_sync_check_finish(env, error, &model);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(1U, model.finding_count);
    TEST_ASSERT_EQUAL_INT(P101_SYNC_CHECK_JOIN_CYCLE, model.findings[0].kind);
    p101_sync_check_model_destroy(env, &model);
}

static void test_processes_do_not_share_lock_identity(void)
{
    struct p101_sync_check_model model;

    p101_sync_check_model_init(env, &model);
    resource_for_pid(&model, 1, 1U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "A@thread=one", NULL, "thread=one");
    resource_for_pid(&model, 1, 2U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "B@thread=one", NULL, "thread=one");
    complete_for_pid(&model, 1, 3U);
    resource_for_pid(&model, 2, 1U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "B@thread=one", NULL, "thread=one");
    resource_for_pid(&model, 2, 2U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "A@thread=one", NULL, "thread=one");
    complete_for_pid(&model, 2, 3U);
    p101_sync_check_finish(env, error, &model);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_size_t(0U, model.finding_count);
    p101_sync_check_model_destroy(env, &model);
}

static void test_rwlock_order_cycle(void)
{
    struct p101_sync_check_model model;

    p101_sync_check_model_init(env, &model);
    resource(&model, 1U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-rwlock-held", "A@thread=one", NULL, "thread=one");
    resource(&model, 2U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-rwlock-held", "B@thread=one", NULL, "thread=one");
    resource(&model, 3U, P101_TOOL_EVENT_RESOURCE_RELEASE, "pthread-rwlock-held", "B@thread=one", NULL, "thread=one");
    resource(&model, 4U, P101_TOOL_EVENT_RESOURCE_RELEASE, "pthread-rwlock-held", "A@thread=one", NULL, "thread=one");
    resource(&model, 5U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-rwlock-held", "B@thread=two", NULL, "thread=two");
    resource(&model, 6U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-rwlock-held", "A@thread=two", NULL, "thread=two");
    complete(&model, 7U);
    p101_sync_check_finish(env, error, &model);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_size_t(1U, model.finding_count);
    TEST_ASSERT_EQUAL_INT(P101_SYNC_CHECK_LOCK_ORDER_CYCLE, model.findings[0].kind);
    p101_sync_check_model_destroy(env, &model);
}

static void test_contracts_identity_and_release_paths(void)
{
    struct p101_sync_check_model  model;
    struct p101_tool_event_record record = {0};
    char                          name[P101_SYNC_CHECK_NAME_SIZE];
    char                          long_id[P101_SYNC_CHECK_NAME_SIZE + 32U];

    p101_sync_check_model_init(env, NULL);
    p101_sync_check_model_destroy(env, NULL);
    p101_sync_check_model_init(env, &model);
    TEST_ASSERT_EQUAL_INT(-1, p101_sync_check_ingest(env, error, NULL, &record));
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);
    TEST_ASSERT_EQUAL_INT(-1, p101_sync_check_ingest(env, error, &model, NULL));
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);

    record.version     = P101_TOOL_EVENT_LOG_VERSION;
    record.run_id      = "sync-test";
    record.pid         = 1;
    record.context_id  = 1U;
    record.sequence    = 1U;
    record.record_kind = P101_TOOL_EVENT_RECORD_CALL;
    TEST_ASSERT_EQUAL_INT(0, p101_sync_check_ingest(env, error, &model, &record));
    record.record_kind    = P101_TOOL_EVENT_RECORD_RESOURCE;
    record.resource_class = NULL;
    TEST_ASSERT_EQUAL_INT(0, p101_sync_check_ingest(env, error, &model, &record));
    record.resource_class = "unknown";
    TEST_ASSERT_EQUAL_INT(0, p101_sync_check_ingest(env, error, &model, &record));

    resource(&model, 1U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-condition-wait", "condition@thread=one", NULL, "thread=one");
    resource(&model, 2U, P101_TOOL_EVENT_RESOURCE_RELEASE, "pthread-condition-wait", "condition@thread=one", NULL, "thread=one");
    resource(&model, 3U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-join-wait", "thread=two", NULL, "thread=one");
    resource(&model, 4U, P101_TOOL_EVENT_RESOURCE_RELEASE, "pthread-join-wait", "thread=two", NULL, "thread=one");
    resource(&model, 5U, P101_TOOL_EVENT_RESOURCE_RELEASE, "pthread-join-wait", "missing", NULL, "thread=one");
    TEST_ASSERT_FALSE(model.waits[0].active);
    TEST_ASSERT_FALSE(model.waits[1].active);
    complete(&model, 6U);
    p101_sync_check_finish(env, error, &model);

    TEST_ASSERT_FALSE(p101_sync_check_class_is(env, NULL, "x"));
    p101_sync_check_copy_name(env, name, NULL);
    TEST_ASSERT_EQUAL_STRING("?", name);
    p101_sync_check_thread_from_record(env, name, &record);
    p101_sync_check_related_thread(env, name, &record);
    record.resource_id = NULL;
    p101_sync_check_physical_resource(env, name, &record);
    p101_memset(env, long_id, 'x', sizeof(long_id));
    long_id[sizeof(long_id) - 1U] = '\0';
    record.resource_id            = long_id;
    p101_sync_check_physical_resource(env, name, &record);

    TEST_ASSERT_EQUAL_STRING("P101-SYNC-000", p101_sync_check_finding_id((enum p101_sync_check_finding_kind)999));
    TEST_ASSERT_EQUAL_STRING("P101-SYNC-001", p101_sync_check_finding_id(P101_SYNC_CHECK_LOCK_ORDER_CYCLE));
    TEST_ASSERT_EQUAL_STRING("P101-SYNC-002", p101_sync_check_finding_id(P101_SYNC_CHECK_WAIT_CYCLE));
    TEST_ASSERT_EQUAL_STRING("P101-SYNC-003", p101_sync_check_finding_id(P101_SYNC_CHECK_JOIN_CYCLE));
    TEST_ASSERT_EQUAL_STRING("P101-SYNC-900", p101_sync_check_finding_id(P101_SYNC_CHECK_INCOMPLETE_STREAM));
    TEST_ASSERT_EQUAL_STRING("P101-SYNC-901", p101_sync_check_finding_id(P101_SYNC_CHECK_CAPACITY));
    TEST_ASSERT_EQUAL_STRING("unknown synchronization finding", p101_sync_check_finding_message((enum p101_sync_check_finding_kind)999));
    p101_sync_check_model_destroy(env, &model);
}

static void test_capacity_and_graph_helpers(void)
{
    struct p101_sync_check_model model;

    p101_sync_check_model_init(env, &model);
    model.held_count = P101_SYNC_CHECK_MAX_HELD;
    resource(&model, 1U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-held", "A", NULL, "thread=one");
    TEST_ASSERT_EQUAL_INT(P101_SYNC_CHECK_CAPACITY, model.findings[0].kind);
    p101_sync_check_model_destroy(env, &model);

    p101_sync_check_model_init(env, &model);
    model.wait_count = P101_SYNC_CHECK_MAX_WAITS;
    resource(&model, 1U, P101_TOOL_EVENT_RESOURCE_ACQUIRE, "pthread-mutex-wait", "A", NULL, "thread=one");
    TEST_ASSERT_EQUAL_INT(P101_SYNC_CHECK_CAPACITY, model.findings[0].kind);
    p101_sync_check_model_destroy(env, &model);

    p101_sync_check_model_init(env, &model);
    p101_sync_check_add_lock_edge(env, error, &model, "A", "B", 1U);
    p101_sync_check_add_lock_edge(env, error, &model, "A", "B", 2U);
    p101_sync_check_add_lock_edge(env, error, &model, "B", "C", 3U);
    p101_sync_check_add_lock_edge(env, error, &model, "C", "A", 4U);
    p101_sync_check_add_lock_edge(env, error, &model, "self", "self", 5U);
    TEST_ASSERT_EQUAL_size_t(4U, model.lock_edge_count);
    TEST_ASSERT_TRUE(p101_sync_check_wait_reaches(env, &model, "same", "same", false));
    TEST_ASSERT_NULL(p101_sync_check_owner_of(env, &model, "A"));
    TEST_ASSERT_FALSE(p101_sync_check_wait_reaches(env, &model, "one", "two", false));

    model.lock_edge_count = P101_SYNC_CHECK_MAX_EDGES;
    p101_sync_check_add_lock_edge(env, error, &model, "new", "edge", 3U);
    TEST_ASSERT_EQUAL_INT(P101_SYNC_CHECK_CAPACITY, model.findings[model.finding_count - 1U].kind);
    p101_sync_check_model_destroy(env, &model);

    p101_sync_check_model_init(env, &model);
    p101_sync_check_add_finding(env, error, &model, P101_SYNC_CHECK_WAIT_CYCLE, 1U, "a", "b");
    p101_sync_check_add_finding(env, error, &model, P101_SYNC_CHECK_WAIT_CYCLE, 2U, "a", "b");
    TEST_ASSERT_EQUAL_size_t(1U, model.finding_count);
    model.finding_count = P101_SYNC_CHECK_MAX_FINDINGS;
    p101_sync_check_add_finding(env, error, &model, P101_SYNC_CHECK_CAPACITY, 3U, "x", "y");
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);
    p101_sync_check_model_destroy(env, &model);

    p101_sync_check_model_init(env, &model);
    model.held_count      = 2U;
    model.held[0].active  = true;
    model.held[1].active  = true;
    model.wait_count      = 2U;
    model.waits[0].active = true;
    model.waits[1].active = true;
    p101_sync_check_copy_name(env, model.held[0].thread, "two");
    p101_sync_check_copy_name(env, model.held[0].resource, "R1");
    p101_sync_check_copy_name(env, model.held[1].thread, "three");
    p101_sync_check_copy_name(env, model.held[1].resource, "R2");
    p101_sync_check_copy_name(env, model.waits[0].thread, "one");
    p101_sync_check_copy_name(env, model.waits[0].resource, "R1");
    p101_sync_check_copy_name(env, model.waits[1].thread, "two");
    p101_sync_check_copy_name(env, model.waits[1].resource, "R2");
    TEST_ASSERT_TRUE(p101_sync_check_wait_reaches(env, &model, "one", "three", false));
    TEST_ASSERT_EQUAL_STRING("two", p101_sync_check_owner_of(env, &model, "R1"));
    p101_sync_check_add_finding(env, error, &model, P101_SYNC_CHECK_WAIT_CYCLE, 6U, NULL, NULL);
    p101_sync_check_model_destroy(env, &model);

    p101_sync_check_model_init(env, &model);
    model.wait_count      = 1U;
    model.waits[0].active = true;
    p101_sync_check_copy_name(env, model.waits[0].thread, "alone");
    p101_sync_check_copy_name(env, model.waits[0].resource, "unowned");
    TEST_ASSERT_FALSE(p101_sync_check_wait_reaches(env, &model, "alone", "other", false));
    p101_sync_check_model_destroy(env, &model);
}

static void test_propagated_and_allocation_failures(void)
{
    struct p101_sync_check_model  model;
    struct p101_tool_event_record record = {0};
    FILE                         *stream;

    p101_sync_check_model_init(env, &model);
    model.held_count      = P101_SYNC_CHECK_MAX_HELD;
    model.finding_count   = P101_SYNC_CHECK_MAX_FINDINGS;
    record.version        = P101_TOOL_EVENT_LOG_VERSION;
    record.run_id         = "sync-test";
    record.record_kind    = P101_TOOL_EVENT_RECORD_RESOURCE;
    record.pid            = 1;
    record.context_id     = 1U;
    record.sequence       = 1U;
    record.resource_kind  = P101_TOOL_EVENT_RESOURCE_ACQUIRE;
    record.resource_class = "pthread-mutex-held";
    record.resource_id    = "A";
    record.metadata       = "thread=one";
    TEST_ASSERT_EQUAL_INT(-1, p101_sync_check_ingest(env, error, &model, &record));
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);
    p101_sync_check_model_destroy(env, &model);

    stream = p101_tmpfile(env, error);
    TEST_ASSERT_NOT_NULL(stream);
    p101_env_set_fault_injector(env, fail_calloc, NULL);
    TEST_ASSERT_EQUAL_INT(P101_SYNC_CHECK_EXIT_TROUBLE, p101_sync_check_analyze(env, error, stream, false));
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_env_set_fault_injector(env, NULL, NULL);
    p101_error_reset(error);
    TEST_ASSERT_EQUAL_INT(0, p101_fclose(env, error, stream));
}

static void test_finish_contract_and_json_escaping(void)
{
    struct p101_sync_check_model model;
    static const char            special[] = {'"', '\\', '\b', '\f', '\n', '\r', '\t', 1, 'x', '\0'};

    p101_sync_check_finish(env, error, NULL);
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);
    p101_sync_check_model_init(env, &model);
    p101_sync_check_finish(env, error, &model);
    TEST_ASSERT_EQUAL_INT(P101_SYNC_CHECK_INCOMPLETE_STREAM, model.findings[0].kind);
    p101_sync_check_test_json_string(env, error, NULL);
    p101_sync_check_test_json_string(env, error, special);
    p101_sync_check_model_destroy(env, &model);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lock_order_cycle);
    RUN_TEST(test_live_wait_cycle);
    RUN_TEST(test_join_cycle);
    RUN_TEST(test_processes_do_not_share_lock_identity);
    RUN_TEST(test_rwlock_order_cycle);
    RUN_TEST(test_contracts_identity_and_release_paths);
    RUN_TEST(test_capacity_and_graph_helpers);
    RUN_TEST(test_propagated_and_allocation_failures);
    RUN_TEST(test_finish_contract_and_json_escaping);
    return UNITY_END();
}
