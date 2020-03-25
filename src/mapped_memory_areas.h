//
// Created by alex on 3/25/20.
//

#ifndef LIBWATCHPOINT_SRC_MAPPED_MEMORY_AREAS_H
#define LIBWATCHPOINT_SRC_MAPPED_MEMORY_AREAS_H

struct MemoryArea
{
    void* start;
    void* end;
};

class MappedMemoryAreas
{
private:
    using MemoryAreas = std::vector<MemoryArea>;
    using iterator = std::vector<MemoryArea>::iterator;

public:
    void add(MemoryArea memory_area)
    {
        mapped_memory_areas.push_back(memory_area);
    }

    std::optional<MemoryArea> remove(void* memory)
    {
        auto it = raw_query(memory);

        if(it != mapped_memory_areas.end())
        {
            mapped_memory_areas.erase(it);
            return *it;
        }
        else return std::nullopt;
    }

    std::optional<MemoryArea> query(void* memory)
    {
        auto it = raw_query(memory);

        if(it != mapped_memory_areas.end())
            return *it;
        else return std::nullopt;
    }

private:
    iterator raw_query(void* memory)
    {
        auto memory_address = reinterpret_cast<std::size_t>(memory);
        auto it = std::find_if(
            mapped_memory_areas.begin(),
            mapped_memory_areas.end(),
            [memory_address](auto& x){
                return memory_address >= reinterpret_cast<std::size_t>(x.start) &&
                       memory_address < reinterpret_cast<std::size_t>(x.end);
          }
        );

        return it;
    }

private:
    MemoryAreas mapped_memory_areas;
};

#endif // LIBWATCHPOINT_SRC_MAPPED_MEMORY_AREAS_H
