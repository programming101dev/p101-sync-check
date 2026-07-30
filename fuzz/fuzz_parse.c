#include "concurrency.h"
#include <p101_c/p101_string.h>
#include <p101_tool_event/event.h>
#include <stddef.h>
#include <stdint.h>

#define FUZZ_LINE_MAX P101_TOOL_EVENT_LINE_MAX_BYTES

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    struct p101_error             *err;
    struct p101_env               *env;
    struct p101_sync_check_model  model;
    size_t                         start;

    err = p101_error_create(false);
    if(err == NULL)
    {
        return 0;
    }
    env = p101_env_create(err, NULL);
    if(env == NULL)
    {
        p101_error_destroy(err);
        return 0;
    }

    p101_sync_check_model_init(env, &model);
    start = 0U;
    while(start < size && p101_error_has_no_error(err))
    {
        struct p101_tool_event_record record;
        char                          line[FUZZ_LINE_MAX];
        size_t                        end;
        size_t                        length;

        end = start;
        while(end < size && data[end] != '\n')
        {
            end++;
        }
        length = end - start;
        if(length >= sizeof(line))
        {
            length = sizeof(line) - 1U;
        }
        p101_memcpy(env, line, data + start, length);
        line[length] = '\0';
        if(p101_tool_event_parse_line(line, &record) == P101_TOOL_EVENT_PARSE_OK)
        {
            (void)p101_sync_check_ingest(env, err, &model, &record);
        }
        start = end < size ? end + 1U : size;
    }
    if(p101_error_has_no_error(err))
    {
        p101_sync_check_finish(env, err, &model);
    }
    p101_sync_check_model_destroy(env, &model);
    p101_env_destroy(env);
    p101_error_destroy(err);
    return 0;
}
