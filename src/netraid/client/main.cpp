#include "client/Client.h"


int main (int argc, char* argv[]) 
{
    Client *c = new Client();
    printf("Run\n");
    c->run();
    printf("Client done.\n");
    char input;
    while(true)
    {
        input = getchar();
        sleep(1);
    }
    return 0;
}

