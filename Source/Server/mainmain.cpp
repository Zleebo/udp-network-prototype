#include "Server.h"

int main()
{
    if (!ServerHandler::GetInstance().Initialize())
    {
        return 1;
    }

    while (ServerHandler::GetInstance().Update())
    {
    }

    ServerHandler::GetInstance().Shutdown();
    return 0;
}
