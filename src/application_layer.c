// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayerRole layerRole;
    if (strcmp(role, "tx") == 0){
        layerRole = LlTx;
    }
    else layerRole = LlRx;
    
   LinkLayer layer;
    unsigned char fake[5] = {0x02,0x7E,0x05,0x7D,0x07};

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
        if(llopen(layer) == -1) return;
        if(llwrite(fake,5) == -1) return;
        if(llclose(0) == -1) return;
        break;
    }
    case LlRx:
    {
        if (llopen(layer) == -1){
        return ;
        }
        printf("yoo\n");
        unsigned char packet[27] = {0};

        int llreadPacketCounter = 0;

        while ((llreadPacketCounter = llread(packet)) && (llcloseDone == FALSE)){
            if (llreadPacketCounter == -1) return;
        }
        break;
    }
    }
    
}
