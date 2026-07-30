#include "concurrency.h"
#include "unity.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_tool_event/event.h>

static struct p101_error *error;
static struct p101_env   *env;

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

    record.version     = P101_TOOL_EVENT_LOG_VERSION;
    record.record_kind = P101_TOOL_EVENT_RECORD_COMPLETE;
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lock_order_cycle);
    RUN_TEST(test_live_wait_cycle);
    RUN_TEST(test_join_cycle);
    RUN_TEST(test_processes_do_not_share_lock_identity);
    RUN_TEST(test_rwlock_order_cycle);
    return UNITY_END();
}
