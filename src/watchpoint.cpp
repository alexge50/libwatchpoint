//
// Created by alex on 3/21/20.
//
#include "watchpoint.h"

#include <csignal>
#include <cassert>
#include <sys/mman.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <algorithm>

#include <Zydis/Zydis.h>
#include "mapped_memory_areas.h"
#include "instruction_buffer.h"
#include "byte_literal.h"


static ZydisDecoder decoder;
static MappedMemoryAreas mapped_memory_areas;
static watchpoint_callback_t callback;

static void handler(int signal, siginfo_t* signal_info, void* pcontext);

extern "C" void watchpoint_intialize()
{
    ZydisDecoderInit(
        &decoder,
        ZYDIS_MACHINE_MODE_LONG_64,
        ZYDIS_ADDRESS_WIDTH_64);

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

extern "C" void watch_point_set_callback(watchpoint_callback_t f)
{
    callback = f;
}

void handler(int signal, siginfo_t* signal_info, void* pcontext)
{
    auto context = reinterpret_cast<ucontext_t*>(pcontext);

    auto addr = signal_info->si_addr;
    auto pc = context->uc_mcontext.gregs[REG_RIP];

    void* buffer = nullptr;
    std::size_t buffer_length = 0;

    if(auto memory_area = mapped_memory_areas.query(addr); memory_area.has_value())
    {
        buffer = memory_area->start;
        buffer_length =
            reinterpret_cast<std::size_t>(memory_area->end) - reinterpret_cast<std::size_t>(memory_area->start);
    }
    else ; // TODO: error handling; quit


    ZydisDecodedInstruction instruction;
    if(ZYAN_SUCCESS(
        ZydisDecoderDecodeBuffer(&decoder, reinterpret_cast<unsigned long long*>(pc), 2 * sizeof(long long), &instruction)
    ))
    {
        thread_local ExecutableInstructionBuffer<512> isolated_instruction;

        long long stack_registers_state[2];

        auto save_stack = create_instruction_buffer(
            0x48_b, 0x89_b, 0xe0_b, // mov %rsp, %rax
            0x48_b, 0xa3_b, reinterpret_cast<long long int>(stack_registers_state), // movabs %rax, 0xccc...
            0x48_b, 0x89_b, 0xe8_b, // mov %rbp, %rax
            0x48_b, 0xa3_b, reinterpret_cast<long long int>(stack_registers_state + 1) // movabs %rax, 0xccc...
        );

        ZydisRegister registers[10];
        int n_registers = 0;
        uint16_t size = 0;

        for(int i = 0; i < instruction.operand_count; i++)
        {
            size = std::max(size, instruction.operands[i].size);

            if(instruction.operands[i].type == ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER ||
               (instruction.operands[i].type == ZydisOperandType::ZYDIS_OPERAND_TYPE_MEMORY &&
                instruction.operands[i].mem.base != ZYDIS_REGISTER_NONE))
            {
                auto reg = ZydisRegisterGetLargestEnclosing(
                    ZYDIS_MACHINE_MODE_LONG_64,
                    (instruction.operands[i].type == ZydisOperandType::ZYDIS_OPERAND_TYPE_REGISTER ?
                     instruction.operands[i].reg.value : instruction.operands[i].mem.base));

                if(reg >= ZYDIS_REGISTER_RAX &&
                   reg <= ZYDIS_REGISTER_RDI)
                {
                    registers[n_registers ++] = reg;
                }
                else assert(!"unrecognized register; aborting");
            }
        }

        InstructionBuffer<512> restore_registers;

        for(int i = 0; i < n_registers; i++)
        {
            static uint8_t register_mapping[8] =
                {
                    REG_RAX,
                    REG_RCX,
                    REG_RDX,
                    REG_RBX,
                    REG_RSP,
                    REG_RBP,
                    REG_RSI,
                    REG_RDI,
                };

            static uint8_t bytecode_register_mapping[] =
                {
                    0xb8,
                    0xb9,
                    0xba,
                    0xbb,
                    0xbc,
                    0xbd,
                    0xbe,
                    0xbf,
                };

            restore_registers += create_instruction_buffer( // movabs $context->uc_mcontext.gregs[register], %rax
                0x48_b, bytecode_register_mapping[registers[i] - ZYDIS_REGISTER_RAX],
                context->uc_mcontext.gregs[register_mapping[registers[i] - ZYDIS_REGISTER_RAX]]
            );
        }

        InstructionBuffer<512> save_registers;

        if(auto i = std::find(
                registers,
                registers + n_registers,
                ZYDIS_REGISTER_RAX); i != registers + n_registers)
        {// save rax
            std::swap(*i, registers[0]);

            save_registers += create_instruction_buffer( // movabs %rax, 0xccc...
                0x48_b, 0xa3_b, reinterpret_cast<long long int>(context->uc_mcontext.gregs + REG_RAX)
            );
        }

        for(int i = 1; i < n_registers; i++)
        {
            static uint8_t register_mapping[8] =
                {
                    REG_RAX,
                    REG_RCX,
                    REG_RDX,
                    REG_RBX,
                    REG_RSP,
                    REG_RBP,
                    REG_RSI,
                    REG_RDI,
                };

            static uint8_t bytecode_register_mapping[] =
                {
                    0x00,
                    0xc8,
                    0xd0,
                    0xd8,
                    0xe0,
                    0xe8,
                    0xf0,
                    0xf8,
                };

            save_registers += create_instruction_buffer(
                //mov %reg, %rax
                0x48_b, 0x89_b, static_cast<unsigned char>(registers[i] - ZYDIS_REGISTER_RAX),
                // movabs %rax, 0xccc...
                0x48_b, 0xa3_b, reinterpret_cast<long long int>(context->uc_mcontext.gregs + REG_RAX)
            );
        }

        auto restore_stack = create_instruction_buffer(
            0x48_b, 0xa1_b, reinterpret_cast<long long int>(stack_registers_state), //movabs 0xcccccccccccccccc,%rax
            0x48_b, 0x89_b, 0xc4_b, //movabs %rax, %rsp
            0x48_b, 0xa1_b, reinterpret_cast<long long int>(stack_registers_state + 1), //movabs 0xcccccccccccccccc,%rax
            0x48_b, 0x89_b, 0xc5_b  //movabs %rax, %rbp
        );

        int n_isolated_instruction = 0;

        isolated_instruction.append(
            0x55_b, // push %rbp
            0x48_b, 0x89_b, 0xe5_b // mov %rsp, %rbp
        );

        isolated_instruction += save_stack;
        isolated_instruction += restore_registers;

        isolated_instruction.append_raw_buffer(reinterpret_cast<std::byte*>(pc), instruction.length);

        isolated_instruction += save_registers;
        isolated_instruction += restore_stack;
        isolated_instruction.append(
            0xc9_b,// leaveq
            0xc3_b //retq
        );

        mprotect(buffer, buffer_length, PROT_READ | PROT_WRITE);

        using f = void(*)();
        reinterpret_cast<f>(isolated_instruction.begin())();

        callback(addr, size / 8);

        mprotect(buffer, buffer_length, PROT_NONE);
        context->uc_mcontext.gregs[REG_RIP] += instruction.length;
    }
    else assert(!"can't decode instruction");
}
