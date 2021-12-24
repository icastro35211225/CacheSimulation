#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

typedef struct cacheBlock
{
  int offset;
  int tag;
  int valid;
} cacheBlock;

typedef struct ptEntry
{
  int f_num;
  int vi;
} ptEntry;

int cacheSize, blockSize, assoc, accessCount=0, hitCount=0, compulsaryMissCount=0, conflictMissCount=0, cycleCount=0, instCount=0, CLK=0, faultCount=0;
int numRows, numBlocks, numUnusedBlocks=0, indexBits, offsetBits, tagBits, offsetMask=0, indexMask=0, pnum, dnum, fnum, x, y;
unsigned int length=0, inst=0, dest, source, cacheOffset, cacheIndex, cacheTag;
double overhead, ims, cost, hitRate, unusedKB, unusedPercent;
unsigned long long int physicalMemSize;
int* rrCount=NULL;
int* frames=NULL;
int* LRUcount=NULL;
int* revmap=NULL;
int* numFramesPerTrace=NULL;
char* replacePol=NULL;
char* physicalMem=NULL;
char* num=NULL;
char* buff=NULL;
char** fileName=NULL;
FILE* currfp=NULL;
ptEntry* pageTable=NULL;
cacheBlock** cache=NULL;

int numBitCalc(int size)
{
  int bitCount=0;
  while(size)
  {
    size>>=1;
    bitCount++;
  }
  return bitCount-1;
}

unsigned long long int getPowerOfTwo(int exponent)
{
  unsigned long long num = 1, i;
  for(i = 0; i < exponent; i++)
  {
    num *= 2;
  }
  return num;
}

unsigned int createMask(int bitLength)
{
  int i, mask=0, currBit=1;
  for(i=0; i<bitLength; i++)
  {
    mask = mask | currBit;
    currBit = currBit<<1;
  }
  return mask;
}

void caching()
{
  int i, hitFlag=0, writeFlag=0;
  for(i=0; i<assoc; i++)
  {
    if(cache[cacheIndex][i].valid==1 && cache[cacheIndex][i].tag==cacheTag)
    {
      hitCount++;
      cycleCount+=1;
      hitFlag=1;
      break;
    }
  }
  if(hitFlag==0)
  {
    for(i=0; i<assoc; i++)
    {
      if(cache[cacheIndex][i].valid==0)
      {
        cache[cacheIndex][i].valid=1;
        cache[cacheIndex][i].tag=cacheTag;
        cache[cacheIndex][i].offset=(cacheOffset - (cacheOffset%blockSize));
        compulsaryMissCount++;
        cycleCount+=(4*ceil(blockSize/4));
        writeFlag=1;
        break;
      }
    }
    if(writeFlag==0)
    {
      if(strcmp("RR", replacePol)==0)
      {
        cache[cacheIndex][rrCount[cacheIndex]].tag=cacheTag;
        cache[cacheIndex][rrCount[cacheIndex]].offset=(cacheOffset - (cacheOffset%blockSize));
        rrCount[cacheIndex]=(rrCount[cacheIndex]+1)%assoc;
        conflictMissCount++;
        cycleCount+=(4*ceil(blockSize/4));
      }
      if(strcmp("RND", replacePol)==0)
      {
                int randIndex=(rand() % assoc);
                cache[cacheIndex][randIndex].tag=cacheTag;
                cache[cacheIndex][randIndex].offset=(cacheOffset - (cacheOffset%blockSize));
                conflictMissCount++;
                cycleCount+=(4*ceil(blockSize/4));
            }
        }
    }
}

int findFreeFrame()
{
    int i;
    for(i=1; i<(physicalMemSize/4096); i++)
    {
        if(frames[i]==1)
        {
            frames[i]=0;
            return i;
        }
    }
    return 0;
}

int findVictimLRU()
{
    int min = LRUcount[1];
    int mindex, i;
    for(i=1; i<(physicalMemSize/4096); i++)
    {
        if(LRUcount[i] <= min)
        {
            min = LRUcount[i];
            mindex = i;
        }
    }
    return mindex;
}

void pageMemory(unsigned int currAddress)
{
    CLK++;
    pnum = currAddress >> 12;
    dnum = currAddress & 0x0FFF;
    if(pageTable[pnum].vi==1)
    {
        fnum = pageTable[pnum].f_num;
        LRUcount[fnum]=CLK;
    }
    else
    {
        x = findFreeFrame();
        frames[x]=0;
        if(x>0)
        {
            pageTable[pnum].f_num = x;
            pageTable[pnum].vi = 1;
            fnum = pageTable[pnum].f_num;
            revmap[x]=pnum;
            LRUcount[fnum]=CLK;
            faultCount++;
        }
        else
        {
            y = findVictimLRU();
            pageTable[revmap[y]].vi = 0;
            pageTable[pnum].f_num = y;
            pageTable[pnum].vi = 1;
            fnum = pageTable[pnum].f_num;
            revmap[fnum]=pnum;
            LRUcount[fnum] = CLK;
            faultCount++;
        }
    }
}

int main(int argc, char** argv)
{
    srand(time(NULL));
    int j = 0, i = 0, fileCount = 0;
    //Get the number of files
    for(i = 0; i < argc; i++)
    {
        if(strcmp(argv[i], "-f") == 0)
            fileCount++;
    }
    if(fileCount<1 || fileCount>3)
    {
        fprintf(stderr, "ERROR: the provided number of files (%d) does not fall within the accepted range\n", fileCount);
        goto exitLabel;
    }
    //Malloc space to store elements
    fileName=(char**)malloc(fileCount*sizeof(char*));
    numFramesPerTrace=(int*)malloc(fileCount*sizeof(int));
    for(i=0; i<fileCount; i++)
    {
        fileName[i]=NULL;
        numFramesPerTrace[i]=0;
    }

    //Go through number of arguments and store data
    for(i=1; i<argc; i+=2) //easier than the while loop
    {
        if(strcmp(argv[i], "-f")==0)
        {
            fileName[j] = (char*)malloc((strlen(argv[i+1])+1)*sizeof(char));
            strcpy(fileName[j], argv[i+1]);
            j++;
        }
        else if(strcmp(argv[i], "-s")==0)
        {
            cacheSize = atoi(argv[i+1]);
            if(cacheSize<1 || cacheSize>8192)
            {
                fprintf(stderr, "ERROR: the provided cache size (%d) does not fall within the accepted range\n", cacheSize);
                goto exitLabel;
            }
        }
        else if(strcmp(argv[i], "-b")==0)
        {
            blockSize = atoi(argv[i+1]);
            if(blockSize<4 || blockSize>64)
            {
                fprintf(stderr, "ERROR: the provided block size (%d) does not fall within the accepted range\n", blockSize);
                goto exitLabel;
            }
        }
        else if(strcmp(argv[i], "-a")==0)
        {
            assoc = atoi(argv[i+1]);
            if(assoc!=1 && assoc!=2 && assoc!=4 && assoc!=8 && assoc!=16)
            {
                fprintf(stderr, "ERROR: the provided associativity (%d) is not supported\n", assoc);
                goto exitLabel;
            }
        }
        else if(strcmp(argv[i], "-r")==0)
        {
            replacePol = (char*)malloc((strlen(argv[i+1])+1)*sizeof(char));
            strcpy(replacePol, argv[i+1]);
            if(strcmp(replacePol, "RR")!=0 && strcmp(replacePol, "RND")!=0)
            {
                fprintf(stderr, "ERROR: the provided replacement policy (%s) is not supported\n", replacePol);
                goto exitLabel;
            }
        }
        else if(strcmp(argv[i], "-p")==0)
        {
            physicalMem = (char*)malloc((strlen(argv[i+1])+1)*sizeof(char));
            strcpy(physicalMem, argv[i+1]);
        }
    }
    //string physicalmem in bytes
    int len = strlen(physicalMem);
    char memSize = physicalMem[len - 2];
    num = (char*)malloc((len-1) * sizeof(char));
    int c = 0;
    while(c < len)
    {
        num[c] = physicalMem[c];
        if(c == len - 2)
        {
            break;
        }
        c++;
    }
    num[c] = '\0';
    if(memSize == 'K')
    {
        physicalMemSize = getPowerOfTwo(10) * atoi(num);
    }
    else if(memSize == 'M')
    {
        physicalMemSize = getPowerOfTwo(20) * atoi(num);
    }
    else if(memSize == 'G')
    {
        physicalMemSize = getPowerOfTwo(30) * atoi(num);
    }

    //print statements
    printf("Cache Simulator CS 3853 Fall 2021 - Group #03\n\n");
    for(i = 0; i < fileCount; i++)
    {
        printf("Trace File: %s\n", fileName[i]);
    }
    printf("\n***** Cache Input Parameters *****\n\n");
    printf("Cache Size:\t\t\t%d KB\n", cacheSize);
    printf("Block Size:\t\t\t%d bytes\n", blockSize);
    printf("Associativity:\t\t\t%d\n", assoc);
    printf("Replacement Policy:\t\t%s\n", replacePol);
    printf("Physical Memory Size:\t\t%llu MB (%llu bytes)\n", physicalMemSize/1048576, physicalMemSize);

    numRows=(cacheSize*1024)/(blockSize*assoc);
    numBlocks=numRows*assoc;
    indexBits=numBitCalc(numRows);
    offsetBits=numBitCalc(blockSize);
    tagBits=32-(indexBits+offsetBits);
    overhead=(numBlocks*(tagBits+1))/8;
    ims = (double)(overhead/1024 + cacheSize);
    cost = .15 * ims;
    printf("\n***** Cache Calculated Values *****\n\n");
    printf("Total # Blocks:\t\t\t%d\n", numBlocks);
    printf("Tag Size:\t\t\t%d bits\n", tagBits);
    printf("Index Size:\t\t\t%d bits\n", indexBits);
    printf("Total # Rows:\t\t\t%d\n", numRows);
    printf("Overhead Size:\t\t\t%.0lf bytes\n", overhead);
    printf("Implementation Memory Size:\t%.2lf KB (%d)\n", ims, (int)(overhead + cacheSize*1024));
    printf("Cost:\t\t\t\t$%0.2lf\n", cost);

    //dynamically create cache and set each block to invalid
    cache = (cacheBlock**)malloc(numRows * sizeof(cacheBlock*));
    for(i=0; i<numRows; i++)
    {
        cache[i] = (cacheBlock*)malloc(assoc * sizeof(cacheBlock));
        for(j=0; j<assoc; j++)
        {
            cache[i][j].valid = 0;
            cache[i][j].tag = 0;
            cache[i][j].offset = 0;
        }
    }

    //dynamically create frames and set each frame to free
    frames = (int*)malloc((physicalMemSize/4096) * sizeof(int));
    LRUcount = (int*)malloc((physicalMemSize/4096) * sizeof(int));
    revmap = (int*)malloc((physicalMemSize/4096) * sizeof(int));
    for(i=0; i<(physicalMemSize/4096); i++)
    {
        if(i==0)
        {
            frames[i]=0;
        }
        else
        {
            frames[i]=1;
        }
        LRUcount[i]=0;
        revmap[i]=-1;
    }

    //create the page table, since the virtual address space is always 32 bits and page is always 4KB it is not necessarily dynamic
    pageTable = (ptEntry*)malloc(1048576 * sizeof(ptEntry));
    for(i=0; i<1048576; i++)
    {
        pageTable[i].vi=0;
    }

    if(strcmp("RR", replacePol)==0)
    {
        rrCount=(int*)malloc(numRows * sizeof(int));
        for(i=0; i<numRows; i++)
        {
            rrCount[i]=0;
        }
    }
    offsetMask=createMask(offsetBits);
    indexMask=createMask(indexBits);
    buff=(char*)malloc(1000*sizeof(char));
    for(i=0; i<fileCount; i++)
    {
        currfp=fopen(fileName[i], "r");
        while(!feof(currfp))
        {
            fgets(buff, 1000, currfp);
            if(buff[0]!='E')
            {
                break;
            }
            sscanf(buff, "EIP (%x): %x %*c", &length, &inst);
            instCount++;
            accessCount++;
            pageMemory(inst);
            cacheOffset=inst & offsetMask;
            cacheIndex=(inst>>offsetBits) & indexMask;
            cacheTag=inst>>(indexBits + offsetBits);
            caching();
            if((cacheOffset+length)>blockSize)
            {
                accessCount++;
                cacheIndex++;
                cacheOffset=cacheOffset+length;
                if(cacheIndex >= numRows)
                {
                    cacheIndex = cacheIndex % numRows;
                    cacheTag+=1;
                }
                caching();
            }
            cycleCount+=2;
            fgets(buff, 1000, currfp);
            if(buff[0]!='d')
            {
                break;
            }
            sscanf(buff, "dstM: %x %*8c    srcM: %x %*c", &dest, &source);
            if(dest>0)
            {
                accessCount++;
                pageMemory(dest);
                cacheOffset=dest & offsetMask;
                cacheIndex=(dest>>offsetBits) & indexMask;
                cacheTag=dest>>(indexBits + offsetBits);
                length = 4;
                caching();
                if((cacheOffset+4)>blockSize)
                {
                    accessCount++;
                    cacheIndex++;
                    cacheOffset=cacheOffset+length;
                    if(cacheIndex >= numRows)
                    {
                        cacheIndex = cacheIndex % numRows;
                        cacheTag+=1;
                    }
                    caching();
                }
                cycleCount+=1;
            }
            if(source>0)
            {
                accessCount++;
                pageMemory(dest);
                cacheOffset=source & offsetMask;
                cacheIndex=(source>>offsetBits) & indexMask;
                cacheTag=source>>(indexBits + offsetBits);
                length=4;
                caching();
                if((cacheOffset+4)>blockSize)
                {
                    accessCount++;
                    cacheIndex++;
                    cacheOffset=cacheOffset+length;
                    if(cacheIndex >= numRows)
                    {
                        cacheIndex = cacheIndex % numRows;
                        cacheTag+=1;
                    }
                    caching();
                }
                cycleCount+=1;
            }
            fgets(buff, 1000, currfp);
        }
        numFramesPerTrace[i]=faultCount;
        fclose(currfp);
    }

    hitRate=((((double)hitCount)*100)/((double)accessCount));
    unusedKB=( ((numBlocks-compulsaryMissCount)*(blockSize+(double)((tagBits + 1)/8))) / 1024);
    unusedPercent=(unusedKB/ims)*100;
    numUnusedBlocks=numBlocks-compulsaryMissCount;

    printf("\n***** Cache Simulation Results *****\n\n");
    printf("Total Cache Accesses:\t\t%d\n", accessCount);
    printf("Cache Hits:\t\t\t%d\n", hitCount);
    printf("Cache Misses:\t\t\t%d\n", compulsaryMissCount + conflictMissCount);
    printf("--- Compulsary Misses:\t\t%d\n", compulsaryMissCount);
    printf("--- Conflict Misses:\t\t%d\n", conflictMissCount);
    printf("\n***** ***** Cache Hit & Miss Rate ***** *****\n\n");
    printf("Hit Rate:\t\t\t%.2lf%%\n", hitRate);
    printf("Miss Rate:\t\t\t%.2lf%%\n", 100-hitRate);
    printf("CPI:\t\t\t\t%lf\n\n", (double)cycleCount/(double)instCount);
    printf("Unused Cache Space:\t\t%.2lf KB / %.2lf KB = %lf%%\n", unusedKB, ims, unusedPercent);
    printf("Waste:\t\t\t\t$%.2lf\n", 0.15*unusedKB);
    printf("Unused Cache Blocks:\t\t%d / %d\n", numUnusedBlocks, numBlocks);

    for(i=fileCount-1; i>0; i--)
    {
        numFramesPerTrace[i] = numFramesPerTrace[i] - numFramesPerTrace[i-1];
    }
    printf("\n***** Virtual Memory Results *****\n\n");
    printf("Total Page Faults:\t\t%d\n", faultCount);
    for(i=0; i<fileCount; i++)
    {
        printf("%s used %d frames resulting in a use of %d KB of physical memory\n", fileName[i], numFramesPerTrace[i], numFramesPerTrace[i]*4);
    }

    //Free malloced memory
    exitLabel:
    if(fileName!=NULL)
    {
        for(i = 0; i < fileCount; i++)
        {
            if(fileName[i]!=NULL)
            {
                free(fileName[i]);
            }
        }
        free(fileName);
    }
    if(replacePol!=NULL)
    {
        free(replacePol);
    }
    if(physicalMem!=NULL)
    {
        free(physicalMem);
    }
    if(num!=NULL)
    {
        free(num);
    }
    if(buff!=NULL)
    {
        free(buff);
    }
    if(cache!=NULL)
    {
        for(i=0; i<numRows; i++)
        {
            free(cache[i]);
        }
        free(cache);
    }
    if(rrCount!=NULL)
    {
        free(rrCount);
    }
    if(frames!=NULL)
    {
        free(frames);
    }
    if(numFramesPerTrace!=NULL)
    {
        free(numFramesPerTrace);
    }
    if(LRUcount!=NULL)
    {
        free(LRUcount);
    }
    if(revmap!=NULL)
    {
        free(revmap);
    }
    if(pageTable!=NULL)
    {
        free(pageTable);
    }
    return 0;
}
