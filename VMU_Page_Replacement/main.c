#include <stdio.h>
#include <stdlib.h>

int allocateFrame();
char getAddressValue(int address);//return the byte value at the logical address
int getOffset(int address); //get the offset of the address
int getNumber(int address); //get the frame number of the address
void initTLB(int numberOfEntry); //initialize the TLB table
void initPageTable();  //init the page lookup table
void initPhysicalAddress(); //init an array of frames
int loadFromBackStore(int frameNum); //load the page into a new frame and return the frame number
int dumpFrame(); //LRU algorithm
int searchTLB(int pageNumber);
void insertTLB(int pageNumber, int frameNumber); //insert into the TLB using FIFO

struct frame{
    char value[256];
} *physicalAddress;

struct entry{
    int pageNumber;
    int frameNumber;
} *TLB;

const int tlbSize = 16;
const int frameAmount = 256; //if this value is lower than 256, a LRU page replacement algorithm is used.
const int pageAmount = 256; //the amount of page

const char *addressFile = "addresses.txt";
const char *backStorage = "BACKING_STORE.bin";

int frameAllocated = 0;
unsigned long age = 0; //the current age for the program
unsigned long *ageTable; //the table for LRU algorithm
int *frameTable; //map frame to page, its used for LRU algorithm

int headOfEntry;
int *pageTable;
int TLBHits;
int pageFaults;
int totalReference;

void initTLB(int numberOfEntry){
    int i;
    headOfEntry = 0;
    TLBHits = 0;
    TLB = malloc(numberOfEntry * sizeof(struct entry));
    for (i = 0; i < numberOfEntry; i++){
        insertTLB(-1, -1);
    }
}

int searchTLB(int pageNumber){
    int i;
    //TODO: maybe we should search from the head
    for(i = 0; i < tlbSize; i++){
        if (TLB[i].pageNumber == pageNumber) {
            TLBHits++;
            return TLB[i].frameNumber;
        }
    }
    return -1;
}

void insertTLB(int pageNumber, int frameNumber){
    TLB[headOfEntry].pageNumber = pageNumber;
    TLB[headOfEntry].frameNumber = frameNumber;

    headOfEntry += 1;
    headOfEntry %= tlbSize;
}

char getAddressValue(int address){
    int frameNumber;
    int pageNumber = getNumber(address);
    int offset = getOffset(address);
    int TLBMiss = 0;

    if((frameNumber = searchTLB(pageNumber)) == -1){
        frameNumber = pageTable[pageNumber];
        TLBMiss = 1;
    }

    char result;
    if(frameNumber == -1){
        //page fault!
        pageFaults++;
        frameNumber = loadFromBackStore(pageNumber);
        pageTable[pageNumber] = frameNumber;
    }

    if (TLBMiss) {
        insertTLB(pageNumber, frameNumber);
    }
    //update the age table for LRU algorithm
    ageTable[frameNumber] = age;
    age++;
    //look up on the physical address
    struct frame* currentFrame = &physicalAddress[frameNumber];
    result = (currentFrame->value)[offset];
    return result;
}

int dumpFrame(){
    //LRU algorithm is implemented here, it will be used once the physical space is full.
    //dump the oldest frame, and set the corresponding page table, as well as remove it from tlb table
    int i;
    int lowestAge = age;
    int lowestFrame = -1;
    for(i = 0; i < frameAmount; i++){
        if(ageTable[i] < lowestAge) {
            lowestAge = ageTable[i];
            lowestFrame = i;
        }
    }
//    printf("dumped frame: %d, age: %d\n" , lowestFrame, lowestAge);

    //remove it from TLB table
    for(i = 0; i < tlbSize; i++){
        if (TLB[i].frameNumber == lowestFrame){
            TLB[i].pageNumber = -1;
            TLB[i].frameNumber = -1;
        }
    }

    pageTable[frameTable[lowestFrame]] = -1;
    frameTable[lowestFrame] = -1;
    return lowestFrame;
}

int loadFromBackStore(int pageNum){
    //load the page into a new frame and return the frame number
    //if the physical address is full, use page replacement algorithm
    int i, newFrame;

    FILE* backingStore = fopen(backStorage, "rb");
    fseek(backingStore, pageNum << 8, SEEK_SET);

    newFrame = allocateFrame();
    frameTable[newFrame] = pageNum;
    for (i = 0; i < 256; i++) {
        physicalAddress[newFrame].value[i] = getc(backingStore);
    }
    fclose(backingStore);

    return newFrame;
}

void initPhysicalAddress(){
    int i;
    physicalAddress = malloc(frameAmount * sizeof(struct frame));
    ageTable = malloc(frameAmount * sizeof(unsigned long));
    for(i = 0; i < frameAmount; i++){
        ageTable[i] = 0;
    }
    frameTable = malloc(frameAmount * sizeof(int));
}

void initPageTable(){
    int i;
    pageFaults = 0;
    pageTable = malloc(pageAmount * sizeof(int));
    for(i = 0; i < pageAmount; i++){
        pageTable[i] = -1;//-1 means page fault
    }
}

int getNumber(int address){
    unsigned int result = address << 16;
    result >>= 24;
    return result;
}

int getOffset(int address){
    unsigned int result = address << 24;
    result >>= 24;
    return result;
}

int allocateFrame() {
    if(frameAllocated >= frameAmount){
//        printf("LRU used\n");
        return dumpFrame();
    } else{
        return frameAllocated++;
    }
}

int main() {
    int address, pageNumber, frameNumber, offset;
    char value;

    FILE* addresses = fopen(addressFile, "rb");
    if(!addresses) {
        perror("File opening failed");
        return 1;
    }
    initTLB(tlbSize);
    initPageTable();
    initPhysicalAddress();
    totalReference = 0;

    FILE* output = fopen("output.csv","w");
    while(fscanf(addresses, "%d", &address) == 1){
        totalReference++;
        value = getAddressValue(address);
        offset = getOffset(address);
        pageNumber = getNumber(address);
        frameNumber = searchTLB(pageNumber); //after reference, the page number must be in the TLB table
        TLBHits--;
//        if (address != (pageNumber << 8) + offset) printf("ERROR!\n");
//        if (((char *)physicalAddress)[(frameNumber << 8) + offset] != value) printf("ERROR!\n");
        fprintf(output, "%d,%d,%d\n", address, (frameNumber << 8) + offset, value);
    }
    fclose(output);

    printf("Page Faults rate: %d/%d = %.1f%%\n", pageFaults, totalReference, (float)pageFaults * 100/totalReference);
    printf("TLB Hits rate: %d/%d = %.1f%%", TLBHits, totalReference, (float)TLBHits * 100/totalReference);

    fclose(addresses);
}
