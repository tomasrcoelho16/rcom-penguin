// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

unsigned char* controlPacket(int control,const char* filename, long fileSize, int* ahm){
    long size = fileSize;
    int bytesRequired=0;
    while(fileSize>0){
        fileSize >>= 8;
        bytesRequired++;
    }
    unsigned char* packeto = (unsigned char*)malloc(2+3+strlen(filename)+bytesRequired);
    packeto[0] = control;
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
            int maxBytestoSend = MAX_PAYLOAD_SIZE-3;
            //OPENING FILE
            FILE *file = fopen(filename,"r");
            fseek(file, 0, SEEK_END);
            fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);
            if(llopen(layer) == -1) return;

            int size = 0;
            unsigned char* packeto = controlPacket(2,filename,fileSize, &size);
            if(llwrite(packeto,size)== -1) return;
            free(packeto);
            while(fileSize>0){ 
                if(fileSize<=maxBytestoSend){ 
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
            unsigned char* packetoFinal = controlPacket(3,filename,fileSize, &size);
            if(llwrite(packetoFinal,size)== -1) return;
            printf("DONE MFSSSSSSSSSSSSSSS\n");
            fclose(file); //adicionei isto ADUNNAODAODNAODNAOSDNAODN

            if(llclose(0) == -1) return;
            break;
        }
        case LlRx:
        {
            unsigned char packet[MAX_PAYLOAD_SIZE+1] = {0};

            if (llopen(layer) == -1){
            return;
            }

            int controlpacketSize = llread(packet);
            //packet[0] = control
            //packet[1] = dizer q é tamanho
            int i = 2;
            char fileSizeBytesNeeded = packet[2];
            i += fileSizeBytesNeeded;
            i++; //dizer q é o nome do ficheiro
            char fileNameBytesNeeded = packet[i];
            i++; // i está na posiçao que começa o fileName
            char* filenameReceived = (char*)malloc(fileNameBytesNeeded);
            memcpy(filenameReceived, packet+i, fileNameBytesNeeded);

            /*char* filename = (unsigned char*)malloc(fileNameBytesNeeded);
            //char a[4] = {'r','e','c','-'};

            //memcpy(filename, a, 4);
            memcpy(filename, packet+i, fileNameBytesNeeded);

            //unsigned char* lastPeriod = (unsigned char*) strrchr((unsigned char)packet, '.');
            if (lastPeriod == NULL){ // ficheiro sem extensao

            }
            else { // ficheiro normal
                char sizeUntilPeriod = lastPeriod - (packet+i);
                char sizeAfterPeriod = fileNameBytesNeeded - sizeUntilPeriod - 1;
                strncpy(filename, packet+i, sizeUntilPeriod);
                strcpy(filename + sizeUntilPeriod, "-received");
                strncpy(filename + sizeUntilPeriod + 9, packet+i+fileNameBytesNeeded-sizeAfterPeriod-1, sizeAfterPeriod+1);

            }

            // -received -> 9 bytes*/
            
            
            /*char* directory = "received/";
            char* fullFilePath;
            fullFilePath = (char*)malloc(strlen(directory) + fileNameBytesNeeded + 1);

            strcopy(fullFilePath, directory);
            strcat(fullFilePath, filenameReceived);*/
            


            FILE* file = fopen(filename, "wb+");
            if (file == NULL) {
                perror("error opening file");
                return ;
            }

            int llreadPacketCounter = 0;
            int packets=0;

            while ((llreadPacketCounter = llread(packet))){ //removi o llcloseDone
                if (packet[0] == 3){ printf("End of file. :)\n"); continue;}
                if (llreadPacketCounter == -1) return;
                fseek(file, packets, SEEK_SET);
                fwrite(packet+3,1, llreadPacketCounter-3, file);
                packets+= (llreadPacketCounter-3);
            }
            fclose(file);
            free(filenameReceived);

            break;
        }
    }
}


