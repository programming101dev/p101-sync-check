#ifndef P101_SYNC_CHECK_FINDING_H
#define P101_SYNC_CHECK_FINDING_H

#include "concurrency_types.h"

const char *p101_sync_check_finding_id(enum p101_sync_check_finding_kind kind);
const char *p101_sync_check_finding_message(enum p101_sync_check_finding_kind kind);

#endif
