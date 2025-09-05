#include "DynamicBufferPool.h"
#include "Chunk.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <chrono>

// ============================================================================
// FreeListAllocator Implementation - Much simpler than BuddyAllocator!
// ============================================================================

DynamicBufferPool::FreeListAllocator::FreeListAllocator(size_t totalSize)
    : totalSize(totalSize)
{
    // Start with one big free block
    blocks.emplace_back(0, totalSize, true);

    std::cout << "[FREELIST] Initialized with size: " << totalSize << " bytes" << std::endl;
}

DynamicBufferPool::FreeListAllocator::Block*
DynamicBufferPool::FreeListAllocator::allocate(size_t size)
{
    // Align size to 16 bytes for better performance
    size = (size + 15) & ~15;

    // Find a free block that fits (first-fit strategy)
    auto it = findFreeBlock(size);
    if (it == blocks.end())
    {
        std::cerr << "[FREELIST] No space for allocation of size: " << size << std::endl;
        return nullptr;
    }

    // If the block is exactly the right size, just mark it as used
    if (it->size == size)
    {
        it->isFree = false;
        return &(*it);
    }

    // Otherwise, split the block
    size_t remainingSize = it->size - size;
    size_t remainingOffset = it->offset + size;

    // Resize the current block to the allocated size
    it->size = size;
    it->isFree = false;

    // Insert a new free block for the remaining space
    blocks.insert(std::next(it), Block(remainingOffset, remainingSize, true));

    return &(*it);
}

void DynamicBufferPool::FreeListAllocator::deallocate(Block* block)
{
    if (!block || block->isFree)
    {
        std::cerr << "[FREELIST] Invalid deallocation attempt" << std::endl;
        return;
    }

    // Find the block in our list
    auto it = std::find_if(blocks.begin(), blocks.end(),
        [block](const Block& b) { return &b == block; });

    if (it == blocks.end())
    {
        std::cerr << "[FREELIST] Block not found in allocator" << std::endl;
        return;
    }

    // Mark as free
    it->isFree = true;

    // Coalesce with adjacent free blocks
    coalesceBlocks();
}

bool DynamicBufferPool::FreeListAllocator::canAllocate(size_t size) const
{
    size = (size + 15) & ~15;  // Apply same alignment

    for (const auto& block : blocks)
    {
        if (block.isFree && block.size >= size)
        {
            return true;
        }
    }
    return false;
}

float DynamicBufferPool::FreeListAllocator::getFragmentation() const
{
    size_t totalFree = 0;
    size_t largestFree = 0;

    for (const auto& block : blocks)
    {
        if (block.isFree)
        {
            totalFree += block.size;
            largestFree = std::max(largestFree, block.size);
        }
    }

    if (totalFree == 0) return 0.0f;

    // Fragmentation: 1.0 means completely fragmented, 0.0 means no fragmentation
    return 1.0f - (static_cast<float>(largestFree) / static_cast<float>(totalFree));
}

size_t DynamicBufferPool::FreeListAllocator::getTotalFreeSpace() const
{
    size_t totalFree = 0;
    for (const auto& block : blocks)
    {
        if (block.isFree)
        {
            totalFree += block.size;
        }
    }
    return totalFree;
}

void DynamicBufferPool::FreeListAllocator::coalesceBlocks()
{
    auto it = blocks.begin();
    while (it != blocks.end())
    {
        if (!it->isFree)
        {
            ++it;
            continue;
        }

        // Look for adjacent free block
        auto next = std::next(it);
        if (next != blocks.end() && next->isFree &&
            (it->offset + it->size == next->offset))
        {
            // Merge blocks
            it->size += next->size;
            blocks.erase(next);
            // Don't increment iterator, check again for more merges
        }
        else
        {
            ++it;
        }
    }
}

std::list<DynamicBufferPool::FreeListAllocator::Block>::iterator
DynamicBufferPool::FreeListAllocator::findFreeBlock(size_t size)
{
    // First-fit strategy: find first block that's big enough
    return std::find_if(blocks.begin(), blocks.end(),
        [size](const Block& block) {
            return block.isFree && block.size >= size;
        });
}

// ============================================================================
// BufferPair Implementation
// ============================================================================

DynamicBufferPool::BufferPair::BufferPair()
{
}

DynamicBufferPool::BufferPair::~BufferPair()
{
    cleanup();
    delete[] chunkIndirectData;
    delete[] chunkInstanceData;
    delete[] drawCommands;
}

void DynamicBufferPool::BufferPair::initialize()
{
    std::cout << "[BUFFER_PAIR] Initializing new buffer pair" << std::endl;

    // Create dynamic buffers
    vertexBuffer = bgfx::createDynamicVertexBuffer(
        VERTEX_BUFFER_SIZE / sizeof(Vertex),
        Vertex::getBgfxLayout(),
        BGFX_BUFFER_ALLOW_RESIZE
    );

    indexBuffer = bgfx::createDynamicIndexBuffer(
        INDEX_BUFFER_SIZE / sizeof(uint32_t),
        BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_INDEX32 | BGFX_BUFFER_ALLOW_RESIZE
    );

    drawCommandBuffer = bgfx::createDynamicIndexBuffer(
        MAX_CHUNKS_PER_BUFFER * 4 + 1,
        BGFX_BUFFER_ALLOW_RESIZE | BGFX_BUFFER_INDEX32 | BGFX_BUFFER_COMPUTE_READ
    );

    instanceBuffer = bgfx::createDynamicVertexBuffer(
        MAX_CHUNKS_PER_BUFFER,
        ChunkInstanceData::getBgfxLayout(),
        BGFX_BUFFER_ALLOW_RESIZE
    );

    // Create indirect buffer for draw commands
    indirectBuffer = bgfx::createIndirectBuffer(MAX_CHUNKS_PER_BUFFER);

    // Initialize free list allocators (much simpler than buddy!)
    vertexAllocator = std::make_unique<FreeListAllocator>(VERTEX_BUFFER_SIZE);
    indexAllocator = std::make_unique<FreeListAllocator>(INDEX_BUFFER_SIZE);

    drawCommands[0] = 0;

    std::cout << "[BUFFER_PAIR] Initialization complete with FreeListAllocators" << std::endl;
}

void DynamicBufferPool::BufferPair::cleanup()
{
    if (bgfx::isValid(vertexBuffer))
    {
        bgfx::destroy(vertexBuffer);
        vertexBuffer = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(indexBuffer))
    {
        bgfx::destroy(indexBuffer);
        indexBuffer = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(drawCommandBuffer))
    {
        bgfx::destroy(drawCommandBuffer);
        drawCommandBuffer = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(indirectBuffer))
    {
        bgfx::destroy(indirectBuffer);
        indirectBuffer = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(instanceBuffer))
    {
        bgfx::destroy(instanceBuffer);
        instanceBuffer = BGFX_INVALID_HANDLE;
    }
}

// ============================================================================
// DynamicBufferPool Implementation
// ============================================================================

DynamicBufferPool::DynamicBufferPool()
{
    std::cout << "[POOL] Initializing DynamicBufferPool with FreeListAllocator" << std::endl;

    // Start with one buffer pair
    auto firstPair = std::make_unique<BufferPair>();
    firstPair->initialize();
    bufferPairs.push_back(std::move(firstPair));
}

DynamicBufferPool::~DynamicBufferPool()
{
    for (auto& pair : bufferPairs)
    {
        pair->cleanup();
    }
}

DynamicBufferPool::BufferPair*
DynamicBufferPool::findOrCreateBufferPair(size_t vertexSize, size_t indexSize)
{
    // Try to find existing buffer with space
    for (size_t i = 0; i < bufferPairs.size(); ++i)
    {
        auto& pair = bufferPairs[i];
        if (pair->vertexAllocator->canAllocate(vertexSize) &&
            pair->indexAllocator->canAllocate(indexSize))
        {
            return pair.get();
        }
    }

    // Create new buffer pair
    std::cout << "[POOL] Creating new buffer pair #" << bufferPairs.size()
        << " (existing buffers full)" << std::endl;

    auto newPair = std::make_unique<BufferPair>();
    newPair->initialize();
    BufferPair* pairPtr = newPair.get();
    bufferPairs.push_back(std::move(newPair));

    return pairPtr;
}

DynamicBufferPool::BufferAllocation
DynamicBufferPool::allocateChunk(Chunk* chunk, size_t vertexCount, size_t indexCount,
    const void* vertexData, const void* indexData)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(poolMutex);

    size_t vertexSize = vertexCount * sizeof(Vertex);
    size_t indexSize = indexCount * sizeof(uint32_t);

    // Find or create suitable buffer pair
    BufferPair* pair = findOrCreateBufferPair(vertexSize, indexSize);
    if (!pair)
    {
        std::cerr << "[POOL] Failed to find or create buffer pair!" << std::endl;
        return BufferAllocation();
    }

    // Find which buffer pair index this is
    size_t pairIndex = SIZE_MAX;
    for (size_t i = 0; i < bufferPairs.size(); ++i)
    {
        if (bufferPairs[i].get() == pair)
        {
            pairIndex = i;
            break;
        }
    }

    if (pairIndex == SIZE_MAX)
    {
        std::cerr << "[POOL] Could not find buffer pair index!" << std::endl;
        return BufferAllocation();
    }

    // Allocate from free list allocators (much simpler!)
    auto* vertexBlock = pair->vertexAllocator->allocate(vertexSize);
    auto* indexBlock = pair->indexAllocator->allocate(indexSize);

    if (!vertexBlock || !indexBlock)
    {
        std::cerr << "[POOL] Allocation failed! Vertex: " << (vertexBlock ? "OK" : "FAIL")
            << ", Index: " << (indexBlock ? "OK" : "FAIL") << std::endl;

        // Cleanup partial allocation
        if (vertexBlock) pair->vertexAllocator->deallocate(vertexBlock);
        if (indexBlock) pair->indexAllocator->deallocate(indexBlock);

        return BufferAllocation();
    }

    // Create allocation record
    BufferAllocation allocation;
    allocation.bufferPairIndex = pairIndex;
    allocation.vertexOffset = vertexBlock->offset;
    allocation.vertexCount = vertexCount;
    allocation.indexOffset = indexBlock->offset;
    allocation.indexCount = indexCount;
    allocation.vertexBlockHandle = vertexBlock;
    allocation.indexBlockHandle = indexBlock;

    // Upload data to buffers
    uploadToBuffer(pair, allocation, vertexData, indexData);

    // Store allocation
    pair->chunkAllocations[chunk] = allocation;
    pair->activeChunks++;
    pair->needsIndirectUpdate = true;

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    //std::cout << "[POOL] Allocation complete in " << duration.count() << "micS - "
    //    << "Pair: " << pairIndex
    //    << ", V-offset: " << allocation.vertexOffset
    //    << ", I-offset: " << allocation.indexOffset << std::endl;

    return allocation;
}

void DynamicBufferPool::uploadToBuffer(BufferPair* pair, const BufferAllocation& allocation,
    const void* vertexData, const void* indexData)
{
    // Update vertex buffer
    if (vertexData && allocation.vertexCount > 0)
    {
        uint32_t vertexStartIndex = allocation.vertexOffset / sizeof(Vertex);

        bgfx::update(pair->vertexBuffer,
            vertexStartIndex,
            bgfx::makeRef(vertexData, allocation.vertexCount * sizeof(Vertex)));
    }

    // Update index buffer  
    if (indexData && allocation.indexCount > 0)
    {
        uint32_t indexStartIndex = allocation.indexOffset / sizeof(uint32_t);

        bgfx::update(pair->indexBuffer,
            indexStartIndex,
            bgfx::makeRef(indexData, allocation.indexCount * sizeof(uint32_t)));
    }
}

void DynamicBufferPool::deallocateChunk(Chunk* chunk, const BufferAllocation& allocation)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    if (!allocation.isValid()) return;

    std::lock_guard<std::mutex> lock(poolMutex);

    if (allocation.bufferPairIndex >= bufferPairs.size())
    {
        std::cerr << "[POOL] Invalid buffer pair index: " << allocation.bufferPairIndex << std::endl;
        return;
    }

    BufferPair* pair = bufferPairs[allocation.bufferPairIndex].get();

    // Deallocate from free list allocators (simple!)
    if (allocation.vertexBlockHandle)
    {
        pair->vertexAllocator->deallocate(
            static_cast<FreeListAllocator::Block*>(allocation.vertexBlockHandle));
    }
    if (allocation.indexBlockHandle)
    {
        pair->indexAllocator->deallocate(
            static_cast<FreeListAllocator::Block*>(allocation.indexBlockHandle));
    }

    // Remove from chunk allocations
    pair->chunkAllocations.erase(chunk);
    pair->activeChunks--;
    pair->needsIndirectUpdate = true;

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    //std::cout << "[POOL] Deallocated a chunk. Active chunks in pair: " << pair->activeChunks << ". Dealloc took " << duration << "microS" << std::endl;
}

DynamicBufferPool::BufferAllocation
DynamicBufferPool::updateChunk(Chunk* chunk, const BufferAllocation& oldAllocation,
    size_t newVertexCount, size_t newIndexCount,
    const void* vertexData, const void* indexData)
{
    // If size fits in existing allocation, update in place
    if (oldAllocation.isValid() &&
        newVertexCount <= oldAllocation.vertexCount &&
        newIndexCount <= oldAllocation.indexCount)
    {
        std::lock_guard<std::mutex> lock(poolMutex);

        BufferPair* pair = bufferPairs[oldAllocation.bufferPairIndex].get();
        BufferAllocation updatedAllocation = oldAllocation;
        updatedAllocation.vertexCount = newVertexCount;
        updatedAllocation.indexCount = newIndexCount;

        uploadToBuffer(pair, updatedAllocation, vertexData, indexData);
        pair->chunkAllocations[chunk] = updatedAllocation;
        pair->needsIndirectUpdate = true;

        return updatedAllocation;
    }

    // Otherwise, deallocate and reallocate
    deallocateChunk(chunk, oldAllocation);
    return allocateChunk(chunk, newVertexCount, newIndexCount, vertexData, indexData);
}

void DynamicBufferPool::updateIndirectBuffer(BufferPair* pair)
{
    if (!pair->needsIndirectUpdate) return;

    // Reset arrays
    memset(pair->drawCommands, 0, (MAX_CHUNKS_PER_BUFFER * 4 + 1) * sizeof(uint32_t));
    memset(pair->chunkInstanceData, 0, MAX_CHUNKS_PER_BUFFER * sizeof(ChunkInstanceData));

    // Build draw commands from chunk allocations
    uint32_t cmdIndex = 1;
    uint32_t k = 0;

    for (const auto& [chunk, allocation] : pair->chunkAllocations)
    {
        if (k >= MAX_CHUNKS_PER_BUFFER)
        {
            std::cerr << "[POOL] WARNING: Exceeded MAX_CHUNKS_PER_BUFFER!" << std::endl;
            break;
        }

        // Calculate offsets
        uint32_t indexOffset = allocation.indexOffset / sizeof(uint32_t);
        uint32_t vertexOffset = allocation.vertexOffset / sizeof(Vertex);

        pair->drawCommands[cmdIndex++] = allocation.indexCount;
        pair->drawCommands[cmdIndex++] = 1;  // Instance count
        pair->drawCommands[cmdIndex++] = indexOffset;
        pair->drawCommands[cmdIndex++] = vertexOffset;

        pair->chunkInstanceData[k].x = static_cast<float>(chunk->getChunkX());
        pair->chunkInstanceData[k].y = static_cast<float>(chunk->getChunkY());
        pair->chunkInstanceData[k].z = static_cast<float>(chunk->getChunkZ());
        pair->chunkInstanceData[k].pad = 0.0f;

        ++k;
    }

    pair->drawCommands[0] = k;  // Number of draw commands

    // Update buffers
    if (pair->drawCommands[0] > 0)
    {
        bgfx::update(pair->drawCommandBuffer, 0,
            bgfx::makeRef(pair->drawCommands, (pair->drawCommands[0] * 4 + 1) * sizeof(uint32_t)));

        bgfx::update(pair->instanceBuffer, 0,
            bgfx::makeRef(pair->chunkInstanceData, pair->drawCommands[0] * sizeof(ChunkInstanceData)));
    }

    pair->needsIndirectUpdate = false;
}

void DynamicBufferPool::renderAll(bgfx::ProgramHandle computeProgram, bgfx::ProgramHandle renderProgram, bgfx::UniformHandle chunkPosUniform)
{
    for (size_t i = 0; i < bufferPairs.size(); ++i)
    {
        BufferPair* pair = bufferPairs[i].get();

        if (pair->activeChunks == 0) continue;

        // Update indirect buffer if needed
        updateIndirectBuffer(pair);

        if (pair->drawCommands[0] == 0)
        {
            continue;
        }

        // Compute shader to build indirect buffer
        bgfx::setBuffer(0, pair->drawCommandBuffer, bgfx::Access::Read);
        bgfx::setBuffer(1, pair->indirectBuffer, bgfx::Access::ReadWrite);
        bgfx::dispatch(0, computeProgram, 1, 1, 1);

        // Set buffers for rendering
        bgfx::setVertexBuffer(0, pair->vertexBuffer);
        bgfx::setIndexBuffer(pair->indexBuffer);
        bgfx::setInstanceDataBuffer(pair->instanceBuffer, 0, pair->drawCommands[0]);

        // Submit indirect draw call
        bgfx::submit(0, renderProgram, pair->indirectBuffer, 0, pair->drawCommands[0]);
    }
}

void DynamicBufferPool::defragmentBuffers(float fragmentationThreshold)
{
    std::lock_guard<std::mutex> lock(poolMutex);

    for (auto& pair : bufferPairs)
    {
        float vertexFrag = pair->vertexAllocator->getFragmentation();
        float indexFrag = pair->indexAllocator->getFragmentation();

        if (vertexFrag > fragmentationThreshold || indexFrag > fragmentationThreshold)
        {
            std::cout << "[POOL] Buffer fragmentation - vertex: "
                << (vertexFrag * 100) << "%, index: "
                << (indexFrag * 100) << "%" << std::endl;

            // With FreeListAllocator, coalescing happens automatically on deallocation
            // For more aggressive defragmentation, we'd need to move allocations around
            // which requires updating all the chunks - leaving this for future optimization
        }
    }
}

float DynamicBufferPool::getAverageFragmentation() const
{
    if (bufferPairs.empty()) return 0.0f;

    float totalFragmentation = 0.0f;
    for (const auto& pair : bufferPairs)
    {
        totalFragmentation += pair->vertexAllocator->getFragmentation();
        totalFragmentation += pair->indexAllocator->getFragmentation();
    }

    return totalFragmentation / (bufferPairs.size() * 2.0f);
}

size_t DynamicBufferPool::getTotalAllocatedMemory() const
{
    return bufferPairs.size() * TOTAL_PAIR_SIZE;
}