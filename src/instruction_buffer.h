//
// Created by alex on 3/25/20.
//

#ifndef LIBWATCHPOINT_SRC_INSTRUCTION_BUFFER_H
#define LIBWATCHPOINT_SRC_INSTRUCTION_BUFFER_H

#include <type_traits>
#include <sys/mman.h>

template <size_t Size>
class ExecutableInstructionBuffer
{
public:
    ExecutableInstructionBuffer():
        instruction_buffer{
            static_cast<std::byte*>(
                mmap(
                    nullptr,
                    Size,
                    PROT_EXEC | PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    -1,
                    0
                ))},
        buffer_size{0}
    {}

    ~ExecutableInstructionBuffer()
    {
        munmap(instruction_buffer, Size);
    }

    void clear() { buffer_size = 0; }

    template<typename Buffer>
    void append(const Buffer& other_buffer)
    {
        std::copy(
            other_buffer.begin(),
            other_buffer.end(),
            instruction_buffer + buffer_size
        );

        buffer_size += other_buffer.size();
    }

    void append_raw_buffer(std::byte* ptr, size_t len)
    {
        std::copy(
            ptr,
            ptr + len,
            instruction_buffer + buffer_size
        );
        buffer_size += len;
    }

    template<typename... Ts>
    void append(Ts... data)
    {
        auto appender = [&](auto x){
            static_assert(std::is_integral_v<typeof(x)> || std::is_same_v<typeof(x), std::byte>);
            std::copy(
                reinterpret_cast<std::byte*>(&x),
                reinterpret_cast<std::byte*>(&x) + sizeof(x),
                instruction_buffer + buffer_size
            );
            buffer_size += sizeof(x);
        };

        (appender(data), ...);
    }

    template<typename Buffer>
    ExecutableInstructionBuffer& operator+=(const Buffer& other_buffer)
    {
        append(other_buffer);
        return *this;
    }

    [[nodiscard]] std::byte* begin() { return instruction_buffer; }
    [[nodiscard]] std::byte* end() { return instruction_buffer + buffer_size; }
    [[nodiscard]] const std::byte* begin() const { return instruction_buffer; }
    [[nodiscard]] const std::byte* end() const { return instruction_buffer + buffer_size; }
    [[nodiscard]] auto size() { return buffer_size; }

private:
    std::byte* instruction_buffer;
    size_t buffer_size;
};

template <size_t Size>
class InstructionBuffer
{
public:
    void clear() { buffer_size = 0; }

    template<typename Buffer>
    void append(const Buffer& other_buffer)
    {
        std::copy(
            other_buffer.begin(),
            other_buffer.end(),
            instruction_buffer + buffer_size
        );

        buffer_size += other_buffer.size();
    }

    template<typename... Ts>
    void append(Ts... data)
    {
        auto appender = [&](auto x){
            static_assert(std::is_integral_v<typeof(x)> || std::is_same_v<typeof(x), std::byte>);
            std::copy(
                reinterpret_cast<std::byte*>(&x),
                reinterpret_cast<std::byte*>(&x) + sizeof(x),
                instruction_buffer + buffer_size
            );
            buffer_size += sizeof(x);
        };

        (appender(data), ...);
    }

    template<typename Buffer>
    InstructionBuffer& operator+=(const Buffer& other_buffer)
    {
        append(other_buffer);
        return *this;
    }

    [[nodiscard]] std::byte* begin() { return instruction_buffer; }
    [[nodiscard]] std::byte* end() { return instruction_buffer + buffer_size; }
    [[nodiscard]] const std::byte* begin() const { return instruction_buffer; }
    [[nodiscard]] const std::byte* end() const { return instruction_buffer + buffer_size; }
    [[nodiscard]] auto size() const { return buffer_size; }

private:
    std::byte instruction_buffer[Size] {};
    size_t buffer_size {};
};

template<typename... Ts>
auto create_instruction_buffer(Ts... data) ->
    InstructionBuffer<(sizeof(Ts) + ...)>
{
    InstructionBuffer<(sizeof(Ts) + ...)> buffer;
    buffer.append(data...);

    return buffer;
}

#endif //LIBWATCHPOINT_SRC_INSTRUCTION_BUFFER_H
