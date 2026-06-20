#ifndef SF_MASZYNA_WIRTUALNA_SHARED_H
#define SF_MASZYNA_WIRTUALNA_SHARED_H
#include <time.h>
#include <fcntl.h>
#include <mqueue.h>
#include <string.h>
#include <stdlib.h>

#define MQ_QUERY_NAME "/mq_query"
enum request_type_t {ADD, DELETE, DISPLAY_TASKS, SAVE_LOG, CLOSE};
enum clock_type_t {ABSOLUTE, RELATIVE, CYCLICAL};

struct query_t {
    enum request_type_t request_type;
    enum clock_type_t clock_type;
    char reply_queue_name[256];
    char time_str[64];
    char program_file[64];
    int are_program_args_available;
    char program_args[32];
    int delete_id;
};

struct reply_t {
    char task_description[256];
    int is_last_task;
};



#endif
