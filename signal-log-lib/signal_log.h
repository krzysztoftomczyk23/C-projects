#ifndef SF_MASZYNA_WIRTUALNA_SIGNAL_LOG_H
#define SF_MASZYNA_WIRTUALNA_SIGNAL_LOG_H

enum signal_log_detail_lvl {
    MIN, STANDARD, MAX
};

int signal_log_init(const char *filename);
void signal_log_change_log_path(const char *filename);
int signal_log_save_log(enum signal_log_detail_lvl lvl, char * format, ...);
int signal_log_clean();
int signal_log_dump_state();

#endif
