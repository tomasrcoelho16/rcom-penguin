// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

unsigned char* controlPacket(const char* filename, long fileSize, int* ahm){
    long size = fileSize;
    int bytesRequired=0;
    while(fileSize>0){
        fileSize >>= 8;
        bytesRequired++;
    }
    unsigned char* packeto = (unsigned char*)malloc(2+3+strlen(filename)+bytesRequired);
    packeto[0] = 0x02;
    packeto[1] = 0;
    packeto[2] = bytesRequired;
    int i = 3;
    while(bytesRequired>0){
        packeto[i] = (size >> 8*(bytesRequired-1)) & 0xFF;
        i++;
        bytesRequired--;
        }
    packeto[i++] = 0x01;
    packeto[i++] = strlen(filename);
    for(int j = 0; j<strlen(filename);j++){
        packeto[i++] = filename[j];
    }
    *ahm = i-1;
    return packeto;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    long fileSize = 0;
    LinkLayerRole layerRole;
    if (strcmp(role, "tx") == 0){
        layerRole = LlTx;
    }
    else layerRole = LlRx;
    
    LinkLayer layer;
    
    int llcloseDone = FALSE;

    strcpy(layer.serialPort, serialPort);

   layer.role = layerRole;
   layer.baudRate = baudRate;
   layer.nRetransmissions = nTries;
   layer.timeout = timeout;
    
    switch (layerRole)
    {
        case LlTx:
        {
            int offset = 0;
            int maxBytestoSend = 997;
            //unsigned char packet = NULL;
            //OPENING FILE
            FILE *file = fopen(filename,"r");
            fseek(file, 0, SEEK_END);
            fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);
            if(llopen(layer) == -1) return;

            int size = 0;
            unsigned char* packeto = controlPacket(filename,fileSize, &size);
            if(llwrite(packeto,size)== -1) return;
            free(packeto);
            while(fileSize>0){ 
                if(fileSize<maxBytestoSend){ 
                    maxBytestoSend= fileSize;
                }
                unsigned char* packet = (unsigned char*)malloc(maxBytestoSend+3);
                packet[0] = 0x01;
                packet[1] = (maxBytestoSend >> 8) & 0xFF;
                packet[2] = maxBytestoSend & 0xFF; 
                fread(packet+3,1,maxBytestoSend,file);
                if(llwrite(packet,maxBytestoSend+3)== -1) break;
                fseek(file,maxBytestoSend,offset);
                fileSize-=maxBytestoSend;
                offset+=maxBytestoSend;
                free(packet);
            } 
            if(fileSize>0) return;
            printf("DONE MFSSSSSSSSSSSSSSS\n");
            fclose(file); //adicionei isto ADUNNAODAODNAODNAOSDNAODN

            if(llclose(0) == -1) return;
            break;
        }
        case LlRx:
        {
            if (llopen(layer) == -1){
            return;
            }

            const char* filename = "penguin-received.gif";

            FILE* file = fopen(filename, "wb+");
            if (file == NULL) {
                perror("error opening file");
                return 1;
            }

            unsigned char packet[MAX_PAYLOAD_SIZE+1] = {0};

            int llreadPacketCounter = 0;
            int packets=0;

            int whiledone = 0;

            while ((llreadPacketCounter = llread(packet))){ //removi o llcloseDone
                whiledone++;
                if (whiledone == 14) break;
                if (llreadPacketCounter == -1) return;

                fseek(file, packets, SEEK_SET);
            
                fwrite(packet+3,1, llreadPacketCounter-3, file);
                packets+= (llreadPacketCounter-3);
            }
            fclose(file);

            break;
        }
    }
}


