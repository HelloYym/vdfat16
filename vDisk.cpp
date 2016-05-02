//============================================================================
// Name        : vDisk.cpp
// Author      : Yym
// Version     :
// Copyright   : Zhejiang University
// Description : Hello World in C++, Ansi-style
//============================================================================



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define SECTOR  512
#define FAT1    136
#define FAT2    192
#define DIR     248
#define DATA    280
#define DIRPERSECT  16
#define MYEOF   -1


typedef unsigned char Byte;
typedef unsigned short HalfWord;
typedef unsigned int Word;

    
FILE    *fp, *newfp, *filefp;


class Directory{
public:
    Byte    nam[8];
    char	ext[3];
    Byte    atr;
    Byte    r[10];
    HalfWord    tim,day;    
    HalfWord    p0;
    Word    size;   
} dir;


char    statement(){
    char opt;
    printf("\nlist all file...1\nadd file........2\nread file.......3\ndelete file.....4\nformat disk.....5\nexit............6\nplease choose a op:\n");
    scanf("%c",&opt);
    getchar();
    return opt; 
}

void    readSect(int n, Byte* buf){
    fseek(fp,n*SECTOR,0);
    fread(buf,512,1,fp);
    return;
}

void    writeSect(int n, Byte *buf){
    fseek(fp,n*SECTOR,0);
    fwrite(buf,512,1,fp);
    return;
}

HalfWord    read_halfword(Byte* s){
    HalfWord    num=0;
    Byte    first_byte, second_byte;
    first_byte=s[0];
    second_byte=s[1];
    num=second_byte<<8|first_byte;
    return num;
}

Word    read_Word(Byte* s){
    Word    num=0;
    Byte    byte[4];
    for (int i=0;i<4;i++)
        byte[i]=s[i];
    num=byte[3]<<24|byte[2]<<16|byte[1]<<8|byte[0];
    return num;
}


void    write_halfword(Byte* s,HalfWord num){
    Byte    first_byte, second_byte;
    first_byte=(num&0xFF00)>>8;
    second_byte=(num&0x00FF);
    s[0]=second_byte;
    s[1]=first_byte;
}

void    write_Word(Byte* s,Word num){
    Byte    byte[4];
    byte[0]=(num&0xFF000000)>>24;
    byte[1]=(num&0x00FF0000)>>16;
    byte[2]=(num&0x0000FF00)>>8;
    byte[3]=(num&0x000000FF);
    for(int i=0;i<4;i++)
        s[i]=byte[3-i];
}


//查找目录中文件
int    seekFILE(char* nam){
    Byte    buf[SECTOR];
    char flag;
    int index=-1;
    int i,j,t;
    for (int l=DIR;l<DATA;l++){
        readSect(l,buf);
        for (i=0;i<DIRPERSECT;i++){
            flag=1;
            for (j=0;j<8;j++)
                dir.nam[j]=*(buf+i*32+j);
            for (j=0;j<3;j++)
                dir.ext[j]=*(buf+i*32+0x8+j);
            dir.p0=read_halfword(buf+i*32+0x1A);
            dir.size=read_Word(buf+i*32+0x1C);

            for (j=0;nam[j]!='.'&&j<8&&nam[j]!=0;j++)
                if (nam[j]!=dir.nam[j]){
                    flag=0;
                    break;
                }
            if (dir.nam[j]!=0x20) flag=0;
            while (nam[j]!='.'&&nam[j]!=0) j++;
            if (nam[j]=='.')    j++;
            for (t=0;j<strlen(nam);j++,t++)
                if (nam[j]!=dir.ext[t]){
                    flag=0;
                    break;
                }
            if (t<3&&dir.ext[t]!=0x20) flag=0;
            if (!flag) continue;    
            index=(l-DIR)*DIRPERSECT+i;
            break;
        }
        if (index>=0) break;
    }
    return index;
}

void    list(){
    int i,j;
    bool have_file=false;
    Byte    buf[SECTOR];
    for (int l=DIR;l<DATA;l++){
        readSect(l,buf);
        for (i=0;i<DIRPERSECT;i++){
            for (j=0;j<8;j++)
                dir.nam[j]=*(buf+i*32+j);
            for (j=0;j<3;j++)
                dir.ext[j]=*(buf+i*32+0x8+j);
            dir.p0=read_halfword(buf+i*32+0x1A);
            dir.size=read_Word(buf+i*32+0x1C);

            //该位置没有文件或已删除
            if (dir.nam[0]==0||(Byte)dir.nam[0]==0xE5||dir.p0==0||dir.size==0) continue;

            for (j=0;j<8;j++)
                if (dir.nam[j]==0x20) break;
                else
                    printf("%c",dir.nam[j]);

            if (dir.ext[0]!=0x20){
                printf(".");
                for (j=0;j<3;j++)
                if (dir.ext[j]==0x20) break;
                else
                    printf("%c",dir.ext[j]);
            }

            printf("  size: %dByte  \n",dir.size);
            have_file=true;
        }
    }

    if (!have_file)
        printf("No file exist.");
    getchar();
    return;
}


void    setFat(HalfWord p,HalfWord pnext){
    Byte    buf[SECTOR];
    readSect(FAT1+p/256,buf);
    write_halfword(buf+p%256*2,pnext);
    writeSect(FAT1+p/256,buf);
    readSect(FAT2+p/256,buf);
    write_halfword(buf+p%256*2,pnext);
    writeSect(FAT2+p/256,buf);
    return;
}


HalfWord    readFAT(HalfWord p){
    Byte    buf[SECTOR];
    readSect(FAT1+p/256,buf);
    return  read_halfword(buf+p%256*2);
}

void    deleteSect(HalfWord p){
    HalfWord pnext=readFAT(p);
    setFat(p,0);

    if (pnext==0xFFFF)
        return;
    deleteSect(pnext);
}


void    deleteDir(int index){
    HalfWord p0;
    Byte    buf[SECTOR];
    readSect(DIR+index/DIRPERSECT,buf);
    p0=read_halfword(buf+index%DIRPERSECT*32+0x1A);
    for (int i=index%DIRPERSECT*32;i<index%DIRPERSECT*32+32;i++)
        buf[i]=0;
    writeSect(DIR+index/DIRPERSECT,buf);
    deleteSect(p0);
}

int     findnewdir(){
    Byte    buf[SECTOR];
    int i,j;
    for (int l=DIR;l<DATA;l++){
        readSect(l,buf);
        for (i=0;i<DIRPERSECT;i++){
            for (j=0;j<8;j++)
                dir.nam[j]=*(buf+i*32+j);
            for (j=0;j<3;j++)
                dir.ext[j]=*(buf+i*32+0x8+j);
            dir.p0=read_halfword(buf+i*32+0x1A);
            dir.size=read_Word(buf+i*32+0x1C);
            if (dir.nam[0]==0) 
                return (l-DIR)*DIRPERSECT+i;
        }
    }

    for (int l=DIR;l<DATA;l++){
        readSect(l,buf);
        for (i=0;i<DIRPERSECT;i++){
            for (j=0;j<8;j++)
                dir.nam[j]=*(buf+i*32+j);
            if (dir.nam[0]==0xE5) 
                deleteDir((l-DIR)*DIRPERSECT+i);
        }
    }

    for (int l=DIR;l<DATA;l++){
        readSect(l,buf);
        for (i=0;i<DIRPERSECT;i++){
            for (j=0;j<8;j++)
                dir.nam[j]=*(buf+i*32+j);
            for (j=0;j<3;j++)
                dir.ext[j]=*(buf+i*32+0x8+j);
            dir.p0=read_halfword(buf+i*32+0x1A);
            dir.size=read_Word(buf+i*32+0x1C);
            if (dir.nam[0]==0) 
                return (l-DIR)*DIRPERSECT+i;
        }
    }

    printf("can't find new dir.\n");
    return -1;
}

HalfWord    findnewsect(){
    Byte    buf[SECTOR];
    HalfWord newSect;
    int i,j;
    HalfWord fat;

    for (i=FAT1;i<FAT2;i++){
        readSect(i,buf);
        for(j=0;j<256;j++){
            fat=read_halfword(buf+j*2);
            if (fat==0) 
                return (i-FAT1)*256+j;
        } 
    }


    for (int l=DIR;l<DATA;l++){
        readSect(l,buf);
        for (i=0;i<DIRPERSECT;i++){
            for (j=0;j<8;j++)
                dir.nam[j]=*(buf+i*32+j);
            if (dir.nam[0]==0xE5) 
                deleteDir((l-DIR)*DIRPERSECT+i);
        }
    }

    for (i=FAT1;i<FAT2;i++){
        readSect(i,buf);
        for(j=0;j<256;j++){
            fat=read_halfword(buf+j*2);
            if (fat==0) 
                return (i-FAT1)*256+j;
        } 
    }

    printf("can't find new sector.\n");
    return 0;
}

Word get_time()
{
    HalfWord year,month,date,hour,minute,second;
    struct tm *local;
    time_t t;
    t=time(NULL);
    local=localtime(&t);

     year = local->tm_year + 1900;
     month = local->tm_mon+1;
     date = local->tm_mday;
     hour = local->tm_hour;
     minute = local->tm_min;
     second = local->tm_sec;

    return    (Word)((year - 1980) << 25)
                | (Word)(month << 21)
                | (Word)(date << 16)
                | (Word)(hour << 11)
                | (Word)(minute << 5)
                | (Word)(second >> 1);
}

void    writeDisk(){
    Byte    buf[SECTOR];
    char nam[10];
    char x=0;

    printf("please enter file name:\n");
    scanf("%s",nam);
    getchar();

    int index = seekFILE(nam);
   
    if (index>=0)
        deleteDir(index);


    if((filefp=fopen(nam,"rb"))==NULL){
        printf("The file does not exist.");
        getchar();
        return;
    }
    
    index=findnewdir();
    if (index<0) return;

    int i,j,t;

    int indexPosition=index%DIRPERSECT*32;
    
    readSect(DIR+index/DIRPERSECT,buf);

    //写入name到buf
    for (i=0;i<32;i++)
        buf[indexPosition+i]=0;
    for (i=0;i<0x0B;i++)
        buf[indexPosition+i]=0x20;

    j=0;
    while (nam[j]!='.'&&nam[j]!=0) j++;  
    for (i=0;i<j;i++)
        buf[indexPosition+i]=nam[i];

    //写入ext
    t=j;
    while (nam[j]!=0) j++;      
    if (j>t)
        for (i=8;i<8+j-t-1;i++)
            buf[indexPosition+i]=nam[t+1+i-8];

    buf[indexPosition+0x0B]=0x20;

    Word time = get_time();
    buf[indexPosition+0x16]=(Byte)(time>>24)&0xff;
    buf[indexPosition+0x17]=(Byte)(time>>16)&0xff;
    buf[indexPosition+0x18]=(Byte)(time>>8)&0xff;
    buf[indexPosition+0x19]=(Byte)(time&0xff);

    //写入首簇
    HalfWord newSect=findnewsect();        
    if (newSect==0) return;
    write_halfword(buf+indexPosition+0x1A,newSect);
        
    //写入文件长
    fseek(filefp,0L,SEEK_END);
    int size=ftell(filefp);
    write_Word(buf+indexPosition+0x1C,size);
    writeSect(DIR+index/DIRPERSECT,buf);

    //写入文件
    HalfWord p=newSect;
    HalfWord pnext;
    int fptr=0;

    fseek(filefp,0,0);
    while (1){
        if(fptr+512<=size){
            fread(buf,512,1,filefp);
            fptr+=512;
        } else {
            fread(buf,size-fptr,1,filefp);
            fptr=size;
            for (int i=size%512+1;i<512;i++)
                buf[i]=0;
        }
        writeSect(DATA+p-2,buf);


        if (fptr>=size){
            setFat(p,0xFFFF);
            printf("success.");
            getchar();
            break;
        }

        setFat(p,0xFFFF);
        pnext=findnewsect();
        if (pnext==0) return;

        setFat(p,pnext);
        p=pnext;
    }

    fclose(filefp);
}

void    format(){
    Byte    buf[SECTOR];

    for (int i=0;i<SECTOR;i++)
        buf[i]=0;

    readSect(DIR,buf);
    for (int i=32;i<SECTOR;i++)
        buf[i]=0;
    for (int i=DIR;i<DATA;i++){
        writeSect(i,buf);
        for (int i=0;i<SECTOR;i++)
        buf[i]=0;
    }

    for (int i=0;i<SECTOR;i++)
        buf[i]=0;

    for (int i=FAT1;i<FAT2;i++){
        if (i==FAT1){
            buf[0]=0xF8;
            buf[1]=0xFF;
            buf[2]=0xFF;
            buf[3]=0xFF;
        }
        writeSect(i,buf);
        for (int i=0;i<SECTOR;i++)
        buf[i]=0;
    }

    for (int i=FAT2;i<DIR;i++){
        if (i==FAT2){
            buf[0]=0xF8;
            buf[1]=0xFF;
            buf[2]=0xFF;
            buf[3]=0xFF;
        }
        writeSect(i,buf);
        for (int i=0;i<SECTOR;i++)
        buf[i]=0;
    }

    printf("success.");
    getchar();
    return;
}


void    readDisk(){

    Byte    buf[SECTOR];
    char nam[10];
    char root[20]="Folder\\";

    printf("please enter file name:\n");
    scanf("%s",nam);
    getchar();

    if (seekFILE(nam)<0){
        printf("The file does not exist.");
        getchar();
        return;
    }


    strcat(root,nam);
    newfp=fopen(root,"wb");

    HalfWord p=dir.p0;
    int fptr=0;

    while (1){
        readSect(DATA+p-2,buf);
        if(fptr+512<=dir.size){
            fwrite(buf,512,1,newfp);
            fptr+=512;
        } 
        else{
            fwrite(buf,dir.size-fptr,1,newfp);
            fptr=dir.size;  
            printf("success.");
            getchar();
            break;  
        }
        p=readFAT(p);
    }
    fclose(newfp);
}

void    deletefile(){
    Byte    buf[SECTOR];
    char nam[10];

    printf("input file name:\n");
    scanf("%s",nam);
    getchar();


    //查找目录
    int index = seekFILE(nam);
    
    if (index>=0) {
        int indexPosition=index%DIRPERSECT*32;
        readSect(DIR+index/DIRPERSECT,buf);
        buf[indexPosition]=0xE5;
        writeSect(DIR+index/DIRPERSECT,buf);
        printf("success.");
        getchar();
        return;
    }

    printf("The file does not exist.");
    getchar();
}


// 文件结构
typedef struct{
    short   bufsize;            //缓冲区大小
    unsigned char*  buffer;     //缓冲区首地址
    unsigned char*  ptr;        //buf中第一个未读地址
    unsigned int    curp;       //文件指针位置
    unsigned short  firstCluster;   //文件首簇
    unsigned short  curCluster;     //当前缓冲区内的簇号
    unsigned int    filesize;       //文件大小
}   myFILE;


myFILE* myfopen(char* nam);
bool    myfclose(myFILE* myfp);

void    myfseek(myFILE* myfp, int offset);
void    myrewind(myFILE* myfp);
bool    myfeof(myFILE* myfp);
int     myftell(myFILE* myfp);

char    myfgetc(myFILE* myfp);
char    myfputc(char ch, myFILE* myfp);
char    myfputs(char* s, myFILE* myfp);
char*   myfgets(char* s, int n, myFILE* myfp);
void    myfread(Byte* buffer, int size, int count, myFILE* myfp);
void    mywrite(Byte* buffer, int size, int count, myFILE* myfp);




myFILE* myfopen(char* nam){
    /*在虚拟磁盘中找到指定文件*/
    int index = seekFILE(nam);
    if (index==-1) return NULL;

    /*在内存中分配保存一个myFILE类型结构的单元*/
    myFILE* myfp = new myFILE;
    myfp->bufsize = 512;
    myfp->firstCluster = dir.p0;
    myfp->curCluster = dir.p0;
    myfp->filesize = dir.size;
    myfp->curp = 0;

    //在内存中分配文件缓冲区单元
    myfp->buffer = new Byte[512];
    myfp->ptr = myfp->buffer;
    readSect(DATA+myfp->firstCluster-2, myfp->buffer);

    //返回myFILE结构地址
    return  myfp;
}

void    myfseek(myFILE* myfp, int offset){
    writeSect(DATA+myfp->curCluster-2, myfp->buffer);
    HalfWord p = myfp->firstCluster;
    int fptr=0;
    while (true){
        if(fptr+512<=offset){
            fptr += 512;
        } 
        else{  
            fptr = offset;
            break;  
        }
        p=readFAT(p);
    }
    readSect(DATA+p-2, myfp->buffer);
    myfp->curCluster = p;
    myfp->curp = offset;
    myfp->ptr = &(myfp->buffer[offset%512]);
}

void    myrewind(myFILE* myfp){
    writeSect(DATA+myfp->curCluster-2, myfp->buffer);
    myfp->curCluster = myfp->firstCluster;
    readSect(DATA+myfp->curCluster-2, myfp->buffer);
    myfp->curp = 0;
    myfp->ptr = myfp->buffer;
}

bool    myfeof(myFILE* myfp){
    if (myfp->curp==myfp->filesize)
        return true;
    else
        return false;
}

int    myftell(myFILE* myfp){
    if (myfp == NULL)
        return -1;
    else
        return myfp->curp;
}

char    myfgetc(myFILE* myfp){
    if (myfeof(myfp))
        return EOF;
    char ch = *(myfp->ptr);
    myfp->ptr++;
    myfp->curp++;
    if (myfp->ptr - myfp->buffer == 512){
        writeSect(DATA+myfp->curCluster-2, myfp->buffer);
        myfp->curCluster = readFAT(myfp->curCluster);
        readSect(DATA+myfp->curCluster-2, myfp->buffer);
        myfp->ptr = myfp->buffer;
    }
    return ch;
}

char    myfputc(char ch, myFILE* myfp){
    if (myfeof(myfp))
        return EOF;
    *(myfp->ptr) = ch;
    myfp->ptr++;
    myfp->curp++;
    if (myfp->ptr - myfp->buffer == 512){
        writeSect(DATA+myfp->curCluster-2, myfp->buffer);
        myfp->curCluster = readFAT(myfp->curCluster);
        readSect(DATA+myfp->curCluster-2, myfp->buffer);
        myfp->ptr = myfp->buffer;
    }
    return ch;
}

char    myfputs(char* s, myFILE* myfp){
    if (myfeof(myfp))
        return EOF;
    int i;
    for (i=0; (s[i]!=0)&&(!myfeof(myfp)); i++){
        *(myfp->ptr) = s[i];
        myfp->ptr++;
        myfp->curp++;
        if (myfp->ptr - myfp->buffer == 512){
            writeSect(DATA+myfp->curCluster-2, myfp->buffer);
            myfp->curCluster = readFAT(myfp->curCluster);
            readSect(DATA+myfp->curCluster-2, myfp->buffer);
            myfp->ptr = myfp->buffer;
        }
    }
    return s[i-1];
}

char*    myfgets(char* s, int n, myFILE* myfp){
    if (myfeof(myfp))
        return NULL;
    for (int i=0; (i<n-1)&&(s[i]!='\n')&&(!myfeof(myfp)); i++){
        s[i] = *(myfp->ptr);
        myfp->ptr++;
        myfp->curp++;
        if (myfp->ptr - myfp->buffer == 512){
            writeSect(DATA+myfp->curCluster-2, myfp->buffer);
            myfp->curCluster = readFAT(myfp->curCluster);
            readSect(DATA+myfp->curCluster-2, myfp->buffer);
            myfp->ptr = myfp->buffer;
        }
    }
    s[n-1] = 0;
    return s;
}


void    myfread(Byte* buffer, int size, int count, myFILE* myfp){
    for (int i=0;(i<size*count)&&(!myfeof(myfp));i++){
        buffer[i] = *(myfp->ptr);
        myfp->ptr++;
        myfp->curp++;
        if (myfp->ptr - myfp->buffer == 512){
            writeSect(DATA+myfp->curCluster-2, myfp->buffer);
            myfp->curCluster = readFAT(myfp->curCluster);
            readSect(DATA+myfp->curCluster-2, myfp->buffer);
            myfp->ptr = myfp->buffer;
        }
    }
}

void    myfwrite(Byte* buffer, int size, int count, myFILE* myfp){
    for (int i=0;(i<size*count)&&(!myfeof(myfp));i++){
        *(myfp->ptr) = buffer[i];
        myfp->ptr++;
        myfp->curp++;
        if (myfp->ptr - myfp->buffer == 512){
            writeSect(DATA+myfp->curCluster-2, myfp->buffer);
            myfp->curCluster = readFAT(myfp->curCluster);
            readSect(DATA+myfp->curCluster-2, myfp->buffer);
            myfp->ptr = myfp->buffer;
        }
    }
}

bool    myfclose(myFILE* myfp){
    if (myfp == NULL) return false;
    writeSect(DATA+myfp->curCluster-2, myfp->buffer);
    delete [] myfp->buffer;
    delete myfp;
    return true;
}


int     main(){
    if((fp=fopen("vdisk.vhd","rb+"))==NULL){
        printf("Can't open disk.");
        getchar();
        return 0;
    }

    int t = true;
    while (t)
        switch(statement()){
            case '1':
                list();
                break;
            case '2':
                writeDisk();
                break;
            case '3':
                readDisk();
                break;
            case '4':
                deletefile();
                break;
            case '5':
                format();
                break;
            case '6':   
                t = false;
                //return 0;
                break;
            default:
                printf("wrong.");
                getchar();
                break;
        }

    /*
    myFILE* myfp;
    if ((myfp = myfopen("A.TXT")) == NULL){
        printf("File open error!\n");
        return 0;
    }
    char s[100] = "123456";
    //myfwrite(s,1,19,myfp);
    myfputs(s,myfp);
    myfclose(myfp);


    if ((myfp = myfopen("A.TXT")) == NULL){
        printf("File open error!\n");
        return 0;
    }
    
    char c[100];
    // int j=0;
    // while(!myfeof(myfp)){
    //     myfread(&c[j++],1,1,myfp);
    // }
    myfgets(c,15,myfp);

    printf("\n%s\n",c);
    myfclose(myfp);
*/



    fclose(fp);
    return 0;
}


