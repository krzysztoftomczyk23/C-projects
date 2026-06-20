#include <stdio.h>
#include "server.h"
#include <signal.h>
#include <pthread.h>
#include "signal_log.h"
#include "shared.h"
#include <spawn.h>
#include <sys/wait.h>

pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
#define TASKS_LIMIT 50
#define ARGS_LIMIT 20
char **environ;

struct task_t {
    char filename[64];
    char arguments[32];
    char time_string[64];
    enum clock_type_t clock_type;
    timer_t timer;
    int active;
    int are_program_args_available;
};


struct task_t tasks[TASKS_LIMIT];
int counter = 0;

void* timer_thread(void* arg){
    struct task_t *task = (struct task_t *)arg;
    char *argv[ARGS_LIMIT];
    int i = 0;
    pthread_mutex_lock(&list_mutex);
    argv[i++] = task->filename;
    char args_copy[32];
    if (task->are_program_args_available) {
        strncpy(args_copy, task->arguments, sizeof(args_copy));
        args_copy[sizeof(args_copy) - 1] = '\0';
        char *token = strtok(args_copy, " ");
        while (token && i < ARGS_LIMIT-1) {
            argv[i++] = token;
            token = strtok(NULL, " ");
        }
    }
    argv[i] = NULL;

    pid_t pid;
    posix_spawn(&pid, task->filename, NULL, NULL, argv, environ);
    pthread_mutex_unlock(&list_mutex);
    int status;
    waitpid(pid, &status, 0);


    pthread_mutex_lock(&list_mutex);
    if(task->clock_type == ABSOLUTE || task->clock_type == RELATIVE){
        timer_delete(task->timer);
        task->active = 0;
    }
    pthread_mutex_unlock(&list_mutex);
}

int run_server(mqd_t mq_query_id){
    struct query_t query;
    int stop = 0;
    while (1){
        if(stop == 1){
            break;
        }
        mq_receive(mq_query_id, (char *)&query, sizeof(struct query_t), NULL);
        enum request_type_t req = query.request_type;
        switch (req) {
            case ADD: {
                if (counter == TASKS_LIMIT)
                    return 2;

                pthread_mutex_lock(&list_mutex);
                strcpy(tasks[counter].arguments, query.program_args);
                strcpy(tasks[counter].filename, query.program_file);
                strcpy(tasks[counter].time_string, query.time_str);
                tasks[counter].are_program_args_available = query.are_program_args_available;

                enum clock_type_t clock_type = query.clock_type;
                tasks[counter].clock_type = clock_type;
                struct sigevent timer_event;
                timer_event.sigev_notify = SIGEV_THREAD;
                timer_event.sigev_notify_function = timer_thread;
                timer_event.sigev_value.sival_ptr = &tasks[counter];
                timer_event.sigev_notify_attributes = NULL;
                timer_create(CLOCK_REALTIME, &timer_event, &tasks[counter].timer);
                struct itimerspec value;

                switch (clock_type) {
                    case ABSOLUTE:{
                        struct tm tm = {0};
                        sscanf(query.time_str, "%d.%d.%d.%d:%d",&tm.tm_year,&tm.tm_mon,&tm.tm_mday,&tm.tm_hour,&tm.tm_min);
                        tm.tm_year -= 1900;
                        tm.tm_mon  -= 1;

                        time_t target = mktime(&tm);

                        value.it_value.tv_sec = target;
                        value.it_value.tv_nsec = 0;
                        value.it_interval.tv_sec = 0;
                        value.it_interval.tv_nsec = 0;
                        timer_settime(tasks[counter].timer, TIMER_ABSTIME, &value, NULL);
                        break;
                    }
                    case RELATIVE:{
                        int h, m, s;
                        sscanf(query.time_str, "%d:%d:%d", &h, &m, &s);
                        time_t seconds = h * 3600 + m * 60 + s;
                        value.it_value.tv_sec = seconds;
                        value.it_value.tv_nsec = 0;
                        value.it_interval.tv_sec = 0;
                        value.it_interval.tv_nsec = 0;
                        timer_settime(tasks[counter].timer, 0, &value, NULL);
                        break;
                    }
                    case CYCLICAL:{
                        int h, m, s;
                        sscanf(query.time_str, "%d:%d:%d", &h, &m, &s);
                        time_t seconds = h * 3600 + m * 60 + s;
                        value.it_value.tv_sec = seconds;
                        value.it_value.tv_nsec = 0;
                        value.it_interval.tv_sec = seconds;
                        value.it_interval.tv_nsec = 0;
                        timer_settime(tasks[counter].timer, 0, &value, NULL);
                        break;
                    }
                    default:
                        break;
                }
                tasks[counter].active = 1;
                counter++;
                pthread_mutex_unlock(&list_mutex);
                break;
            }
            case DELETE:{
                pthread_mutex_lock(&list_mutex);
                int delete_index = query.delete_id;
                tasks[delete_index].active = 0;
                timer_delete(tasks[delete_index].timer);
                pthread_mutex_unlock(&list_mutex);
                break;
            }
            case DISPLAY_TASKS:{
                struct reply_t reply;
                mqd_t mq_reply_to_client = mq_open(query.reply_queue_name, O_WRONLY);
                pthread_mutex_lock(&list_mutex);
                reply.is_last_task = 0;
                for(int i  = 0; i < counter; i++){
                    if(tasks[i].active == 0){
                        continue;
                    }
                    sprintf(reply.task_description, "task id: %d, task program: %s, task type: %d, task time given: %s", i, tasks[i].filename, tasks[i].clock_type, tasks[i].time_string);
                    mq_send(mq_reply_to_client, (const char *)&reply, sizeof(struct reply_t), 0);
                }
                reply.is_last_task = 1;
                mq_send(mq_reply_to_client, (const char *)&reply, sizeof(struct reply_t), 0);
                pthread_mutex_unlock(&list_mutex);
                mq_close(mq_reply_to_client);
                break;
            }
            case SAVE_LOG:{
                signal_log_init("task_manager_log.txt");
                time_t time_now;
                struct tm * time_info;
                time(&time_now);
                time_info = localtime(&time_now);
                int temp = counter - 1;
                signal_log_save_log(MAX,"LOG: %02d-%02d-%04d %02d:%02d:%02d aktualnie zainicjalizowano %d zadan\n", time_info->tm_mday, time_info->tm_mon + 1, time_info->tm_year + 1900, time_info->tm_hour, time_info->tm_min, time_info->tm_sec, temp);
                signal_log_clean();
                break;
            }
            case CLOSE:{
                stop = 1;
                break;
            }
            default:
                break;
        }
    }

    mq_close(mq_query_id);
    mq_unlink(MQ_QUERY_NAME);
    return 0;
}

