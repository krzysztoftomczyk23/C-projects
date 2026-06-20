#include <stdio.h>
#include <string.h>
#include "shared.h"
#include <unistd.h>

enum request_type_t parse_request(const char *cmd){
    if(strcmp(cmd, "add") == 0) return ADD;
    if(strcmp(cmd, "delete") == 0) return DELETE;
    if(strcmp(cmd, "display") == 0) return DISPLAY_TASKS;
    if(strcmp(cmd, "save") == 0) return SAVE_LOG;
    if(strcmp(cmd, "close") == 0) return CLOSE;
    return -1;
}

enum clock_type_t parse_clock_type(const char *cmd){
    if(strcmp(cmd, "absolute") == 0) return ABSOLUTE;
    if(strcmp(cmd, "relative") == 0) return RELATIVE;
    if(strcmp(cmd, "cyclical") == 0) return CYCLICAL;
    return -1;
}

int send_request(struct query_t query){
    mqd_t mq_query_id = mq_open(MQ_QUERY_NAME, O_WRONLY);
    if(query.request_type == DISPLAY_TASKS){
        char mq_reply_name[30];
        sprintf(mq_reply_name, "/mq_reply_list_view_%d", getpid());
        strcpy(query.reply_queue_name, mq_reply_name);

        struct mq_attr mq_reply_attr;
        mq_reply_attr.mq_msgsize = sizeof(struct reply_t);
        mq_reply_attr.mq_maxmsg = 1;
        mqd_t mq_reply_id = mq_open(mq_reply_name, O_CREAT | O_RDONLY, 0666, &mq_reply_attr);
        struct reply_t reply;
        mq_send(mq_query_id, (const char *)&query, sizeof(struct query_t), 0);

        while (1){
            mq_receive(mq_reply_id, (char *)&reply, sizeof(struct reply_t), NULL);
            if (reply.is_last_task == 1) {
                break;
            }
            printf("%s\n", reply.task_description);
        }
        mq_close(mq_query_id);

        mq_close(mq_reply_id);
        mq_unlink(mq_reply_name);
    } else{
        mq_send(mq_query_id, (const char *)&query, sizeof(struct query_t), 0);
        mq_close(mq_query_id);
    }
    return 0;
}