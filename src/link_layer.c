// Link layer protocol implementation

#include "link_layer.h"
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define BUF_SIZE 5
#define SET_SIZE 5
#define BAUDRATE B38400
#define flag 0x7E
#define adress 0x03

int llcloseDone = FALSE;
int alarmCount = 0;
int bytes;
int alarmEnabled = FALSE;
int timeout;
int tries;

typedef enum {
START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP_STATE, DADOS, VERIFICATION_7D, BCC1_OK, BCC2_CHECK, BCC2_RECEIVED, DISCONNECT
} SystemState;

SystemState state = START;

LinkLayerRole role;

int fd; 

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm %d\n", alarmCount);
}

int writeSU(int fd, unsigned char Address, unsigned char Control){
    unsigned char newBuf[BUF_SIZE + 1] = {0};
    newBuf[0] = 0x7E;
    newBuf[1] = Address;
    newBuf[2] = Control;
    newBuf[3] = Address ^ Control;
    newBuf[4] = 0x7E;
    return write(fd, newBuf, BUF_SIZE);
}
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{   
    printf("yo\n");
    fd = open( connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(connectionParameters.serialPort);
        return -1; 
    }

    struct termios oldtio;
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return -1;
    }

    tries = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    role = connectionParameters.role;
    switch(connectionParameters.role) {
        case LlTx:
        {
            unsigned char UA[SET_SIZE+1] = {0};
            unsigned char set[SET_SIZE+1] = {0};
            set[0] = flag;
            set[1] = adress;
            set[2] = 0x03;
            set[3] = set[1] ^ set[2];   
            set[4] = flag;
            
            SystemState state = START;

            (void)signal(SIGALRM, alarmHandler);    

            while(alarmCount<tries && state!=STOP_STATE){ 
                //printf("ishere\n");
                write(fd, set, SET_SIZE+1);
                alarm(timeout);
                alarmEnabled = TRUE;
                while (state != STOP_STATE && alarmEnabled==TRUE){
                    //printf("ishere\n");
                    bytes = read(fd, UA, 1);
                    if (UA[0] != 0x00) UA[bytes] = '\0';
                    switch(state){
                        case START:
                        {
                            if(UA[0]==flag) state = FLAG_RCV;
                            else state=START;
                            break;
                        }
                        case FLAG_RCV:
                        {
                            if (UA[0] == 0x03) state = A_RCV;
                            else if (UA[0] != 0x7E) state = START;
                            break;
                        }
                        case A_RCV:
                        {
                            if (UA[0] == 0x07) state = C_RCV;
                            else if(UA[0] == flag) state = FLAG_RCV;
                            else state = START;
                            break;
                        }
                        case C_RCV:
                        {
                            if (UA[0] == 0x03 ^ 0x07) state = BCC_OK;
                            else if(UA[0] == flag) state = FLAG_RCV;
                            else state = START;
                            break;
                        }
                        case BCC_OK:
                        {
                            if (UA[0] == 0x7E) state = STOP_STATE;
                            else if(UA[0] == flag) state = FLAG_RCV;
                            else state = START;
                            break;
                        }
                    }
                }
            }
            printf("something\n");
            if(state == START){ printf("Deu merda mermao!\n"); return -1;}
            else printf("cierto baby!\n");
            return 1;
            break;
            }
        case LlRx:
        {
                unsigned char buf[BUF_SIZE + 1] = {0};
                while (state != STOP_STATE){
                    bytes = read(fd, buf, 1);
                    switch(state){
                        case START:
                        {
                            if (buf[0] == 0x7E){
                                state = FLAG_RCV;
                            }
                        }
                        break;
                        
                        case FLAG_RCV:
                        {
                            //printf("CASE FLAG_RCV");
                            if (buf[0] == 0x03){
                                state = A_RCV;
                            }
                            else if (buf[0] != 0x7E) state = START;
                        }
                        break;
                        
                        case A_RCV:
                        {
                            if (buf[0] == 0x03){
                                state = C_RCV;
                            }
                            else if (buf[0] == 0x7E) {
                                state = FLAG_RCV;
                            }
                            else {
                                state = START;
                            }
                        }
                        break;
                        
                        case C_RCV:
                        {
                            if (buf[0] == (0x03 ^ 0x03)){
                                state = BCC_OK;
                            }
                            else {
                                if (buf[0] == 0x7E) {
                                    state = FLAG_RCV;
                                }
                                else {
                                    state = START;
                                }
                            }
                        }
                        break;
                        
                        case BCC_OK:
                        {
                            
                            if (buf[0] == 0x7E) {
                                printf("STOP STATE");
                                int bytesUA = writeSU(fd, 0x03, 0x07);  
                                printf("%d UA bytes written\n", bytesUA);
                                state = STOP_STATE;
                            }
                            else {
                                state = START;
                            }
                        }
                        break;
                    }
                }
                return 1;
            break;
            }
    }

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    int aMais = 0;
    int BCC2 = 0x00;

    for(int i = 0;i<bufSize;i++){
        if(buf[i] == 0x7E || buf[i] == 0x7D) aMais++;
        BCC2 = BCC2 ^ buf[i];
    }

    unsigned char packet[bufSize+aMais+6];
    packet[0]= flag;
    packet[1]= adress;
    packet[2]= 0x00; // frame control [0x40,0x00]
    packet[3]=packet[2] ^ packet[1]; 
    int i = 4;
    int b = 0;
    while(i<bufSize+aMais+4){
        if(buf[b] == 0x7E){
            packet[i++] = 0x7D;
            packet[i++] = 0x5E;
            b++;
        } else if (buf[b] == 0x7D) {
            packet[i++] = 0x7D;
            packet[i++] = 0x5D;
            b++;
        } else{ packet[i++] = buf[b++];}
    }
    packet[i++] = BCC2;
    packet[i] = flag; 

    sleep(1);

    unsigned char UA[SET_SIZE+1] = {0};
    SystemState state = START;
    unsigned char Control;
    alarmCount=0;
    while(alarmCount<tries && state != STOP_STATE){

        write(fd, packet, sizeof(packet));
        alarm(timeout);
        alarmEnabled = TRUE;

        while(state != STOP_STATE && alarmEnabled == TRUE){
            bytes = read(fd, UA, 1);
            if (UA[0] != 0x00) UA[bytes] = '\0';
            switch (state){
            case START:
                {
                    if(UA[0]==flag) state = FLAG_RCV;
                    break;
                    }
                case FLAG_RCV:
                {
                    if (UA[0] == 0x03) state = A_RCV;
                    else if (UA[0] != 0x7E) state = START;
                    break;
                    }
                case A_RCV:
                {
                    if (UA[0] == 0x81 || UA[0] == 0x01 || UA[0] == 0x85 || UA[0] == 0x05){ state = C_RCV; Control = UA[0]; }
                    else if(UA[0] == flag) state = FLAG_RCV;
                    else state = START;
                    break;
                }
                case C_RCV:
                {
                    if (UA[0] == 0x03 ^ Control) state = BCC_OK;
                    else if(UA[0] == flag) state = FLAG_RCV;
                    else state = START;
                    break;
                    }
                case BCC_OK:
                {
                    if(UA[0] == flag) state = STOP_STATE;
                    else state = START;
                    break;
                    }
            }
        }
        alarm(0);
        printf("LEZGO\n");  
        if((Control == 0x85 || Control == 0x05) && state == STOP_STATE){ printf("yauza\n"); return sizeof(packet);} //RR1 ou RR0, aceite
        else  state = START; //REJ0 ou REJ1, recusado
    }

    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
        state = START;
        unsigned char buf[BUF_SIZE + 1] = {0};
        unsigned char trama = 0;
        unsigned char controlByte = 0;
        unsigned char bcc2temp = 0;
        int packetCounter = 0;
        
        /*while (1){
            bytes = read(fd, buf, 1);
            buf[bytes] = '\0';
            if (buf[0] != 0x00)
                printf("var = 0x%02X\n", (unsigned int)(buf[0] & 0xFF));
        }

        return 0;*/

        while (state != STOP_STATE){
            bytes = read(fd, buf, 1);
            buf[bytes] = '\0';
            if (buf[0] != 0x00)
                printf("var = 0x%02X\n", (unsigned int)(buf[0] & 0xFF));
            switch(state){
                case START:
                {
                    if (buf[0] == 0x7E){
                        state = FLAG_RCV;
                        //printf("flag\n");
                    }
                    break;
                }
                case FLAG_RCV:
                {
                    if (buf[0] == 0x03){
                        state = A_RCV;
                        //printf("address\n");
                    }
                    else if (buf[0] != 0x7E) state = START;
                    break;
                }
                case A_RCV:
                {
                    if (buf[0] == 0x00 && trama == 0){
                        state = C_RCV;
                        //printf("control\n");
                        controlByte = buf[0];
                    }
                    else if (buf[0] == 0x40 && trama == 1) {
                        state = C_RCV;
                        controlByte = buf[0];
                    }
                    /*else if (buf[0] == 0x00 && trama = 1){
                    }*/
                    else if (buf[0] == 0x7E) state = FLAG_RCV;
                    else if (buf[0] == 0x0B){
                        //printf("DISCONNECT RECEIVED ON CONTROL");
                        state = DISCONNECT;
                        controlByte = buf[0];
                        //int bytesDisconnect = writeSU(fd,0x03, 0x0B);  
                        //printf("%d DISCONNECT bytes written\n", bytesDisconnect);
                        //return 0;
                    }
                    else {
                        state = START;
                    }
                    break;
                }
                case C_RCV:
                {
                    if (buf[0] == 0x03 ^ controlByte){
                        state = DADOS; //bcc1 check was ok
                    }
                    else if (buf[0] == 0x7E) {
                        state = FLAG_RCV;
                    }
                    else {
                        state = START;
                    }
                    break;
                }
                case DISCONNECT:
                    {
                    printf("disconnect case\n");
                    if (buf[0] == 0x7E){
                        state = STOP_STATE;
                        int llcloseOutput = llclose(0);
                        if (llcloseOutput == -1){
                            printf("error receiving UA response after Disconnect");
                        }
                        else{
                            printf("UA received correctly, shutting down...");
                        }
                        return 0;
                    }
                    break;
                    }

                case DADOS:
                {   
                    if(buf[0] != 0x7E){
                        if(buf[0]==0x7D){state = VERIFICATION_7D; break;} 
                        packet[packetCounter] = buf[0];
                        packetCounter++;
                    }
                    else {
                        unsigned char bcc2_byte = packet[packetCounter-1];
                        bcc2temp = packet[0];
                        printf("FIRST element BCC2: %X \n", bcc2temp); //0x02
                        for (int i = 1; i < packetCounter-1; i++){
                            printf("element %i : %X\n", i, packet[i]);
                            bcc2temp ^= packet[i];

                        }
                        printf("%X temp\n", bcc2temp);
                        printf("%X bcc2TOMASCOELHOHENRIQUEHENRIQUECOELHO\n", bcc2_byte);
                        if (bcc2temp != bcc2_byte){ // REJECTS
                            if (trama == 0){
                                int bytesReject = writeSU(fd, 0x03, 0x01); //REJECT TRAMA 0
                                printf("%d reject bytes written, TRAMA 0\n", bytesReject);
                                state = START;
                                return -1;
                            }
                            else if (trama == 1){
                                int bytesReject = writeSU(fd, 0x03, 0x81); //REJECT TRAMA 1
                                printf("%d reject bytes written, TRAMA 1\n", bytesReject);
                                state = START;
                                return -1;
                            }
                            // NAO SE MUDA A TRAMA
                        }

                        else {
                            // write RECEIVED POGGERS
                            state = STOP_STATE;
                            if (trama = 0){ // READY TO RECEIVE TRAMA 1, SEND RR1
                                int bytesReceived = writeSU(fd, 0x03, 0x85);
                                printf("%d received bytes written, ready for trama 1\n", bytesReceived);
                            }
                            else{ // READY TO RECEIVE TRAMA 0, SEND RR0
                                int bytesReceived = writeSU(fd, 0x03, 0x05);
                                printf("%d received bytes written, ready for trama 1\n", bytesReceived);
                            }
                            if (trama == 0) trama = 1;
                            else if (trama == 1) trama = 0;
                            // mudar trama de 0 para 1 ou vice versa
                            return packetCounter;
                        }
                    }
                break;
                }
                case VERIFICATION_7D:
                {
                    //destuffing do 7D e 7E

                    if (buf[0] == 0x5E){
                        packet[packetCounter] = 0x7E;
                        packetCounter++;
                        state = DADOS;
                        break;
                    }
                    else if (buf[0] == 0x5D){
                        packet[packetCounter] = 0x7D;
                        packetCounter++;
                        state = DADOS;
                        break;
                    }
                    /*else if (buf[0] == 0x7E){  //caso especifico BCC2 = 0x7D e FLAG LOGO A SEGUIR (0x7E)
                        //BCC2_RECEIVED = 0x7D; // CASE WHEN BCC2_RECEIVED IS 0X7D
                        state = BCC2_CHECK;
                        if (trama = 0){ // READY TO RECEIVE TRAMA 1, SEND RR1
                                int bytesReceived = writeSU(fd, 0x03, 0x85);
                                printf("%d received bytes written, ready for trama 1\n", bytesReceived);
                            }
                        else{ // READY TO RECEIVE TRAMA 0, SEND RR0
                            int bytesReceived = writeSU(fd, 0x03, 0x05);
                            printf("%d received bytes written, ready for trama 1\n", bytesReceived);
                        }
                        return packetCounter;
                    }*/
                    else state = START; // se nao entrou em nenhum dos casos, é porque algo está mal, volta ao start
                break;
                }
            }
        }
    return 1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    switch (role){
        case LlRx:
        {
             printf("inside llclose\n");
            int discResponse = writeSU(fd, 0x03, 0x0B);
            printf("DISCONNECT WRITTEN\n");
            state = START;
            unsigned char buf[BUF_SIZE + 1] = {0};

            while (state != STOP_STATE){
                int bytes = read(fd, buf, 1);
                switch(state){
                    case START:
                    {
                        if (buf[0] == 0x7E){
                            state = FLAG_RCV;
                        }
                    }
                    break;
                    
                    case FLAG_RCV:
                    {
                        //printf("CASE FLAG_RCV");
                        if (buf[0] == 0x01){
                            state = A_RCV;
                        }
                        else if (buf[0] != 0x7E){
                            state = START; 
                            return -1;
                        }
                    }
                    break;
                    
                    case A_RCV:
                    {
                        if (buf[0] == 0x07){
                            state = C_RCV;
                        }
                        else if (buf[0] == 0x7E) {
                            state = FLAG_RCV;
                        }
                        else {
                            state = START;
                            return -1;
                        }
                    }
                    break;
                    
                    case C_RCV:
                    {
                        if (buf[0] == 0x03 ^ 0x07){
                            state = BCC_OK;
                        }
                        else {
                            if (buf[0] == 0x7E) {
                                state = FLAG_RCV;
                            }
                            else {
                                state = START;
                                return -1;
                            }
                        }
                    }
                    break;
                    
                    case BCC_OK:
                    {
                        if (buf[0] == 0x7E) {
                            printf("UA bytes received correctly");
                            state = STOP_STATE;
                            llcloseDone = TRUE;
                        }
                        else {
                            state = START;
                            return -1;
                        }
                    }
                    break;
                }
            }
                return 1;
            break;
            }
        case LlTx:
        {
            alarmCount = 0;
            int Control = 0;
            unsigned char conf[SET_SIZE + 1] = {0};
            unsigned char dc[SET_SIZE + 1] = {0};
            unsigned char UA[SET_SIZE + 1] = {0};
            dc[0] = flag;
            dc[1] = 0x03;
            dc[2] = 0x0B;
            dc[3] = dc[1] ^ dc[2];
            dc[4] = flag;

            UA[0] = flag;
            UA[1] = 0x01;
            UA[2] = 0x07;
            UA[3] = UA[1] ^ UA[2];
            UA[4] = flag;

            SystemState state = START;

            while(alarmCount < tries && state!= STOP_STATE){

                write(fd, dc, SET_SIZE);
                alarm(timeout);
                alarmEnabled==TRUE;

                while(state!=STOP_STATE && alarmEnabled == TRUE){
                    bytes = read(fd, conf, 1);
                    if (conf[0] != 0x00) conf[bytes] = '\0';
                    switch (state){
                    case START:
                    {
                            if(conf[0] == flag) state = FLAG_RCV;
                            break;
                            }
                        case FLAG_RCV:
                        {
                            printf("flag of disc\n");
                            if (conf[0] == 0x03) state = A_RCV;
                            else if (conf[0] != 0x7E) state = START;
                            break;
                            }
                        
                        case A_RCV:
                        {
                            printf("address of disc\n");
                            if (conf[0] == 0x0B){ state = C_RCV; Control = conf[0]; }
                            else if(conf[0] == flag) state = FLAG_RCV;
                            else state = START;
                            break;
                            }
                        
                        case C_RCV:
                        {
                            printf("control of disc\n");
                            if (conf[0] == 0x03 ^ 0x0B) state = BCC_OK;
                            else if(conf[0] == flag) state = FLAG_RCV;
                            else state = START;
                            break;
                            }
                        case BCC_OK:
                        {
                            if(UA[0] == flag) state = STOP_STATE;
                            else state = START;
                            break;
                            }
                    }
                }
            }
            if(Control == 0x0B){ write(fd, UA, SET_SIZE); printf("disc rec\n"); return 1;}
            return -1;
            break;
            }
    }

    return 1;
}
