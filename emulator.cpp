#include <openssl/md5.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <zlib.h>
#include "uthash.h"
#include <sys/types.h>
#include <dirent.h>
#include "sub_file_chunking.h"
#include <openssl/sha.h>
#include<string>
#include <set>
using namespace std;

#define CacheSize 1024 * 1024 * 1024

void generate_file_abs_path(char* buffer, const char* dir_path, const char* file_relative_path){
    memset(buffer, 0, sizeof(buffer)); 
	strcpy(buffer, dir_path); 
	strcat(buffer, file_relative_path);
}

int main(int argc, char** argv) {
    
    char* file_dir = argv[1];

    // codes below are just for testing
    FILE *random_file;
    size_t readStatus = 0;

    int chunk_num = 0;
    char *fileCache = (char *)malloc(CacheSize);
    memset(fileCache, 0, sizeof(CacheSize));

    int offset = 0, chunk_length = 0, readFlag = 0;

    DIR * file_DIR = opendir(file_dir);
    if (file_DIR == NULL) {
        perror("Fail to open dir");
        exit(-1);
    }

    struct dirent* dir_entry;
    char file_abs_path[512]={0}; 
    while((dir_entry=readdir(file_DIR))!=NULL) {
        if(strcmp(dir_entry->d_name, ".")==0 || strcmp(dir_entry->d_name, "..")==0)
            continue;
        generate_file_abs_path(file_abs_path, argv[1], dir_entry->d_name);

        FILE* random_file = fopen(file_abs_path, "r+");
        if (random_file == NULL) {
            perror("Fail to open file");
            exit(-1);
        }

        fseek(random_file, 0, SEEK_END); // seek to end of file
        int file_size = ftell(random_file); // get current file pointer
        fseek(random_file, 0, SEEK_SET); 
        int end = file_size - 1;
        readStatus = fread(fileCache, 1, CacheSize, random_file);

        unsigned char fingerprint[SHA_DIGEST_LENGTH];
        set<string> fps;
        for(;;){
            if((end - offset + 1) == 0){
                break;
            }
            chunk_length = pdf_chunking(fileCache + offset, end - offset + 1); 

            memset(fingerprint, 0, SHA_DIGEST_LENGTH);
            SHA1((const unsigned char*)fileCache + offset, chunk_length, fingerprint);
            string fp((const char*)fingerprint, SHA_DIGEST_LENGTH);
            fps.insert(fp);
            
            offset += chunk_length;
            chunk_num += 1;

            if (offset >= end && readFlag == 1){
                printf("last chunk size = %d\n", chunk_length);
                break;
            }   
        }
    }

    printf("chunknum is %d\n", chunk_num);
    return 0;
}