#pragma once

#include <bgfx/bgfx.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstdint>
#include <list>
#include <algorithm>

class Chunk;

class DynamicBufferPool {
public:
    static constexpr size_t TOTAL_PAIR_SIZE = 64000000;     // 64 MB
    static constexpr size_t VERTEX_BUFFER_SIZE = 36000000;  // ~36 MB
    static constexpr size_t INDEX_BUFFER_SIZE = 27000000;   // ~27 MB (=63MB)
    static constexpr size_t MAX_CHUNKS_PER_BUFFER = 2048;   // Maximum chunks per buffer pair

    struct BufferAllocation {
        size_t bufferPairIndex = SIZE_MAX;
        size_t vertexOffset = 0;
        size_t vertexCount = 0;
        size_t indexOffset = 0;
        size_t indexCount = 0;

        // Free list block handles
        void* vertexBlockHandle = nullptr;
        void* indexBlockHandle = nullptr;

        bool isValid() const { return bufferPairIndex != SIZE_MAX; }
    };

private:
    // Simple Free List Allocator for managing buffer memory
    class FreeListAllocator {
    public:
        struct Block {
            size_t offset;
            size_t size;
            bool isFree;

            Block(size_t off, size_t sz, bool free = true)
                : offset(off), size(sz), isFree(free)
            {
            }
        };

        FreeListAllocator(size_t totalSize);
        ~FreeListAllocator() = default;
        
        Block* allocate(size_t size);
        void deallocate(Block* block);
        bool canAllocate(size_t size) const;
        float getFragmentation() const;

        // Get total free space
        size_t getTotalFreeSpace() const;

    private:
        size_t totalSize;
        std::list<Block> blocks;  // Sorted by offset

        // Helper to merge adjacent free blocks
        void coalesceBlocks();

        // Find best fitting free block (first-fit strategy)
        std::list<Block>::iterator findFreeBlock(size_t size);
    };

    struct ChunkInstanceData {
        float x;
        float y;
        float z;
        float pad;

        static bgfx::VertexLayout getBgfxLayout()
        {
            static bgfx::VertexLayout layout;
            static bool initialized = false;

            if (!initialized)
            {
                layout.begin()
                    .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
                    .end();
                initialized = true;
            }

            return layout;
        }
    };

    struct BufferPair {
        bgfx::DynamicVertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
        bgfx::DynamicIndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;
        bgfx::DynamicIndexBufferHandle drawCommandBuffer = BGFX_INVALID_HANDLE;
        bgfx::IndirectBufferHandle indirectBuffer = BGFX_INVALID_HANDLE;
        bgfx::DynamicVertexBufferHandle instanceBuffer = BGFX_INVALID_HANDLE;

        std::unique_ptr<FreeListAllocator> vertexAllocator;
        std::unique_ptr<FreeListAllocator> indexAllocator;

        float* chunkIndirectData = new float[MAX_CHUNKS_PER_BUFFER * 4];
        ChunkInstanceData* chunkInstanceData = new ChunkInstanceData[MAX_CHUNKS_PER_BUFFER];

        // First element is count of commands
        // After that:
        // k + 0 is indexCount
        // k + 1 is instanceCount
        // k + 2 is firstIndex;
        // k + 3 is vertexOffset;
        uint32_t* drawCommands = new uint32_t[MAX_CHUNKS_PER_BUFFER * 4 + 1];
        std::unordered_map<Chunk*, BufferAllocation> chunkAllocations;

        bool needsIndirectUpdate = false;
        size_t activeChunks = 0;

        BufferPair();
        ~BufferPair();

        void initialize();
        void cleanup();
    };

public:
    DynamicBufferPool();
    ~DynamicBufferPool();

    // Allocate space for a chunk's mesh data
    BufferAllocation allocateChunk(Chunk* chunk, size_t vertexCount, size_t indexCount,
        const void* vertexData, const void* indexData);

    // Deallocate a chunk's buffer space
    void deallocateChunk(Chunk* chunk, const BufferAllocation& allocation);

    // Update chunk data (in-place if possible, reallocate if needed)
    BufferAllocation updateChunk(Chunk* chunk, const BufferAllocation& oldAllocation,
        size_t newVertexCount, size_t newIndexCount,
        const void* vertexData, const void* indexData);

    // Render all buffer pairs using indirect drawing
    void renderAll(bgfx::ProgramHandle computeProgram, bgfx::ProgramHandle renderProgram, bgfx::UniformHandle chunkPosUniform);

    // Defragmentation (call periodically or when fragmentation is high)
    void defragmentBuffers(float fragmentationThreshold = 0.3f);

    // Statistics
    size_t getBufferPairCount() const { return bufferPairs.size(); }
    float getAverageFragmentation() const;
    size_t getTotalAllocatedMemory() const;

private:
    std::vector<std::unique_ptr<BufferPair>> bufferPairs;
    mutable std::mutex poolMutex;

    // Helper methods
    BufferPair* findOrCreateBufferPair(size_t vertexSize, size_t indexSize);
    void uploadToBuffer(BufferPair* pair, const BufferAllocation& allocation,
        const void* vertexData, const void* indexData);
    void updateIndirectBuffer(BufferPair* pair);
};