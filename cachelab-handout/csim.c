#include "cachelab.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>



char* t;
int s,E,b;
bool h,v;
int miss,hit,eviction;
int curTimeStamp;

typedef struct{
    bool valid;
    unsigned long long tag;
    int timeStamp;   
} cacheLine;

typedef struct{
    cacheLine* lines;
} cacheSet;

typedef struct{
    cacheSet* sets;
} cache;

cache* myCache;

void printUsage();
void parseCommandLine(int argc,char* argv[]);
cache* initCache(int s,int E);
void releaseMem(cache* c,char* t);
void processFile(char* t,cache* c);
void updateCache(char op,unsigned long long addr,cache* c );
void updateCacheSet(cacheSet* set,int matchTag,char op);
void lruUpdate(int matchTag, cacheSet* set,int times);

int main(int argc, char* argv[])
{
    //1. 获取输入参数。 -v 输出每次的输入信息。 -s 组索引  -E 行数 -b 字节站位 -t 文件地址
    //2.打开文件，获取输入  按行读入
    //3.处理每个输入，进行LRU数据入队,出队。
    //4.记录输出信息  miss hit evictions
    //
    parseCommandLine(argc,argv);
    myCache = initCache(s,E);
    processFile(t,myCache);
    releaseMem(myCache,t);
    printSummary(hit, miss, eviction);
    return 0;
}


void lruUpdate(int matchTag,cacheSet* set,int times){
    int isMiss= 0, isHit =0, isEvict =0;
    for (int k =0; k < times;k++){
        for (int i = 0;i < E;i++){
            cacheLine* line = set->lines+i;
            if (line->valid && matchTag == line->tag){
                isHit++;
                if (v) printf("%s ","hit ");
                line->timeStamp = curTimeStamp++;
                break;
            }
        }
        if (isHit) continue;
        isMiss++;
        if (v) printf("%s","miss ");
        bool isFilled = false;
        for (int i = 0;i < E; i++){
            cacheLine* line = set->lines + i;
            if (!line->valid){
                line->tag = matchTag;
                line->valid = true;
                line->timeStamp = curTimeStamp++;
                isFilled = true;
                break;
            }
        }
        if (isFilled) continue;
        isEvict++;
        if (v) printf("%s","eviction ");
        cacheLine* oldestLine;
        int minStamp = 0x7fffffff;
        for (int i =0;i<E;i++){
            cacheLine* line = set->lines+i;
            if (minStamp > line->timeStamp){
                minStamp = line->timeStamp;
                oldestLine = line;
            }
        }
        oldestLine->tag = matchTag;
        oldestLine->timeStamp = curTimeStamp++;
    }
    if (v) printf("\n");
    hit+=isHit;
    miss+=isMiss;
    eviction += isEvict;
}

void updateCacheSet(cacheSet* set,int matchTag,char op){
    switch(op){
        case 'L':
            lruUpdate(matchTag,set,1);
            break;
        case 'M':
            lruUpdate(matchTag,set,2);
            break;
        case 'S':
            lruUpdate(matchTag,set,1);
            break;
        default:
            printf("Invalid op %c\n",op);
            exit(-1);
    }   
}

void updateCache(char op,unsigned long long addr,cache* c){
    int matchTag = addr >> (b+s);
    int matchSetAddr = (addr >>b) & ((1<<s)-1);
    cacheSet* set = c->sets + matchSetAddr;
    updateCacheSet(set,matchTag,op);

}

void processFile(char* t,cache* c){
    FILE* fp = fopen(t,"r");
    if (NULL == fp){
        printf("%s:No such file or directory\n",t);
        exit(-1);
    }
    char buf[1024];
    char op;
    int size;
    unsigned long long addr;

    while(fgets(buf,1024,fp)){
        if (buf[0] == 'I') continue;
        if (sscanf(buf," %c %llx,%d",&op,&addr,&size) < 3){
            printf("%s:Invalid Argument %s",t,buf);
            exit(-1);
        }
        if (v) printf("%c %llx,%d",op,addr,size);
        updateCache(op,addr,c);
    }

    fclose(fp);
}

void releaseMem(cache* c,char* t){
    int S = 1<<s;
    for (int i =0;i < S;i++){
        cacheSet* set = c->sets + i;
        free(set->lines);
    }
    free(c->sets);
    free(c);
    free(t);
}

cache* initCache(int s,int E){
    int S = 1<<s;
    cache* c = (cache*) calloc(1,sizeof(cache));
    cacheSet* setArr = (cacheSet*) calloc(S,sizeof(cacheSet));
    c->sets = setArr;
    for (int i = 0; i<S;i++){
        (setArr+i)->lines = (cacheLine*)calloc(E,sizeof(cacheLine));
    }
    return c;
}

void parseCommandLine(int argc,char* argv[]){
    int opt;
    while (-1 != (opt = getopt(argc,argv,"hvs:E:b:t:"))){
        switch (opt){
            case 'h':
                h = true;
                break;
            case 'v':
                v = true;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                t = (char*) malloc(1000);
                strcpy(t,optarg);
                break;
            default:
                printUsage();
                break;
        }
    }
    if (h){
        printUsage();
        exit(0);
    }
    if (s<=0 || E<=0 || b <=0 || t ==NULL){
        printf("%s: Missing required command line argument\n", argv[0]);
        exit(-1);
    }
        
}

void printUsage() {
    printf(
        "Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n"
        "Options:\n"
        "  -h         Print this help message.\n"
        "  -v         Optional verbose flag.\n"
        "  -s <num>   Number of set index bits.\n"
        "  -E <num>   Number of lines per set.\n"
        "  -b <num>   Number of block offset bits.\n"
        "  -t <file>  Trace file.\n\n"
        "Examples:\n"
        "  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
        "  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}
