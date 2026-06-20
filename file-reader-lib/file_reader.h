#ifndef PROJECT1_DIRECTORY_READER_H
#define PROJECT1_DIRECTORY_READER_H

struct creation_date_t  {
    int day;
    int month;
    int year;
};

struct creation_time_t  {
    int hour;
    int minute;
    int second;
};

struct dir_entry_t  {
    char name[13];
    size_t size;
    int is_archived;
    int is_readonly;
    int is_system;
    int is_hidden;
    int is_directory;
    int is_volume;
    struct creation_date_t creation_date;
    struct creation_time_t creation_time;
    uint16_t first_cluster;
};

struct clusters_chain_t {
    uint16_t *clusters;
    size_t size;
};

struct dir_entry_t *read_directory_entry(const int8_t *file);
struct clusters_chain_t *get_chain_fat16(const void * buffer, size_t size, uint16_t first_cluster);


struct disk_t{
    FILE * disk;
    int32_t sectors;
    size_t size;
};

struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

struct volume_t{
    FILE * fat_volume_data;
    uint32_t first_sector;
    int8_t * super_sector;
    int16_t * fat_table;
    int8_t * fat_root_dir;
    int8_t sectors_per_cluster;
    int16_t bytes_per_sector;
    int16_t size_of_area;
    int16_t size_of_fat;
    long offset_to_data;
    size_t size_of_disk;
    int16_t max_num_of_files;
};


struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);


struct file_t{
    struct dir_entry_t * file_info;
    struct volume_t * volume;
    int pos_in_file;
};
struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);


struct dir_t{
    int8_t * dir_info;
    struct volume_t * volume;
    int num_of_files;
    int pos_in_dir;
};

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);



#endif
