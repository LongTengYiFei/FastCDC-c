#include <cstring>

int get_substring(const char* p, const char* search){
    const char* ans = strstr(p, search);
    if(ans == nullptr)
        return -1;
    return ans - p;
}

int pdf_chunking(const char *p, int n) {
    int pos_start = get_substring(p, "stream");
    if(p[pos_start+6] == 0x0D && p[pos_start+7] == 0x0A){
        return pos_start+8;
    }
    if(p[pos_start+6] == 0x0A){
        return pos_start+7;
    }

    int pos_end = get_substring(p, "endstream");
    if(pos_end == 1){
        return 10 + pdf_chunking(p+10, n-10);
    }else if(pos_end == 2){
        return 11 + pdf_chunking(p+11, n-11);
    }else{
        if(p[pos_end-2] == 0x0D && p[pos_end-1] == 0x0A){
            return pos_end-2;
        }
        if(p[pos_end-1] == 0x0D || p[pos_end-1] == 0x0A){
            return pos_end-1;
        }
    }
}

int office_chunking(unsigned char *p, int n){

}