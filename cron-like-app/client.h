#ifndef SF_MASZYNA_WIRTUALNA_CLIENT_H
#define SF_MASZYNA_WIRTUALNA_CLIENT_H

enum request_type_t parse_request(const char *cmd);
enum clock_type_t parse_clock_type(const char *cmd);
int send_request(struct query_t query);

#endif
