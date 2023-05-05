
#include <cstdio>

#include "Engine.h"
#include "EngineContext.h"

int main(int argc, char** argv)
{
    EngineContext context  = {};

    init(context);
    run(context);
    cleanup(context);

    return 0;
}