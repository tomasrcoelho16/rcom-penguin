// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
            int bytesSent = 0;
            int bytesLeft = 997;
            unsigned char packet[1000] = {0};
            packet[0] = 0x02;
            packet[1] = (997 >> 8) & 0xFF;
            packet[2] = 997 & 0xFF; 
            //OPENING FILE
            FILE *file = fopen(filename,"r");
            fseek(file, 0, SEEK_END);
            fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);
            if(llopen(layer) == -1) return;
            while(fileSize>0){ 
                if(fileSize<997){ 
                    bytesLeft= fileSize;
                    packet[1] = (bytesLeft >> 8) & 0xFF;
                    packet[2] = bytesLeft & 0xFF;
                }
                printf("ahm : %ld\n",fileSize);
                fread(packet+3,1,bytesLeft,file);
                if(llwrite(packet,sizeof(packet))== -1) break;
                fseek(file,bytesLeft,bytesSent);
                fileSize-=997;
                bytesSent+=bytesLeft;
            } 
            if(fileSize>0) return;
            //
            //ControlPacket 1
            //creatingPacket(2,filename,fileSize);
            /*while(fileSize!=0){
                //DATA 
                creatingPacket(1, filename);
            }
            //ControlPacker 3
            creatingPacket(3,);
            */

            fclose(file); //adicionei isto ADUNNAODAODNAODNAOSDNAODN

            if(llclose(0) == -1) return;
            break;
        }
        case LlRx:
        {
            if (llopen(layer) == -1){
            return;
            }
            printf("yoo\n");

            const char* filename = "penguin-received.gif";

            FILE* file = fopen(filename, "wb+");
            if (file == NULL) {
                perror("error opening file");
                return 1;
            }

            unsigned char packet[1050] = {0};

            int llreadPacketCounter = 0;
            int packets=0;

            int whiledone = 0;

            while ((llreadPacketCounter = llread(packet))){ //removi o llcloseDone
                whiledone++;
                if (whiledone == 13) break;
                if (llreadPacketCounter == -1) return;

                printf("packet cenas: %X", packet[6]);

                fseek(file, packets, SEEK_SET);
            
                fwrite(packet+3,1, llreadPacketCounter, file);
                packets+= (llreadPacketCounter-3);
                printf("\n%i\n", packets);
            }
            printf("WHILE DONES: %i", whiledone);
            fclose(file);

            break;
        }
    }
}

int creatingPacket(int control, const char* filename, long fileSize){
    switch(control){
        case 1: // DATA

            break;
        case 2: // START
        {
            int size[2];
            integerToBytes(size,2,fileSize);
            printf("try: %d\n", size[0]);
            unsigned char packet[strlen(filename) + 1];
            packet[0] = 0x02;
            packet[1] = 0x01;
            packet[2] = strlen(filename);
            int i = 0;
            while(i<strlen(filename)+1){ packet[i+3] = filename[i]; i++;}
            i=0;
            while(i<sizeof(packet)-1)printf("%x\n", packet[i++]);
            break;
        }
        case 3: // END
            
            break;
    }
}

int calculateSizeBytes(int size) {
    int bytesRequired = 0;

    while (size > 0) {
        size >>= 8; // Shift right by 8 bits (1 byte)
        bytesRequired++;
    }

    return (bytesRequired == 0) ? 1 : bytesRequired;
}

void integerToBytes(int* bytes, int numBytes, int value) {
    for (int i = 0; i < numBytes; i++) {
        bytes[i] = (value >> (8 * (numBytes - 1 - i))) & 0xFF;
    }
}
