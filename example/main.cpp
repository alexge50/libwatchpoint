#include <cstdio>
#include <csignal>
#include <malloc.h>
#include <cassert>
#include <sys/mman.h>

#include <utility>
#include <algorithm>

#include <Zydis/Zydis.h>
#include <cstdint>

#define NDEBUG

char* buffer;
const auto pagesize = 4096;

ZydisDecoder decoder;


void handler(int signal, siginfo_t* signal_info, void* pcontext)
{
    ucontext_t* context = reinterpret_cast<ucontext_t*>(pcontext);

    auto addr = reinterpret_cast<long>(signal_info->si_addr);
    auto pc = context->uc_mcontext.gregs[REG_RIP];
    unsigned long long p = *reinterpret_cast<unsigned long long*>(pc);

#ifdef NDEBUG
    printf("fault at address: 0x%lx\n", addr);
    printf("pc: 0x%llx\n", pc);
    printf("opcode: 0x%llx\n", p);
#endif

    ZydisDecodedInstruction instruction;
    if(ZYAN_SUCCESS(
            ZydisDecoderDecodeBuffer(&decoder, reinterpret_cast<unsigned long long*>(pc), 2 * sizeof(long long), &instruction)
            ))
    {
        mprotect(buffer, pagesize, PROT_READ | PROT_WRITE);

        uint8_t* isolated_instruction = reinterpret_cast<uint8_t*>(mmap(nullptr, 512, PROT_EXEC | PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

        long long stack_registers_state[2];
        unsigned char save_stack[] = {
                0x48, 0x89, 0xe0, // mov %rsp, %rax
                0x48, 0xa3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // movabs %rax, 0xccc...
                0x48, 0x89, 0xe8, // mov %rbp, %rax
                0x48, 0xa3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // movabs %rax, 0xccc...
        };

        *reinterpret_cast<long long*>(save_stack + 5) = reinterpret_cast<long long int>(stack_registers_state);
        *reinterpret_cast<long long*>(save_stack + 18) = reinterpret_cast<long long int>(stack_registers_state + 1);

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

        unsigned char restore_registers[512];
        int n_restore_registers = 0;

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

            restore_registers[n_restore_registers ++] = 0x48; // movabs $context->uc_mcontext.gregs[register], %rax
            restore_registers[n_restore_registers ++] = bytecode_register_mapping[registers[i] - ZYDIS_REGISTER_RAX];
            for(std::size_t j = 0; j < 8; j++)
                restore_registers[n_restore_registers ++] = reinterpret_cast<uint8_t*>(
                            context->uc_mcontext.gregs + register_mapping[registers[i] - ZYDIS_REGISTER_RAX]
                        )[j];
        }

        unsigned char save_registers[512];
        int n_save_registers = 0;

        if(auto i = std::find(
                    registers,
                    registers + n_registers,
                    ZYDIS_REGISTER_RAX); i != registers + n_registers)
        {// save rax
            std::swap(*i, registers[0]);

            save_registers[n_save_registers++] = 0x48; // movabs %rax, 0xccc...
            save_registers[n_save_registers++] = 0xa3;
            *reinterpret_cast<long long*>(save_registers + n_save_registers) =
                    reinterpret_cast<long long int>(context->uc_mcontext.gregs + REG_RAX);
            n_save_registers += 8;
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

            save_registers[n_save_registers++] = 0x48; //mov %reg, %rax
            save_registers[n_save_registers++] = 0x89;
            save_registers[n_save_registers++] = registers[i] - ZYDIS_REGISTER_RAX;

            save_registers[n_save_registers++] = 0x48; // movabs %rax, 0xccc...
            save_registers[n_save_registers++] = 0xa3;
            *reinterpret_cast<long long*>(save_registers + n_save_registers) =
                    reinterpret_cast<long long int>(context->uc_mcontext.gregs + REG_RAX);
            n_save_registers += 8;
        }

        uint8_t restore_stack[] ={
            0x48, 0xa1, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, //movabs 0xcccccccccccccccc,%rax
            0x48, 0x89, 0xc4, //movabs %rax, %rsp
            0x48, 0xa1, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, //movabs 0xcccccccccccccccc,%rax
            0x48, 0x89, 0xc5, //movabs %rax, %rbp
        };

        *reinterpret_cast<long long*>(restore_stack + 2) = reinterpret_cast<long long int>(stack_registers_state);
        *reinterpret_cast<long long*>(restore_stack + 15) = reinterpret_cast<long long int>(stack_registers_state + 1);

        int n_isolated_instruction = 0;

        isolated_instruction[n_isolated_instruction ++] = 0x55; // push %rbp
        isolated_instruction[n_isolated_instruction ++] = 0x48; // mov %rsp, %rbp
        isolated_instruction[n_isolated_instruction ++] = 0x89;
        isolated_instruction[n_isolated_instruction ++] = 0xe5;

        std::copy(
                std::begin(save_stack),
                std::end(save_stack),
                isolated_instruction + n_isolated_instruction
        );

        n_isolated_instruction += std::size(save_stack);

        std::copy(
                restore_registers,
                restore_registers + n_restore_registers,
                isolated_instruction + n_isolated_instruction
        );

        n_isolated_instruction += n_restore_registers;

        std::copy(
                reinterpret_cast<uint8_t*>(pc),
                reinterpret_cast<uint8_t*>(pc) + instruction.length,
                isolated_instruction + n_isolated_instruction
        );

        n_isolated_instruction += instruction.length;

        std::copy(
                save_registers,
                save_registers + n_save_registers,
                isolated_instruction + n_isolated_instruction
        );
        n_isolated_instruction += n_save_registers;

        std::copy(
                std::begin(restore_stack),
                std::end(restore_stack),
                isolated_instruction + n_isolated_instruction
        );
        n_isolated_instruction += std::size(restore_stack);

        isolated_instruction[n_isolated_instruction ++] = 0xc9; // leaveq
        isolated_instruction[n_isolated_instruction ++] = 0xc3; // retq

        using f = void(*)();
        reinterpret_cast<f>(isolated_instruction)();

        printf("[0x%lx] = ", addr);
        for(int i = 0; i < size / 8; i++)
            printf("0x%hhx ", reinterpret_cast<char*>(addr)[i]);
        printf("\n");

        munmap(isolated_instruction, 512);
        mprotect(buffer, pagesize, PROT_NONE);
        context->uc_mcontext.gregs[REG_RIP] += instruction.length;
    }
    else assert(!"can't decode instruction");
}


int main()
{
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);

    {
        struct sigaction sa{};
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = handler;

        assert(sigaction(SIGSEGV, &sa, nullptr) != -1);
    }

    buffer = reinterpret_cast<char*>(memalign(pagesize, pagesize));

    mprotect(buffer, pagesize, PROT_NONE);
    printf("Attempting to write to memory\n");
    reinterpret_cast<long long*>(buffer)[0] = 1;
    printf("Written to memory\n");

    return 0;
}
