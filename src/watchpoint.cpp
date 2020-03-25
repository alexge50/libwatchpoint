//
// Created by alex on 3/21/20.
//

#include <csignal>
#include <cassert>
#include <sys/mman.h>

#include <cstdint>
#include <optional>

#include "mapped_memory_areas.h"


static MappedMemoryAreas mapped_memory_areas;

static void handler(int signal, siginfo_t* signal_info, void* pcontext);

extern "C" void watchpoint_intialize()
{
    {
        struct sigaction sa{};
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = handler;

        assert(sigaction(SIGSEGV, &sa, nullptr) != -1);
    }
}

extern "C" void* watchpoint_alloc(size_t size)
{
    void* addr = mmap(
            nullptr,
            size,
            PROT_NONE,
            MAP_ANONYMOUS | MAP_PRIVATE,
            -1,
            0
        );

    mapped_memory_areas.add({addr, static_cast<std::byte*>(addr) + size});

    return addr;
}

extern "C" void watchpoint_free(void* addr)
{
    std::optional<MemoryArea> memory_area = mapped_memory_areas.remove(addr);

    if(memory_area.has_value())
    {
        void* addr = memory_area->start;
        std::size_t length =
            reinterpret_cast<std::size_t>(memory_area->end) - reinterpret_cast<std::size_t>(memory_area->start);
        munmap(addr, length);
    }
    else ; // TODO: error handling
}

