#include "concurrency_runner.h"
#include "concurrency.h"
#include "concurrency_finding.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>

enum
{
    JSON_CONTROL_LIMIT = 32
};

static void report_text(const struct p101_env *env, struct p101_error *err, const struct p101_sync_check_model *model);
static void report_json(const struct p101_env *env, struct p101_error *err, const struct p101_sync_check_model *model);
static void json_string(const struct p101_env *env, struct p101_error *err, const char *text);
static bool analysis_has_trouble(const struct p101_sync_check_model *model);

int p101_sync_check_analyze(const struct p101_env *env, struct p101_error *err, FILE *stream, bool json)
{
    struct p101_sync_check_model *model;
    char                          line[P101_TOOL_EVENT_LINE_MAX_BYTES];
    int                           result;

    model = p101_calloc(env, err, 1U, sizeof(*model));
    if(model == NULL || p101_error_has_error(err))
    {
        return P101_SYNC_CHECK_EXIT_TROUBLE;
    }
    p101_sync_check_model_init(env, model);
    while(p101_error_has_no_error(err))
    {
        struct p101_tool_event_record record;
        p101_tool_event_line_status   line_status;
        p101_tool_event_parse_status  parse_status;

        line_status = p101_tool_event_read_line(err, stream, line, sizeof(line));
        if(line_status == P101_TOOL_EVENT_LINE_EOF || line_status == P101_TOOL_EVENT_LINE_ERROR)
        {
            break;
        }
        if(line_status == P101_TOOL_EVENT_LINE_MALFORMED)
        {
            model->malformed++;
            continue;
        }
        parse_status = p101_tool_event_parse_line(line, &record);
        if(parse_status == P101_TOOL_EVENT_PARSE_OTHER)
        {
            continue;
        }
        if(parse_status == P101_TOOL_EVENT_PARSE_BAD_VERSION)
        {
            model->unsupported++;
            continue;
        }
        if(parse_status != P101_TOOL_EVENT_PARSE_OK)
        {
            model->malformed++;
            continue;
        }
        (void)p101_sync_check_ingest(env, err, model, &record);
    }
    if(p101_error_has_no_error(err))
    {
        p101_sync_check_finish(env, err, model);
    }
    if(json)
    {
        report_json(env, err, model);
    }
    else
    {
        report_text(env, err, model);
    }
    result = P101_SYNC_CHECK_EXIT_CLEAN;
    if(p101_error_has_error(err) || model->malformed != 0U || model->unsupported != 0U || analysis_has_trouble(model))
    {
        result = P101_SYNC_CHECK_EXIT_TROUBLE;
    }
    else if(model->finding_count != 0U)
    {
        result = P101_SYNC_CHECK_EXIT_FINDINGS;
    }
    p101_sync_check_model_destroy(env, model);
    p101_free(env, model);
    return result;
}

static bool analysis_has_trouble(const struct p101_sync_check_model *model)
{
    for(size_t index = 0U; index < model->finding_count; index++)
    {
        if(model->findings[index].kind == P101_SYNC_CHECK_INCOMPLETE_STREAM || model->findings[index].kind == P101_SYNC_CHECK_CAPACITY)
        {
            return true;
        }
    }
    return false;
}

static void report_text(const struct p101_env *env, struct p101_error *err, const struct p101_sync_check_model *model)
{
    p101_fprintf(env,
                 err,
                 stdout,
                 "p101-sync-check: %zu finding%s, %zu lock-order edge%s, %zu malformed, %zu unsupported\n",
                 model->finding_count,
                 model->finding_count == 1U ? "" : "s",
                 model->lock_edge_count,
                 model->lock_edge_count == 1U ? "" : "s",
                 model->malformed,
                 model->unsupported);
    for(size_t index = 0U; index < model->finding_count; index++)
    {
        const struct p101_sync_check_finding *finding;

        finding = &model->findings[index];
        p101_fprintf(env, err, stdout, "%s: %s (event %zu): %s -> %s\n", p101_sync_check_finding_id(finding->kind), p101_sync_check_finding_message(finding->kind), finding->sequence, finding->first, finding->second);
    }
}

static void report_json(const struct p101_env *env, struct p101_error *err, const struct p101_sync_check_model *model)
{
    p101_fprintf(env, err, stdout, "{\"schema\":\"p101-sync-check-findings-v1\",\"findings\":[");
    for(size_t index = 0U; index < model->finding_count; index++)
    {
        const struct p101_sync_check_finding *finding;

        finding = &model->findings[index];
        p101_fprintf(env, err, stdout, "%s{\"id\":", index == 0U ? "" : ",");
        json_string(env, err, p101_sync_check_finding_id(finding->kind));
        p101_fputs(env, err, ",\"severity\":\"error\",\"location\":{\"sequence\":", stdout);
        p101_fprintf(env, err, stdout, "%zu},\"message\":", finding->sequence);
        json_string(env, err, p101_sync_check_finding_message(finding->kind));
        p101_fputs(env, err, ",\"evidence\":{\"from\":", stdout);
        json_string(env, err, finding->first);
        p101_fputs(env, err, ",\"to\":", stdout);
        json_string(env, err, finding->second);
        p101_fputs(env, err, "}}", stdout);
    }
    p101_fprintf(env, err, stdout, "],\"summary\":{\"findings\":%zu,\"lock_order_edges\":%zu,\"malformed\":%zu,\"unsupported\":%zu}}\n", model->finding_count, model->lock_edge_count, model->malformed, model->unsupported);
}

static void json_string(const struct p101_env *env, struct p101_error *err, const char *text)
{
    const unsigned char *cursor;
    size_t               length;

    p101_fputc(env, err, '"', stdout);
    if(text == NULL)
    {
        p101_fputc(env, err, '"', stdout);
        return;
    }
    cursor = (const unsigned char *)text;
    length = p101_strlen(env, text);
    for(size_t index = 0U; index < length && p101_error_has_no_error(err); index++)
    {
        switch(*cursor)
        {
            case '"':
                p101_fputs(env, err, "\\\"", stdout);
                break;
            case '\\':
                p101_fputs(env, err, "\\\\", stdout);
                break;
            case '\b':
                p101_fputs(env, err, "\\b", stdout);
                break;
            case '\f':
                p101_fputs(env, err, "\\f", stdout);
                break;
            case '\n':
                p101_fputs(env, err, "\\n", stdout);
                break;
            case '\r':
                p101_fputs(env, err, "\\r", stdout);
                break;
            case '\t':
                p101_fputs(env, err, "\\t", stdout);
                break;
            default:
                if(*cursor < JSON_CONTROL_LIMIT)
                {
                    p101_fprintf(env, err, stdout, "\\u%04x", (unsigned int)*cursor);
                }
                else
                {
                    p101_fputc(env, err, (int)*cursor, stdout);
                }
                break;
        }
        cursor++;
    }
    p101_fputc(env, err, '"', stdout);
}
