/*
 * C语言串口通信，自动连接设备
 */
/* 配置
@*
/dev/ttyUSB 5
/dev/ttyS 5
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include <termios.h> //set baud rate

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>

// 读取配置文件最大行数
#define MAXLINEN 10
// 最多支持扫描的类型数
#define MAXDEVICE 6
// 开辟内存空间
#define CHARARRAY(LEN) (char *)calloc(LEN, sizeof(char))
#define DEVICENUMARRAY(LEN) (DeviceNumP) calloc(LEN, sizeof(DeviceNum))

//接受数据容器大小
#define buffLen 1024
// 延时
#define rcvTimeOut 200

// 遍历时用的串口名和数量
// eg: /dev/ttyUSB 5
typedef struct deviceNum
{
    char Name[30];
    int Num;
} DeviceNum, *DeviceNumP;

// 配置文件名
static char *configName = "status";
// 文件第一行内容 核对的信息行
char *PASSWDSTR[MAXLINEN] = {NULL};
// 配置文件设备类型， 扫描数量
// eg: /dev/ttyUSB 5
DeviceNumP DEVICEN[MAXDEVICE] = {NULL};

// 波特率
static int BPS;
// 串口名
static char DEVNAME[50];

// 命令行解析
static const struct option long_option[] = {
    {"send", required_argument, NULL, 's'},
    {NULL, 0, NULL, 0}};

/*
自动扫描串口设备
连接上设备后，发送信息验证返回信息，确保连接正确。
passwd 指针数组
位置信息：
    0 要发送的信息
    1 核对的信息
返回值为 fdSerial 错误返回 0
*/
int autoMaticAddressing(char **passwd);

/*
设置串口信息
fdSerial 串口文件描述符
pbs 波特率
*/
int setSerialOpt(int fdSerial, int pbs);

/*
核对连接上的串口设备是正确的设备
fdSerial 串口文件描述符
send 要发送的信息
receive 核对的信息
*/
int checkDevice(int fdSerial, char *send, char *receive);

/*
调试串口使用
fdSerial 串口文件描述符
*/
void DEBUG(int fdSerial);

/*
输出已连接上设备的波特率
*/
void putBaudrate();

/*
读取本地文件中的配置信息
*/
void getConfigFile();

/*
释放指针数组空间
*/
void freeCharArray(char **array);
/*
释放指针数组空间
*/
void freeDeviceNumArray(DeviceNum **array);
/*
删除换行符 \r \n
*/
void delChangeLineChar(char *Line);
/*-----------------------------------------------------*/
/*******************************************************/

/*************Linux and Serial Port *********************/
/*************Linux and Serial Port *********************/
int openPort(char *devName)
{
    int fd = 0;

    fd = open(devName, O_RDWR | O_NOCTTY | O_NDELAY);
    if (-1 == fd)
    {
        // perror("Can't Open Serial Port");
        return (0);
    }

    // 阻塞文件
    if (fcntl(fd, F_SETFL, 0) < 0)
    {
        //printf("fcntl failed!\n");
    }
    else
    {
        fcntl(fd, F_SETFL, 0);
        //printf("fcntl=%d\n", fcntl(fd, F_SETFL, 0));
    }

    /* 检查是否为设备
    if (isatty(STDIN_FILENO) == 0)  
    {  
        printf("standard input is not a terminal device\n");  
    }  
    else  
    {  
        printf("is a tty success!\n");  
    }  
    //printf("fd-open=%d\n", fd);  
    */

    strcpy(DEVNAME, devName);
    return fd;
}

int setOpt(int fd, int nSpeed, int nBits, char nEvent, int nStop)
{
    struct termios newtio, oldtio;
    if (tcgetattr(fd, &oldtio) != 0)
    {
        //perror("SetupSerial 1");
        return -1;
    }
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag |= CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;

    switch (nBits)
    {
    case 7:
        newtio.c_cflag |= CS7;
        break;
    case 8:
        newtio.c_cflag |= CS8;
        break;
    }

    switch (nEvent)
    {
    case 'O': //奇校验
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= PARODD;
        newtio.c_iflag |= (INPCK | ISTRIP);
        break;
    case 'E': //偶校验
        newtio.c_iflag |= (INPCK | ISTRIP);
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        break;
    case 'N': //无校验
        newtio.c_cflag &= ~PARENB;
        break;
    }

    switch (nSpeed)
    {
    case 38400:
        cfsetispeed(&newtio, B38400);
        cfsetospeed(&newtio, B38400);
        BPS = 38400;
        break;
    case 921600:
        cfsetispeed(&newtio, B921600);
        cfsetospeed(&newtio, B921600);
        BPS = 921600;
        break;
    case 115200:
        BPS = 115200;
        cfsetispeed(&newtio, B115200);
        cfsetospeed(&newtio, B115200);
        break;
    default:
        BPS = 38400;
        cfsetispeed(&newtio, B38400);
        cfsetospeed(&newtio, B38400);
        break;
    }
    if (nStop == 1)
    {
        newtio.c_cflag &= ~CSTOPB;
    }
    else if (nStop == 2)
    {
        newtio.c_cflag |= CSTOPB;
    }
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;
    tcflush(fd, TCIFLUSH);
    if ((tcsetattr(fd, TCSANOW, &newtio)) != 0)
    {
        //perror("com set error");
        return -1;
    }
    //printf("---------- set done! ----------\n");
    return 0;
}

int readDataTty(int fd, char *rcv_buf, int TimeOut, int Len)
{
    int retval;
    fd_set rfds;
    struct timeval tv;
    int ret, pos;
    tv.tv_sec = TimeOut / 1000;         //set the rcv wait time
    tv.tv_usec = TimeOut % 1000 * 1000; //100000us = 0.1s

    pos = 0;
    while (1)
    {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        retval = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (retval == -1)
        {
            //perror("select check is it readable error!");
            break;
        }
        else if (retval)
        {
            ret = read(fd, rcv_buf + pos, 1);
            if (-1 == ret)
            {
                break;
            }

            pos++;
            if (Len <= pos)
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    return pos;
}

int sendDataTty(int fd, char *send_buf, int Len)
{
    ssize_t ret;

    ret = write(fd, send_buf, Len);
    if (ret == -1)
    {
        printf("write device error\n");
        return -1;
    }

    return 1;
}

int main(int argc, char **argv)
{
    char argvSend[30] = "";
    int opt = 0;
    while ((opt = getopt_long(argc, argv, "s:", long_option, NULL)) != -1)
    {
        switch (opt)
        {
        case 0:
            break;
        case 's':
            strcpy(argvSend, optarg);
        }
    }
    //printf("%s \n", argvSend);
    int fdSerial = 0;

    getConfigFile();
    char send[20] = "";
    char receive[20] = "";
    char *passwd[2] = {send, receive};
    send[0] = PASSWDSTR[0][0];
    receive[0] = PASSWDSTR[0][1];

    //openPort
    if ((fdSerial = autoMaticAddressing(passwd)) == 0)
    {
        printf("Failed to connect to serial device!\n");
        freeCharArray(PASSWDSTR);
        freeDeviceNumArray(DEVICEN);
        return 1;
    }

    //printf("Serial fdSerial=%d\n", fdSerial);

    tcflush(fdSerial, TCIOFLUSH); //清掉串口缓存
    fcntl(fdSerial, F_SETFL, 0);
    //DEBUG(fdSerial);

    char buffRcvData[buffLen];
    sendDataTty(fdSerial, argvSend, sizeof(argvSend));
    int readDataNum = 0;
    readDataNum = readDataTty(fdSerial, buffRcvData, rcvTimeOut, buffLen);
    close(fdSerial);

    // 释放申请的内存空间
    freeCharArray(PASSWDSTR);
    freeDeviceNumArray(DEVICEN);

    return 0;
}

// 自动寻址
int autoMaticAddressing(char **config)
{
    int fdSerial = 0;
    char devName[50];
    int max = 5;
    int i = 0;
    int devIndex = 0;
    int res = 0;

    for (devIndex = 0; devIndex < MAXDEVICE; devIndex++)
    {
        if (!DEVICEN[devIndex] ||
            strlen(DEVICEN[devIndex]->Name) < 2)
        {
            //printf("+++%s---\n", DEVICEN[devIndex]->Name);
            continue;
        }
        for (i = 0; i < DEVICEN[devIndex]->Num; i++)
        {
            sprintf(devName, "%s%d", DEVICEN[devIndex]->Name, i);
            // printf("---%s---\n", devName);
            fdSerial = openPort(devName);
            if (fdSerial != 0)
            {
                // printf("passwd:%s-%s-\n", config[0], config[1]);
                if (setSerialOpt(fdSerial, 38400) > 0 &&
                    checkDevice(fdSerial, config[0], config[1]))
                {
                    res = 1;
                    break;
                }
                else if (setSerialOpt(fdSerial, 921600) > 0 &&
                         checkDevice(fdSerial, config[0], config[1]))
                {
                    res = 1;
                    break;
                }
            }
        }
        if (res)
        {
            break;
        }
    }
    if (res)
    {
        putBaudrate();
    }
    else
    {
        fdSerial = 0;
    }
    return fdSerial;
}

// 设置串口信息
int setSerialOpt(int fdSerial, int pbs)
{

    if (setOpt(fdSerial, pbs, 8, 'N', 1) < 0)
    {
        // perror("set_opt error");
        return 0;
    }
    // printf("--- OK ---\n");
    return 1;
}

// 验证设备
int checkDevice(int fdSerial, char *send, char *receive)
{
    char receiveData[buffLen] = "";
    int readDataNum = 0;
    sendDataTty(fdSerial, send, sizeof(send));
    readDataNum = readDataTty(fdSerial, receiveData, rcvTimeOut, buffLen);
    //printf("%d == %s\n", readDataNum, receiveData);
    if (readDataNum == 1)
    {
        if (receive[0] == receiveData[0])
        {
            return 1;
        }
    }
    else if (readDataNum > 1)
    {
        if (strcmp(receiveData, receive) == 0)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

// 打印波特率
void putBaudrate()
{
    printf("--------------------\n");
    printf("Device   = %s\n", DEVNAME);
    printf("Baudrate = %d\n", BPS);
    printf("--------------------\n");
}

// 读取配置文件
void getConfigFile()
{
    FILE *fp = NULL;
    fp = fopen(configName, "r");
    int maxChar = 20;

    int line = 0;
    int line_1 = 0;
    DeviceNumP dev;
    do
    {
        //fseek(fp, SEEK_END, 0);
        if (line == 0)
        {
            PASSWDSTR[line] = CHARARRAY(maxChar);
            fgets(PASSWDSTR[line], maxChar, fp);
            delChangeLineChar(PASSWDSTR[line]);
            //printf("%se\n", PASSWDSTR[line]);
        }
        else
        {
            line_1 = line - 1;
            DEVICEN[line_1] = DEVICENUMARRAY(1);
            dev = DEVICEN[line_1];
            fscanf(fp, "%s %d", dev->Name, &dev->Num);
        }
        if (feof(fp))
        {
            break;
        }
    } while (++line < MAXLINEN);

    fclose(fp);
}

// DEBUG
void DEBUG(int fdSerial)
{
    char buffRcvData[buffLen] = {0};
    unsigned int readDataNum = 0;

    char c[10] = "";
    int sendNum = 10;
    while (1)
    {
        scanf("%s", c);
        printf("send: %s\n", c);
        sendDataTty(fdSerial, c, sendNum);
        printf("rec:      %s\n", buffRcvData);
        readDataNum = 0;
        readDataNum = readDataTty(fdSerial, buffRcvData, rcvTimeOut, buffLen);
        printf("recevie %d -----\n", readDataNum);
    }
}

//释放内存空间
void freeCharArray(char **array)
{
    char *line = NULL;
    int n = 0;
    for (n = 0; n < MAXLINEN; line = array[n], n++)
    {
        if (!line)
        {
            continue;
        }
        //printf("%d:  %s\n",n, line);
        free(line);
    }
}

//释放内存空间
void freeDeviceNumArray(DeviceNum **array)
{
    DeviceNumP line = NULL;
    int n = 0;
    for (n = 0; n < MAXDEVICE; line = array[n], n++)
    {
        if (!line)
        {
            continue;
        }
        //printf("%s:  %d\n",line->Name, line->Num);
        free(line);
    }
}
//删除换行符
void delChangeLineChar(char *Line)
{
    int Len = strlen(Line);
    int n = 0;
    for (n = 0; n < Len; n++)
    {
        if (Line[n] == 10 || Line[n] == 13)
        {
            Line[n] = '\0';
            break;
        }
    }
}