#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include "file_reader.h"




struct dir_entry_t *read_directory_entry(const int8_t *file){
    struct dir_entry_t * entry = malloc(sizeof(struct dir_entry_t));
    if(entry == NULL){
        return NULL;
    }
    uint8_t buffor[32];

    memcpy(buffor,file,32);

    memcpy(entry->name, &buffor, 8);
    *(entry->name+12) = '\0';
    memcpy(entry->name+9, &buffor[8], 3);
    if(*(entry->name+9) != ' '){
        *(entry->name+8) = '.';
    } else{
        *(entry->name+8) = ' ';
    }
    int max_size=12;
    for (int i = 0; i < max_size; ++i) {
        if(*(entry->name+i) == ' '){
            for (int j = i; j < max_size; ++j) {
                *(entry->name+j) = *(entry->name+j+1);
            }
            i--;
            max_size--;
        }
    }
    entry->size = *(uint32_t *)(buffor + 28);
    uint8_t file_attributes = buffor[11];
    entry->is_archived = (file_attributes & 0x20) != 0;
    entry->is_readonly = (file_attributes & 0x01) != 0;
    entry->is_system = (file_attributes & 0x04) != 0;
    entry->is_hidden = (file_attributes & 0x02) != 0;
    entry->is_directory = (file_attributes & 0x10) != 0;

    uint16_t date = *(uint16_t *)(buffor + 16);
    entry->creation_date.day = date & 0x1F;
    entry->creation_date.month = (date >> 5) & 0x0F;
    entry->creation_date.year = 1980 + ((date >> 9) & 0x7F);

    if (buffor[11] == 0x08) {
        entry->is_volume = 1;
    } else {
        entry->is_volume = 0;
    }

    uint16_t time = *(uint16_t *)(buffor + 14);
    entry->creation_time.second = (time & 0x1F) * 2;
    entry->creation_time.minute = (time >> 5) & 0x3F;
    entry->creation_time.hour = (time >> 11) & 0x1F;
    entry->first_cluster = *(uint16_t *)(buffor + 26);
    return entry;
}



struct clusters_chain_t *get_chain_fat16(const void * const buffer, size_t size, uint16_t first_cluster){
    if(buffer == NULL || size <= 0 || first_cluster == 0 || first_cluster == 0xFFFF){
        return NULL;
    }
    int16_t * table = (int16_t *)buffer;
    int16_t max_clusters = size/ sizeof(int16_t);
    if(first_cluster >= max_clusters){
        return NULL;
    }
    struct clusters_chain_t * chain = malloc(sizeof(struct clusters_chain_t));
    if(chain == NULL){
        return NULL;
    }
    chain->clusters = malloc(sizeof(uint16_t));
    if(chain->clusters == NULL){
        free(chain);
        return NULL;
    }
    *chain->clusters = first_cluster;
    size_t cur_size = 1;
    chain->size = cur_size;
    uint16_t cur_cluster = first_cluster;
    while (1){
        cur_cluster = *(table+cur_cluster);
        if(cur_cluster >= max_clusters){
            break;
        }
        cur_size++;
        chain->size = cur_size;
        uint16_t * new_chain = realloc(chain->clusters,cur_size* sizeof(uint16_t));
        if(new_chain== NULL){
            free(chain->clusters);
            free(chain);
            return NULL;
        }
        chain->clusters = new_chain;
        *(chain->clusters+cur_size-1) = cur_cluster;
    }
    return chain;
}



struct disk_t* disk_open_from_file(const char* volume_file_name){
    if(volume_file_name == NULL){
        errno = EFAULT;
        return NULL;
    }
    FILE * new_file = fopen(volume_file_name,"rb");
    if(new_file == NULL){
        errno = ENOENT;
        return NULL;
    }
    struct disk_t * disk = malloc(sizeof(struct disk_t));
    if(disk == NULL){
        fclose(new_file);
        errno = ENOMEM;
        return NULL;
    }
    disk->disk = new_file;
    fseek(disk->disk,0,SEEK_END);
    long size_in_bytes = ftell(disk->disk);
    fseek(disk->disk,0,SEEK_SET);
    int32_t size_in_sectors = size_in_bytes/512;
    disk->sectors = size_in_sectors;
    disk->size = size_in_bytes;
    return disk;
}

int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read){
    if(pdisk == NULL || buffer == NULL || first_sector < 0){
        errno = EFAULT;
        return -1;
    }
    int32_t last_sector = first_sector + sectors_to_read;
    if(last_sector > pdisk->sectors){
        errno = ERANGE;
        return -1;
    }
    fseek(pdisk->disk,first_sector*512,SEEK_SET);
    fread(buffer,1,512*sectors_to_read,pdisk->disk);
    return sectors_to_read;
}

int disk_close(struct disk_t* pdisk){
    if(pdisk == NULL){
        errno = EFAULT;
        return -1;
    }
    fclose(pdisk->disk);
    free(pdisk);
    return 0;
}



struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector){
    if(pdisk == NULL || pdisk->disk == NULL || first_sector >= (uint32_t)pdisk->sectors){
        errno = EFAULT;
        return NULL;
    }
    FILE * fat = pdisk->disk;
    fseek(fat,first_sector*512,SEEK_SET);
    int8_t * super_sector = malloc(512* sizeof(int8_t));
    if(super_sector == NULL){
        errno = ENOMEM;
        return NULL;
    }
    fread(super_sector,1,512,fat);
    if(*(uint16_t *)(super_sector+510) != 0xAA55){
        free(super_sector);
        errno = EINVAL;
        return NULL;
    }
    int16_t number_of_sectorrs = *(int16_t *)(super_sector+19);
    if(number_of_sectorrs < 0){
        return NULL;
    }

    int16_t size_of_reserved_area = *(int16_t *)(super_sector+14);
    int16_t size_of_fat = *(int16_t *)(super_sector+22);
    int16_t bytes_per_sector = *(int16_t *)(super_sector+11);
    fseek(fat,size_of_reserved_area*bytes_per_sector,SEEK_SET);
    int16_t * first_fat = malloc(size_of_fat*bytes_per_sector);
    if(first_fat == NULL){
        free(super_sector);
        errno = ENOMEM;
        return NULL;
    }
    int16_t * second_fat = malloc(size_of_fat*bytes_per_sector);
    if(second_fat == NULL){
        free(super_sector);
        free(first_fat);
        errno = ENOMEM;
        return NULL;
    }
    size_t num_to_read = size_of_fat * bytes_per_sector/ sizeof(int16_t);
    fread(first_fat, sizeof(int16_t),num_to_read,fat);
    fread(second_fat, sizeof(int16_t),num_to_read,fat);
    for (int i = 0; i < (int)num_to_read; ++i) {
        if(*(first_fat+i) != *(second_fat+i)){
            free(super_sector);
            free(first_fat);
            free(second_fat);
            errno = EINVAL;
            return NULL;
        }
    }
    free(second_fat);
    struct volume_t * new_volumine = malloc(sizeof(struct volume_t));
    if(new_volumine == NULL){
        free(super_sector);
        free(first_fat);
        errno = ENOMEM;
        return NULL;
    }
    new_volumine->super_sector = super_sector;
    new_volumine->fat_table = first_fat;
    new_volumine->first_sector = first_sector;
    new_volumine->fat_volume_data = pdisk->disk;
    new_volumine->sectors_per_cluster = *(super_sector+13);
    new_volumine->size_of_fat = size_of_fat;
    new_volumine->bytes_per_sector = bytes_per_sector;
    int16_t max_number_of_files = *(int16_t *)(super_sector+17);
    new_volumine->max_num_of_files = max_number_of_files;

    int8_t * root_dir = malloc(max_number_of_files*32);
    if(root_dir == NULL){
        free(new_volumine->super_sector);
        free(new_volumine->fat_table);
        free(new_volumine);
        errno = ENOMEM;
        return NULL;
    }
    fread(root_dir,1,max_number_of_files*32,fat);
    new_volumine->offset_to_data = ftell(fat);
    new_volumine->fat_root_dir = root_dir;
    int size_of_area = *(uint16_t *)((uint8_t *)super_sector + 14) * new_volumine->bytes_per_sector;
    new_volumine->size_of_area = size_of_area;
    new_volumine->size_of_disk = pdisk->size;
    return new_volumine;
}

int fat_close(struct volume_t* pvolume){
    if(pvolume == NULL || pvolume->fat_table == NULL || pvolume->super_sector == NULL || pvolume->fat_root_dir == NULL){
        errno = EFAULT;
        return -1;
    }
    free(pvolume->fat_table);
    free(pvolume->super_sector);
    free(pvolume->fat_root_dir);
    free(pvolume);
    return 0;
}



struct file_t* file_open(struct volume_t* pvolume, const char* file_name){
    if(pvolume == NULL || file_name == NULL){
        errno = EFAULT;
        return NULL;
    }
    struct dir_entry_t * entry_found = NULL;
    for (int i = 0; i < pvolume->max_num_of_files; ++i) {
        struct dir_entry_t * temp_entry = read_directory_entry(pvolume->fat_root_dir+i*32);
        if(temp_entry == NULL){
            errno = ENOMEM;
            return NULL;
        }
        if(strcmp(temp_entry->name,file_name)==0){
            entry_found = temp_entry;
            break;
        }
        free(temp_entry);
    }
    if(entry_found == NULL){
        errno = ENOENT;
        return NULL;
    }
    if(entry_found->is_directory == 1 || entry_found->is_volume == 1){
        free(entry_found);
        errno = EISDIR;
        return NULL;
    }
    struct file_t * new_file = malloc(sizeof(struct file_t));
    if(new_file == NULL){
        free(entry_found);
        errno = ENOMEM;
        return NULL;
    }
    new_file->file_info = entry_found;
    new_file->volume = pvolume;
    new_file->pos_in_file = 0;
    return new_file;
}
int file_close(struct file_t* stream){
    if(stream == NULL){
        errno = EFAULT;
        return -1;
    }
    free(stream->file_info);
    free(stream);
    return 0;
}

size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream){
    if(ptr == NULL || stream == NULL || size <= 0 || nmemb <= 0){
        errno = EFAULT;
        return -1;
    }
    FILE * data = stream->volume->fat_volume_data;
    size_t file_size = size*nmemb;
    int8_t *pointer = ptr;


    if((size_t)file_size+(size_t)stream->volume->offset_to_data > stream->volume->size_of_disk){
        errno = ENXIO;
        return -1;
    }

    if((size_t)stream->pos_in_file == stream->file_info->size){
        return 0;
    }
    struct clusters_chain_t * clusters = get_chain_fat16(stream->volume->fat_table,stream->volume->bytes_per_sector*stream->volume->size_of_fat,stream->file_info->first_cluster);

    fseek(data,stream->volume->offset_to_data,SEEK_SET);
    size_t bytes_to_pos = 0;
    size_t bytes_already = 0;
    int starting_cluster = stream->pos_in_file/(stream->volume->bytes_per_sector*stream->volume->sectors_per_cluster);
    bytes_to_pos = starting_cluster*stream->volume->bytes_per_sector*stream->volume->sectors_per_cluster;
    for (int i = starting_cluster; i < (int)clusters->size; ++i) {
        int move_value = stream->volume->offset_to_data+(*(clusters->clusters+i)-2)*stream->volume->bytes_per_sector*stream->volume->sectors_per_cluster;
        int offset_additional = stream->pos_in_file;
        offset_additional-=i*(stream->volume->bytes_per_sector*stream->volume->sectors_per_cluster);
        move_value+=offset_additional;
        bytes_to_pos+=offset_additional;
        fseek(data,move_value,SEEK_SET);
        for (int j = offset_additional; j < stream->volume->bytes_per_sector*stream->volume->sectors_per_cluster; ++j) {
            if(stream->pos_in_file == (int)stream->file_info->size){
                break;
            }
            if(bytes_already == nmemb*size){
                break;
            }
            if(bytes_to_pos < (size_t)stream->pos_in_file){
                fseek(data,1,SEEK_CUR);
                bytes_to_pos++;
            }else {
                fread((pointer + bytes_already), 1, 1, data);
                bytes_already++;
                bytes_to_pos++;
                stream->pos_in_file++;
            }
        }
        if(stream->pos_in_file == (int)stream->file_info->size){
            break;
        }
        if(bytes_already == nmemb*size){
            break;
        }
    }
    free(clusters->clusters);
    free(clusters);
    int res = bytes_already/(int)size;
    return res;
}

int32_t file_seek(struct file_t* stream, int32_t offset, int whence){
    if(stream == NULL){
        errno = EFAULT;
        return -1;
    }
    if(whence != SEEK_CUR && whence != SEEK_SET && whence != SEEK_END){
        errno = EINVAL;
        return -1;
    }
    if(whence == SEEK_SET){
        if(offset < 0 || offset > (int32_t)stream->file_info->size){
            errno = ENXIO;
            return -1;
        }
        stream->pos_in_file = offset;
    }
    if(whence == SEEK_END){
        if(offset > 0 || offset < -(int32_t)stream->file_info->size){
            errno = ENXIO;
            return -1;
        }
        stream->pos_in_file = (int32_t)stream->file_info->size + offset;
    }
    if(whence == SEEK_CUR){
        if(stream->pos_in_file + offset < 0 || stream->pos_in_file+offset > (int)stream->file_info->size){
            errno = ENXIO;
            return -1;
        }
        stream->pos_in_file = (int32_t)stream->pos_in_file + offset;
    }
    int32_t res = stream->pos_in_file;
    return res;
}



struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path){
    if(pvolume == NULL || dir_path == NULL){
        return NULL;
    }
    if(strcmp("\\",dir_path)==0){
        struct dir_t * new_dir = malloc(sizeof(struct dir_t));
        if(new_dir == NULL){
            errno = ENOMEM;
            return NULL;
        }
        new_dir->volume = pvolume;
        new_dir->dir_info = malloc(pvolume->max_num_of_files*32);
        if(new_dir->dir_info == NULL){
            free(new_dir);
            errno = ENOMEM;
            return NULL;
        }
        memcpy(new_dir->dir_info,pvolume->fat_root_dir,pvolume->max_num_of_files*32);
        new_dir->num_of_files = pvolume->max_num_of_files;
        new_dir->pos_in_dir = 0;
        return new_dir;

    }else{
        return NULL;
    }

}
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry){
    if(pdir == NULL || pentry == NULL){
        errno = EFAULT;
        return -1;
    }
    struct dir_entry_t * entry_found = NULL;
    for (int i = pdir->pos_in_dir; i < pdir->num_of_files; ++i) {
        struct dir_entry_t * temp_entry = read_directory_entry(pdir->dir_info+i*32);
        pdir->pos_in_dir++;
        if(temp_entry == NULL){
            errno = ENOMEM;
            return -1;
        }
        if(temp_entry->name[0] == 0x00 || (unsigned char)temp_entry->name[0] == 0xE5){
            free(temp_entry);
            continue;
        }
        entry_found = temp_entry;
        break;
    }
    if(entry_found == NULL){
        return 1;
    }
    memcpy(pentry,entry_found, sizeof(struct dir_entry_t));
    free(entry_found);
    return 0;
}

int dir_close(struct dir_t* pdir){
    if(pdir == NULL){
        errno = EFAULT;
        return -1;
    }
    free(pdir->dir_info);
    free(pdir);
    return 0;
}

