
#ifdef _WIN32

#include <intrin.h>

void doDebugBreak()
{
	__debugbreak();
}

#endif // _WIN32