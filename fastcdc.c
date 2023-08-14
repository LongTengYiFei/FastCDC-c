#include "fastcdc.h"
#include "dirent.h"
#include "sys/types.h"

#define USE_CHUNKING_METHOD NORMALIZED_CDC
#define NC_level 2
#define MAX_CHUNK_SIZE 64

uint64_t Mask_front;
uint64_t Mask_back;

int expected_chunk_size;
int chunk_num_list[MAX_CHUNK_SIZE] = {0};

void insertChunkLength(int chunkLength){
    for(int i=0; i<=MAX_CHUNK_SIZE-1; i++){
        if( i*1024 <= chunkLength && chunkLength <(i+1)*1024){
            chunk_num_list[i]++;
        }
    }
}

void printChunkLengthStatistic(){
    printf("-- chunk size distribution --\n");
    for(int i=0; i<=MAX_CHUNK_SIZE-1; i++)
        printf("%d ", chunk_num_list[i]);
    printf("\n");
}

void scanOneFile(const char* random_file_path){
    // codes below are just for testing
    FILE *random_file;
    size_t readStatus = 0;

    int chunk_num = 0;
    unsigned char *fileCache = (unsigned char *)malloc(CacheSize);
    memset(fileCache, 0, sizeof(CacheSize));

    int offset = 0, chunkLength = 0, readFlag = 0;
    fastCDC_init();

    random_file = fopen(random_file_path, "r+");
    if (random_file == NULL) {
        perror("Fail to open file");
        exit(-1);
    }

    fseek(random_file, 0, SEEK_END); // seek to end of file
    int file_size = ftell(random_file); // get current file pointer
    fseek(random_file, 0, SEEK_SET); 
    int end = file_size - 1;

    readStatus = fread(fileCache, 1, CacheSize, random_file);

    switch (USE_CHUNKING_METHOD)
    {
        case ORIGIN_CDC:
            chunking = GearCDC_OptimizedHashJudgement;
            printf("You choose the GearCDC with Optimized hash judgement.\n");
            printf("Gear mask = 0x%x\n", FING_GEAR_08KB_64);
            break;

        case ROLLING_2Bytes:
            chunking = rolling_data_2byes_64;
            break;
        
        case NORMALIZED_CDC:
            chunking = normalized_chunking_64;
            break;

        case NORMALIZED_2Bytes:
            chunking = normalized_chunking_2byes_64;
            break;
        
        default:
            break;
            //printf(stderr, "No implement other chunking method");
    }

    unsigned long long sum_chunk_length = 0;
    
    for (;;) {
        if((end - offset + 1) == 0){
            //printf("rest size is 0, last chunk size = %d\n", chunkLength);
            break;
        }
            
        gettimeofday(&tmStart, NULL);
        chunkLength = chunking(fileCache + offset, end - offset + 1); 
        gettimeofday(&tmEnd, NULL);

        totalTm += ((tmEnd.tv_sec - tmStart.tv_sec) * 1000000 + (tmEnd.tv_usec - tmStart.tv_usec));  //微秒

        insertChunkLength(chunkLength);
        chunk_num += 1;
        sum_chunk_length += chunkLength;
        if(chunkLength <= 1024 * 4) ++smalChkCnt;
        offset += chunkLength;

        if (CacheSize - offset < MaxSize) {
            printf("*-*-");
            memcpy(fileCache, fileCache + offset + 1, CacheSize - offset);
            readStatus = fread(fileCache + CacheSize - offset, 1, offset, random_file);
            end = CacheSize - 1 - offset + readStatus;

            if (readStatus < offset + 1) {
                // all the files are read
                readFlag = 1;
            }

            offset = 0;
        }

        if (offset >= end && readFlag == 1){
            printf("last chunk size = %d\n", chunkLength);
            break;
        }   
    }

    // printf("totalTime is %f ms\n", totalTm / 1000);
    // printf("chunknum is %d\n", chunk_num);
    // printf("sum chunk length is %lld\n", sum_chunk_length);
    // printf("small chunknum is %d\n", smalChkCnt);

    // clear the items
    free(fileCache);
}


int main(int argc, char** argv) {
    if(argc < 3){
        printf("You need 2 args\n");
        exit(-1);
    }

    // input directory
    char* directory_path = argv[1];
    DIR *dirp = opendir(directory_path);
    if(dirp == NULL){
        printf("Open dir error\n");
        exit(-1);
    }

    // expect chunk size
    expected_chunk_size = atoi(argv[2]) * 1024;
    printf("ECS:%d\n", expected_chunk_size);

    struct dirent *direntp;
    while((direntp=readdir(dirp))!=NULL){
        if(strcmp(direntp->d_name, "..") == 0 || strcmp(direntp->d_name, ".") == 0)
            continue;
        
        char dirbuf[512]; 
		memset(dirbuf, 0, sizeof(dirbuf)); 
		strcpy(dirbuf, argv[1]); 
		strcat(dirbuf, direntp->d_name);
        
        scanOneFile(dirbuf);
    }

    printChunkLengthStatistic();
    return 0;
}

// functions
void fastCDC_init() {
    // 64 bit init
    for (int i = 0; i < SymbolCount; i++) {
        LEARv2[i] = GEARv2[i] << 1;
    }

    MinSize = expected_chunk_size / 4;
    MaxSize = expected_chunk_size * 8;    
    Mask_15 = 0xf9070353;
    Mask_11 = 0xd9000353;
    Mask_11_64 = 0x0000d90003530000;
    Mask_15_64 = 0x0000f90703530000;
    MinSize_divide_by_2 = MinSize / 2;

    if(NC_level == 1){
        Mask_front = Mask14;
        Mask_back = Mask12;
    }
    else if(NC_level == 2){
        Mask_front = Mask15;
        Mask_back = Mask11;
    }
    else if(NC_level == 3){
        Mask_front = Mask16;
        Mask_back = Mask10;
    }
    else if(NC_level == 0)
        ;
    else
        printf("Invalid NC level: %d\n", NC_level);
}

int normalized_chunking_64(unsigned char *p, int n) {
    uint64_t fingerprint = 0, digest;
    int Mid = expected_chunk_size;
    MinSize = Mid / 4;
    MaxSize = Mid * 8;
    int i = MinSize;
    // the minimal subChunk Size.
    if (n <= MinSize)  
        return n;

    if (n > MaxSize)
        n = MaxSize;
    else if (n < Mid)
        Mid = n;

    while (i < Mid) {
        fingerprint = (fingerprint << 1) + (GEARv2[p[i]]);
        if ((!(fingerprint & Mask_front)))
            return i;
        i++;
    }

    while (i < n) {
        fingerprint = (fingerprint << 1) + (GEARv2[p[i]]);
        if ((!(fingerprint & Mask_back)))
            return i;
        i++;
    }
    return n;
}

int normalized_chunking_2byes_64(unsigned char *p, int n) {
    uint64_t fingerprint = 0, digest;
    MinSize = 6 * 1024;
    int i = MinSize / 2, Mid = 8 * 1024;

    // the minimal subChunk Size.
    if (n <= MinSize) 
        return n;

    if (n > MaxSize)
        n = MaxSize;
    else if (n < Mid)
        Mid = n;

    while (i < Mid / 2) {
        int a = i * 2;
        fingerprint = (fingerprint << 2) + (LEARv2[p[a]]);

        if ((!(fingerprint & FING_GEAR_32KB_ls_64))) {
            return a;
        }

        fingerprint += GEARv2 [p[a + 1]];  

        if ((!(fingerprint & FING_GEAR_32KB_64))) {
            return a + 1;
        }

        i++;
    }

    while (i < n / 2) {
        int a = i * 2;
        fingerprint = (fingerprint << 2) + (LEARv2[p[a]]);

        if ((!(fingerprint & FING_GEAR_02KB_ls_64))) {
            return a;
        }

        fingerprint += GEARv2[p[a + 1]];

        if ((!(fingerprint & FING_GEAR_02KB_64))) {
            return a + 1;
        }

        i++;
    }

    return n;
}

int rolling_data_2byes_64(unsigned char *p, int n) {
    uint64_t fingerprint = 0, digest;
    int i = MinSize_divide_by_2;

    // the minimal subChunk Size.
    if (n <= MinSize) 
        return n;

    if (n > MaxSize) 
        n = MaxSize;

    while (i < n / 2) {
        int a = i * 2;
        fingerprint = (fingerprint << 2) + (LEARv2[p[a]]);

        if ((!(fingerprint & FING_GEAR_08KB_ls_64))) {
            return a;
        }

        fingerprint += GEARv2[p[a + 1]];

        if ((!(fingerprint & FING_GEAR_08KB_64))) {
            return a + 1;
        }

        i++;
    }

    return n;
}

int GearCDC_OptimizedHashJudgement(unsigned char *p, int n) {
    uint64_t fingerprint = 0;

    int i = MinSize;

    if (n <= MinSize)  
        return n;

    if (n > MaxSize) 
        n = MaxSize;

    for ( ;i<n ;i++) {
        fingerprint = (fingerprint << 1) + (GEARv2[p[i]]);
        if ((!(fingerprint & FING_GEAR_08KB_64)))
            return i;
    }
    return n;
}