#include <vulkan/vulkan.h>

#include <cstdio>

int main(int argc, char** argv)
{
    EngineContext context;

    init(context);
    run(context);
    cleanup(context);

    return 0;
}