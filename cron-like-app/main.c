#include <stdio.h>
#include "shared.h"
#include "client.h"
#include "server.h"


int main(int argc, char** argv) {
    struct mq_attr mq_query_attr;
    mq_query_attr.mq_msgsize = sizeof(struct query_t);
    mq_query_attr.mq_maxmsg = 1;

    mqd_t mq_query_id = mq_open(MQ_QUERY_NAME, O_RDONLY | O_CREAT | O_EXCL, 0666, &mq_query_attr);

    if(mq_query_id == -1){
        if(argc < 2){
            printf("brak argumentu sterujacego");
            return -1;
        }
        enum request_type_t req = parse_request(argv[1]);
        switch (req) {
            case ADD: {
                if(argc < 3){
                    printf("brak argumentu specyfikujacego typ zegara");
                    return -1;
                }
                enum clock_type_t clock_type = parse_clock_type(argv[2]);
                struct query_t query = {0};
                query.clock_type = clock_type;
                query.request_type = req;
                if(argc < 4){
                    printf("brak argumentu specyfikujacego czas wykonania zegara");
                    return -1;
                }
                strcpy(query.time_str, argv[3]);
                if(argc < 5){
                    printf("brak argumentu specyfikujacego sciezke do programu");
                    return -1;
                }
                strcpy(query.program_file, argv[4]);
                query.are_program_args_available = 0;
                if(argc == 6) {
                    strcpy(query.program_args, argv[5]);
                    query.are_program_args_available = 1;
                }
                send_request(query);
                break;
            }
            case DELETE:{
                struct query_t query = {0};
                query.request_type = req;
                if(argc < 3){
                    printf("brak argumentu podajacego indeks do usuniecia");
                    return -1;
                }
                query.delete_id = atoi(argv[2]);
                send_request(query);
                break;
            }
            case DISPLAY_TASKS:{
                struct query_t query = {0};
                query.request_type = req;
                send_request(query);
                break;
            }
            case SAVE_LOG:{
                struct query_t query = {0};
                query.request_type = req;
                send_request(query);
                break;
            }
            case CLOSE:{
                struct query_t query = {0};
                query.request_type = req;
                send_request(query);
                break;
            }
            default:
                printf("zla komenda sterujaca\n");
                break;
        }
    }else{
        run_server(mq_query_id);
    }
    return 0;
}
