#include <Jolt/Jolt.h>

#include <cstdarg>
#include <cstdio>

// Forward declarations for custom trace/assert handlers.
static void TraceImpl(const char* inFMT, ...)
#if defined(_MSC_VER)
    ;
#endif

static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, unsigned int inLine) {
    std::printf("[Jolt Assert] %s:%u - %s (%s)\n", inFile, inLine, inExpression, inMessage ? inMessage : "");
    return true;
}

static void TraceImpl(const char* inFMT, ...) {
    char buffer[1024];
    va_list list;
    va_start(list, inFMT);
#if defined(_MSC_VER)
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, inFMT, list);
#else
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
#endif
    va_end(list);
    buffer[sizeof(buffer) - 1] = '\0';
    std::printf("[Jolt] %s", buffer);
}

struct JoltGlobalInitializer {
    JoltGlobalInitializer() {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)
    }
} gJoltInitializer;


