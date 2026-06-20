#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "signal_log.h"
#include <time.h>
#include <string.h>
#include <stdarg.h>


volatile sig_atomic_t sigrtmin_info;
volatile sig_atomic_t sigrtmin_plus_two_info;
volatile sig_atomic_t break_info;
volatile sig_atomic_t dump_error = 0;
static int initialized = 0;
static int log_on = 1;
static enum signal_log_detail_lvl cur_lvl = STANDARD;
static char log_filename[200];
static pthread_t sigrtmin_tid;
static pthread_t sigrtmin_plus_one_tid;
static pthread_t sigrtmin_plus_two_tid;

pthread_mutex_t dump_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t sigrtmin_sem;
sem_t sigrtmin_plus_one_sem;
sem_t sigrtmin_plus_two_sem;

void sigrtmin_handler(int signo, siginfo_t* info, void* other){
    sigrtmin_info = info->si_value.sival_int;
    sem_post(&sigrtmin_sem);
}

void sigrtmin_plus_one_handler(int signo, siginfo_t* info, void* other){
    sem_post(&sigrtmin_plus_one_sem);
}

void sigrtmin_plus_two_handler(int signo, siginfo_t* info, void* other){
    sigrtmin_plus_two_info = info->si_value.sival_int;
    sem_post(&sigrtmin_plus_two_sem);
}

void* sigrtmin_thread(void * arg){
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN+1);
    sigaddset(&set, SIGRTMIN+2);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    int action = 0;
    while(1){
        while(sem_wait(&sigrtmin_sem));
        if(break_info == 1){
            break;
        }
        action = sigrtmin_info;
        printf("w sigrtmin numer: %d", action);
        if(action == 0){
            char filename[256];
            time_t now = time(NULL);
            struct tm tm_now;
            localtime_r(&now, &tm_now);
            snprintf(filename, sizeof(filename),"dump_%04d-%02d-%02d_%02d-%02d-%02d.txt",tm_now.tm_year + 1900,tm_now.tm_mon + 1,tm_now.tm_mday,tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);

            pthread_mutex_lock(&dump_mutex);
            FILE *maps = fopen("/proc/self/maps", "r");
            if (!maps) {
                dump_error = 1;
                pthread_mutex_unlock(&dump_mutex);
                continue;
            }

            unsigned long start = 0, end = 0;
            char line[512];

            while (fgets(line, sizeof(line), maps)) {
                if (strstr(line, "[stack]")) {
                    sscanf(line, "%lx-%lx", &start, &end);
                    break;
                }
            }
            fclose(maps);
            if (start == 0 || end == 0) {
                dump_error = 1;
                pthread_mutex_unlock(&dump_mutex);
                continue;
            }
            size_t stack_size = end - start;
            FILE *out = fopen(filename, "wb");
            if (!out) {
                dump_error = 1;
                pthread_mutex_unlock(&dump_mutex);
                continue;
            }
            fwrite((void*)start, 1, stack_size, out);
            fclose(out);
            pthread_mutex_unlock(&dump_mutex);
        }
        if(action == 1){
            char filename[256];
            time_t now = time(NULL);
            struct tm tm_now;
            localtime_r(&now, &tm_now);
            snprintf(filename, sizeof(filename),"dump_%04d-%02d-%02d_%02d-%02d-%02d.bin",tm_now.tm_year + 1900,tm_now.tm_mon + 1,tm_now.tm_mday,tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);

            pthread_mutex_lock(&dump_mutex);
            FILE *maps = fopen("/proc/self/maps", "r");
            if (!maps) {
                dump_error = 1;
                pthread_mutex_unlock(&dump_mutex);
                continue;
            }
            unsigned long start = 0, end = 0;
            char line[512];

            while (fgets(line, sizeof(line), maps)) {
                if (strstr(line, "[heap]")) {
                    sscanf(line, "%lx-%lx", &start, &end);
                    break;
                }
            }
            fclose(maps);
            if (start == 0 || end == 0) {
                dump_error = 1;
                pthread_mutex_unlock(&dump_mutex);
                continue;
            }
            size_t stack_size = end - start;
            FILE *out = fopen(filename, "wb");
            if (!out) {
                dump_error = 1;
                pthread_mutex_unlock(&dump_mutex);
                continue;
            }
            fwrite((void*)start, 1, stack_size, out);
            fclose(out);
            pthread_mutex_unlock(&dump_mutex);
        }
    }
    return NULL;
}

void* sigrtmin_plus_one_thread(void * arg){
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN);
    sigaddset(&set, SIGRTMIN+2);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    while(1){
        while(sem_wait(&sigrtmin_plus_one_sem));
        if(break_info == 1){
            break;
        }
        if(log_on == 1){
            log_on = 0;
        } else{
            log_on = 1;
        }
        }
    return NULL;
}

void* sigrtmin_plus_two_thread(void * arg){
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN);
    sigaddset(&set, SIGRTMIN+1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    int lvl = 0;
    while(1){
        while(sem_wait(&sigrtmin_plus_two_sem));
        if(break_info == 1){
            break;
        }
        lvl = sigrtmin_plus_two_info;
        if(lvl == 0){
            cur_lvl = MIN;
        }
        if(lvl == 1){
            cur_lvl = STANDARD;
        }
        if(lvl == 2){
            cur_lvl = MAX;
        }
    }
    return NULL;
}

int signal_log_init(const char *filename){
    if(initialized == 1){
        return -1;
    }
    strcpy(log_filename,filename);

    sem_init(&sigrtmin_sem,0,0);
    sem_init(&sigrtmin_plus_one_sem,0,0);
    sem_init(&sigrtmin_plus_two_sem,0,0);

    pthread_create(&sigrtmin_tid, NULL, sigrtmin_thread,NULL);
    pthread_create(&sigrtmin_plus_one_tid, NULL, sigrtmin_plus_one_thread,NULL);
    pthread_create(&sigrtmin_plus_two_tid, NULL, sigrtmin_plus_two_thread,NULL);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN);
    sigaddset(&set, SIGRTMIN+1);
    sigaddset(&set, SIGRTMIN+2);

    pthread_sigmask(SIG_BLOCK, &set, NULL);

    struct sigaction sigrtmin_sa;
    sigrtmin_sa.sa_sigaction = sigrtmin_handler;
    sigfillset(&sigrtmin_sa.sa_mask);
    sigrtmin_sa.sa_flags = SA_SIGINFO;
    sigaction(SIGRTMIN, &sigrtmin_sa, NULL);

    struct sigaction sigrtmin_plus_one_sa;
    sigrtmin_plus_one_sa.sa_sigaction = sigrtmin_plus_one_handler;
    sigfillset(&sigrtmin_plus_one_sa.sa_mask);
    sigrtmin_plus_one_sa.sa_flags = SA_SIGINFO;
    sigaction(SIGRTMIN+1, &sigrtmin_plus_one_sa, NULL);

    struct sigaction sigrtmin_plus_two_sa;
    sigrtmin_plus_two_sa.sa_sigaction = sigrtmin_plus_two_handler;
    sigfillset(&sigrtmin_plus_two_sa.sa_mask);
    sigrtmin_plus_two_sa.sa_flags = SA_SIGINFO;
    sigaction(SIGRTMIN+2, &sigrtmin_plus_two_sa, NULL);

    initialized = 1;
    break_info = 0;
    return 0;
}

int signal_log_dump_state(){
    if(dump_error == 0)return 0;
    dump_error = 0;
    return 1;
}

int signal_log_save_log(enum signal_log_detail_lvl lvl, char * format, ...){
    if(cur_lvl == STANDARD){
        if(lvl == MIN) return -1;
    }
    if(cur_lvl == MIN){
        if(lvl != MAX) return -1;
    }
    if(log_on == 0)return -2;


    pthread_mutex_lock(&log_mutex);
    FILE *fp = fopen(log_filename, "a+");
    if (!fp) {
        pthread_mutex_unlock(&log_mutex);
        return -3;
    }
    va_list args;
    va_start(args, format);
    if (vfprintf(fp, format, args) < 0) {
        va_end(args);
        fclose(fp);
        pthread_mutex_unlock(&log_mutex);
        return -3;
    }
    fprintf(fp, "\n");
    va_end(args);
    fclose(fp);
    pthread_mutex_unlock(&log_mutex);
    return 0;
}

void signal_log_change_log_path(const char *filename){
    strcpy(log_filename,filename);
}

int signal_log_clean(){
    union sigval value;
    value.sival_int = 0;
    break_info = 1;
    sigqueue(getpid(), SIGRTMIN, value);
    sigqueue(getpid(), SIGRTMIN+1, value);
    sigqueue(getpid(), SIGRTMIN+2, value);

    pthread_join(sigrtmin_tid, NULL);
    pthread_join(sigrtmin_plus_one_tid, NULL);
    pthread_join(sigrtmin_plus_two_tid, NULL);

    sem_destroy(&sigrtmin_sem);
    sem_destroy(&sigrtmin_plus_one_sem);
    sem_destroy(&sigrtmin_plus_two_sem);

    struct sigaction default_sa;
    default_sa.sa_handler = SIG_DFL;
    sigemptyset(&default_sa.sa_mask);
    default_sa.sa_flags = 0;
    sigaction(SIGRTMIN, &default_sa, NULL);
    sigaction(SIGRTMIN+1, &default_sa, NULL);
    sigaction(SIGRTMIN+2, &default_sa, NULL);
    initialized = 0;
    return 0;
}