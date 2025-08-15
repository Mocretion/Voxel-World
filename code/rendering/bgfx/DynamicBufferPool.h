#pragma once

#include <bgfx/bgfx.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstdint>
#include <functional>

class Chunk;

class DynamicBufferPool {
public:
    static constexpr size_t TOTAL_PAIR_SIZE = 67108864;     // 67 MB MB
    static constexpr size_t VERTEX_BUFFER_SIZE = 33554432;  // ~33.55 MB (4.194.304 vertices)
    static constexpr size_t INDEX_BUFFER_SIZE = 33554432;   // ~33.55  MB
    static constexpr size_t MAX_CHUNKS_PER_BUFFER = 2048;     // Maximum chunks per buffer pair

    struct BufferAllocation {
        size_t bufferPairIndex = SIZE_MAX;
        size_t vertexOffset = 0;
        size_t vertexCount = 0;
        size_t indexOffset = 0;
        size_t indexCount = 0;

        // Buddy system block handles
        void* vertexBlockHandle = nullptr;
        void* indexBlockHandle = nullptr;

        bool isValid() const { return bufferPairIndex != SIZE_MAX; }
    };

private:
    // Buddy Allocator for managing buffer memory
    class BuddyAllocator {
    public:
        struct Block {
            size_t offset;
            size_t size;
            bool isFree;
            Block* buddy;
            Block* parent;
            Block* left;
            Block* right;
            int level;  // Level in buddy tree (0 = full size)
        };

        BuddyAllocator(size_t totalSize);
        ~BuddyAllocator();

        Block* allocate(size_t size);
        void deallocate(Block* block);
        bool canAllocate(size_t size) const;

        // Defragmentation
        void defragment(std::function<void(size_t oldOffset, size_t newOffset, size_t size)> moveCallback);
        float getFragmentation() const;

        std::vector<std::vector<Block*>> freeLists;  // Free lists per level

    private:
        size_t totalSize;
        size_t minBlockSize;
        int maxLevel;
        Block* root;
        std::vector<std::unique_ptr<Block>> blockPool;

        size_t roundUpToPowerOfTwo(size_t size) const;
        int getSizeLevel(size_t size) const;
        Block* createBlock(size_t offset, size_t size, int level);
        void splitBlock(Block* block);
        void mergeBlock(Block* block);
        Block* findBuddy(Block* block) const;
    };

    struct ChunkInstanceData {
        int x;
        int y;
        int z;
        int pad;

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


        std::unique_ptr<BuddyAllocator> vertexAllocator;
        std::unique_ptr<BuddyAllocator> indexAllocator;

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

        // Staging buffers for defragmentation
        std::vector<uint8_t> vertexStagingBuffer;
        std::vector<uint8_t> indexStagingBuffer;

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
    void defragmentPair(BufferPair* pair);

    // Move data during defragmentation
    void moveBufferData(BufferPair* pair, size_t oldOffset, size_t newOffset,
        size_t size, bool isVertexBuffer);
};