#include <stdio.h>
#include <watchpoint.h>

void callback(const void* addr, int size)
{
    printf("[%p] = ", addr);
    for(int i = 0; i < size; i++)
        printf("0x%hhx ", reinterpret_cast<const char*>(addr)[i]);
    printf("\n");
}

int main()
{
    watchpoint_intialize();
    watchpoint_set_callback(callback);

    auto buffer = static_cast<long long*>(watchpoint_alloc(4096));

    printf("Attempting to write to memory %p\n", buffer);
    buffer[0] = 1;
    printf("Written to memory\n");

    return 0;
}
