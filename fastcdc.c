#include "fastcdc.h"

// USE_CHUNKING_METHOD
#define USE_CHUNKING_METHOD 1

int chunk0_4 = 0, chunk4_8 = 0, chunk8_12 = 0;
int chunk12_16 = 0, chunk16_20 = 0, chunk20_24 = 0;
int chunk24_28 = 0, chunk28_32 = 0;
float chunk_summ = 0;
void insertChunkLength(int chunkLength){
    if(chunkLength <= 4*1024)
        chunk0_4++;
    if(4*1024 <= chunkLength && chunkLength <= 8*1024)
        chunk4_8++;
    if(8*1024 <= chunkLength && chunkLength <= 12*1024)
        chunk8_12++;
    if(12*1024 <= chunkLength && chunkLength <= 16*1024)
        chunk12_16++;
    if(16*1024 <= chunkLength && chunkLength <= 20*1024)
        chunk16_20++;
    if(20*1024 <= chunkLength && chunkLength <= 24*1024)
        chunk20_24++;
    if(24*1024 <= chunkLength && chunkLength <= 28*1024)
        chunk24_28++;
    if(28*1024 <= chunkLength && chunkLength <= 32*1024)
        chunk28_32++;
}

void printChunkLengthStatistic(){
    printf("-- chunk size distribution --");
    printf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f\n",
    chunk0_4/chunk_summ*100, chunk4_8/chunk_summ*100, chunk8_12/chunk_summ*100,
    chunk12_16/chunk_summ*100, chunk16_20/chunk_summ*100, chunk20_24/chunk_summ*100,
    chunk24_28/chunk_summ*100, chunk28_32/chunk_summ*100);
}

int main(int argc, char** argv) {
    
    // random file path 
    char* random_file_path = argv[1];

    // codes below are just for testing
    FILE *random_file;
    uint8_t SHA1_digest[20];
    uLong weakHash;
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
    printf("file size is %d\n", file_size);
    int end = file_size - 1;

    readStatus = fread(fileCache, 1, CacheSize, random_file);

    switch (USE_CHUNKING_METHOD)
    {
        case ORIGIN_CDC:
            chunking = cdc_origin_64;
            printf("gear mask = %x\n", FING_GEAR_08KB_64);
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
            printf("rest size is 0, last chunk size = %d\n", chunkLength);
            break;
        }
            
        gettimeofday(&tmStart, NULL);
        chunkLength = chunking(fileCache + offset, end - offset + 1); 
        //printf("%d\n", chunkLength);
        gettimeofday(&tmEnd, NULL);
        totalTm += ((tmEnd.tv_sec - tmStart.tv_sec) * 1000000 +
                    (tmEnd.tv_usec - tmStart.tv_usec));  //微秒
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

    printf("totalTime is %f ms\n", totalTm / 1000);
    printf("chunknum is %d\n", chunk_num);
    printf("sum chunk length is %lld\n", sum_chunk_length);
    printf("small chunknum is %d\n", smalChkCnt);

    chunk_summ = chunk_num;
    printChunkLengthStatistic();
    // clear the items
    free(fileCache);
    // fileCache = NULL;
    return 0;
}

// functions
void fastCDC_init(void) {
    unsigned char md5_digest[16];
    uint8_t seed[SeedLength];
    for (int i = 0; i < SymbolCount; i++) {

        for (int j = 0; j < SeedLength; j++) {
            seed[j] = i;
        }

        g_global_matrix[i] = 0;
        MD5(seed, SeedLength, md5_digest);
        memcpy(&(g_global_matrix[i]), md5_digest, 4);
        g_global_matrix_left[i] = g_global_matrix[i] << 1;
    }

    // 64 bit init
    for (int i = 0; i < SymbolCount; i++) {
        LEARv2[i] = GEARv2[i] << 1;
    }

    MinSize = 8192 / 4;
    MaxSize = 8192 * 4;    // 32768;
    Mask_15 = 0xf9070353;  //  15个1
    Mask_11 = 0xd9000353;  //  11个1
    Mask_11_64 = 0x0000d90003530000;
    Mask_15_64 = 0x0000f90703530000;
    MinSize_divide_by_2 = MinSize / 2;
}

int normalized_chunking_64(unsigned char *p, int n) {
    uint64_t fingerprint = 0, digest;
    MinSize = 6 * 1024;
    int i = MinSize, Mid = 8 * 1024;

    // the minimal subChunk Size.
    if (n <= MinSize)  
        return n;

    if (n > MaxSize)
        n = MaxSize;
    else if (n < Mid)
        Mid = n;

    while (i < Mid) {
        fingerprint = (fingerprint << 1) + (GEARv2[p[i]]);

        if ((!(fingerprint & FING_GEAR_32KB_64))) {// 11 1 bit
            return i;
        }

        i++;
    }

    while (i < n) {
        fingerprint = (fingerprint << 1) + (GEARv2[p[i]]);

        if ((!(fingerprint & FING_GEAR_02KB_64))) {// 15 1 bit
            return i;
        }

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

int cdc_origin_64(unsigned char *p, int n) {
    uint64_t fingerprint = 0, digest;
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
