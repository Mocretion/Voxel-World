#include "DynamicBufferPool.h"
#include "Chunk.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <cmath>

// ============================================================================
// BuddyAllocator Implementation
// ============================================================================

DynamicBufferPool::BuddyAllocator::BuddyAllocator(size_t totalSize)
    : totalSize(totalSize), minBlockSize(1024)  // Minimum 1KB blocks
{
    // Calculate max level (root is level 0)
    maxLevel = 0;
    size_t size = totalSize;
    while (size > minBlockSize)
    {
        size /= 2;
        maxLevel++;
    }

    // Initialize free lists for each level
    freeLists.resize(maxLevel + 1);

    // Create root block
    root = createBlock(0, totalSize, 0);
    root->isFree = true;
    freeLists[0].push_back(root);

    std::cout << "[BUDDY] Initialized with total size: " << totalSize
        << ", max level: " << maxLevel
        << ", min block size: " << minBlockSize << std::endl;
}

DynamicBufferPool::BuddyAllocator::~BuddyAllocator()
{
    // Blocks are managed by unique_ptr in blockPool
}

DynamicBufferPool::BuddyAllocator::Block*
DynamicBufferPool::BuddyAllocator::createBlock(size_t offset, size_t size, int level)
{
    auto block = std::make_unique<Block>();
    block->offset = offset;
    block->size = size;
    block->isFree = false;
    block->buddy = nullptr;
    block->parent = nullptr;
    block->left = nullptr;
    block->right = nullptr;
    block->level = level;

    Block* ptr = block.get();
    blockPool.push_back(std::move(block));
    return ptr;
}

size_t DynamicBufferPool::BuddyAllocator::roundUpToPowerOfTwo(size_t size) const
{
    if (size <= minBlockSize) return minBlockSize;

    // Round up to next power of 2
    size_t power = 1;
    while (power < size)
    {
        power *= 2;
    }
    return power;
}

int DynamicBufferPool::BuddyAllocator::getSizeLevel(size_t size) const
{
    // FIX: Level 0 is the largest (full size), higher levels are smaller
    // We need to find the level where block size >= requested size
    size_t blockSize = totalSize;

    for (int level = 0; level <= maxLevel; level++)
    {
        if (blockSize <= size)
        {
            // This level's block size is small enough to fit the request
            return level;
        }
        blockSize /= 2;
    }

    return maxLevel;  // Smallest possible block
}

DynamicBufferPool::BuddyAllocator::Block*
DynamicBufferPool::BuddyAllocator::allocate(size_t size)
{
    size_t requiredSize = roundUpToPowerOfTwo(size);
    int targetLevel = getSizeLevel(requiredSize);

    // Find a free block at target level or lower (larger blocks)
    for (int level = targetLevel; level >= 0; level--)
    {
        if (!freeLists[level].empty())
        {
            Block* block = freeLists[level].back();
            freeLists[level].pop_back();

            // Mark as not free immediately
            block->isFree = false;

            // Split block down to target level if needed
            while (block->level < targetLevel)
            {
                // Split will add both children to free list
                splitBlock(block);

                // Take the left child from free list and use it
                block = block->left;

                // Remove the left child from free list since we're using it
                auto& childFreeList = freeLists[block->level];
                childFreeList.erase(std::remove(childFreeList.begin(), childFreeList.end(), block),
                    childFreeList.end());
                block->isFree = false;
            }

            return block;
        }
    }

    std::cerr << "[BUDDY] ALLOCATION FAILED - No space available for size: " << size << std::endl;
    return nullptr;  // No space available
}

void DynamicBufferPool::BuddyAllocator::splitBlock(Block* block)
{
    if (block->level >= maxLevel)
    {
        std::cerr << "[BUDDY] Cannot split - already at max level: " << block->level << std::endl;
        return;
    }

    // CRITICAL: Remove block from free list before splitting!
    // The parent block should not be in any free list once it has children
    auto& parentFreeList = freeLists[block->level];
    parentFreeList.erase(std::remove(parentFreeList.begin(), parentFreeList.end(), block),
        parentFreeList.end());

    size_t halfSize = block->size / 2;

    // Create left and right children
    block->left = createBlock(block->offset, halfSize, block->level + 1);
    block->right = createBlock(block->offset + halfSize, halfSize, block->level + 1);

    block->left->parent = block;
    block->right->parent = block;
    block->left->buddy = block->right;
    block->right->buddy = block->left;

    // IMPORTANT: Both children start as free until one is allocated
    block->left->isFree = true;
    block->right->isFree = true;

    // Add BOTH children to free list initially
    // The allocation function will remove the one it uses
    freeLists[block->level + 1].push_back(block->left);
    freeLists[block->level + 1].push_back(block->right);
}

void DynamicBufferPool::BuddyAllocator::deallocate(Block* block)
{
    if (!block || block->isFree)
    {
        std::cerr << "[BUDDY] Deallocate called on null or already free block" << std::endl;
        return;
    }

    std::cout << "[BUDDY] Deallocating block - offset: " << block->offset
        << ", size: " << block->size
        << ", level: " << block->level << std::endl;

    block->isFree = true;

    // Try to merge with buddy
    Block* current = block;
    while (current->parent && current->buddy && current->buddy->isFree)
    {
        std::cout << "[BUDDY] Merging with buddy at offset: " << current->buddy->offset << std::endl;

        Block* parent = current->parent;
        Block* buddy = current->buddy;

        // Remove both blocks from free lists
        auto& freeList = freeLists[current->level];
        freeList.erase(std::remove(freeList.begin(), freeList.end(), current), freeList.end());
        freeList.erase(std::remove(freeList.begin(), freeList.end(), buddy), freeList.end());

        // Clear parent's children
        parent->left = nullptr;
        parent->right = nullptr;
        parent->isFree = true;

        current = parent;
    }

    // Add the final block to free list
    freeLists[current->level].push_back(current);

    std::cout << "[BUDDY] Deallocation complete - final block at level: " << current->level
        << ", offset: " << current->offset << std::endl;
}

bool DynamicBufferPool::BuddyAllocator::canAllocate(size_t size) const
{
    size_t requiredSize = roundUpToPowerOfTwo(size);
    int targetLevel = getSizeLevel(requiredSize);

    // Check if any free block exists at target level or lower (larger)
    for (int level = targetLevel; level >= 0; level--)
    {
        if (!freeLists[level].empty())
        {
            return true;
        }
    }
    return false;
}

float DynamicBufferPool::BuddyAllocator::getFragmentation() const
{
    size_t totalFree = 0;
    size_t largestFree = 0;

    for (const auto& freeList : freeLists)
    {
        for (const Block* block : freeList)
        {
            totalFree += block->size;
            largestFree = std::max(largestFree, block->size);
        }
    }

    if (totalFree == 0) return 0.0f;
    return 1.0f - (static_cast<float>(largestFree) / static_cast<float>(totalFree));
}

void DynamicBufferPool::BuddyAllocator::defragment(
    std::function<void(size_t oldOffset, size_t newOffset, size_t size)> moveCallback)
{
    // For buddy allocator, defragmentation requires rebuilding the entire structure
    // This is complex and may not be worth it - consider if really needed
    std::cout << "[BUDDY] Defragmentation requested but not implemented for buddy allocator" << std::endl;
}

// ============================================================================
// BufferPair Implementation
// ============================================================================

DynamicBufferPool::BufferPair::BufferPair()
{
    vertexStagingBuffer.reserve(VERTEX_BUFFER_SIZE);
    indexStagingBuffer.reserve(INDEX_BUFFER_SIZE);
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

    // Initialize buddy allocators
    vertexAllocator = std::make_unique<BuddyAllocator>(VERTEX_BUFFER_SIZE);
    indexAllocator = std::make_unique<BuddyAllocator>(INDEX_BUFFER_SIZE);

    drawCommands[0] = 0;

    std::cout << "[BUFFER_PAIR] Initialization complete" << std::endl;
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
    std::cout << "[POOL] Initializing DynamicBufferPool" << std::endl;

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
            std::cout << "[POOL] Using existing buffer pair #" << i << std::endl;
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

    // Ensure proper alignment
    vertexSize = (vertexSize + 15) & ~15;  // Align to 16 bytes
    indexSize = (indexSize + 15) & ~15;

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
        std::cerr << "[POOL] CRITICAL ERROR: Could not find buffer pair index!" << std::endl;
        return BufferAllocation();
    }

    // Allocate from buddy allocators
    auto* vertexBlock = pair->vertexAllocator->allocate(vertexSize);
    auto* indexBlock = pair->indexAllocator->allocate(indexSize);

    if (!vertexBlock || !indexBlock)
    {
        std::cerr << "[POOL] Allocation failed in buddy allocators!" << std::endl;

        // Cleanup partial allocation
        if (vertexBlock) pair->vertexAllocator->deallocate(vertexBlock);
        if (indexBlock) pair->indexAllocator->deallocate(indexBlock);

        return BufferAllocation();
    }

    // Validate alignment
    if (vertexBlock->offset % sizeof(Vertex) != 0)
    {
        std::cerr << "[POOL] WARNING: Vertex offset not aligned: " << vertexBlock->offset << std::endl;
    }
    if (indexBlock->offset % sizeof(uint32_t) != 0)
    {
        std::cerr << "[POOL] WARNING: Index offset not aligned: " << indexBlock->offset << std::endl;
    }

    // Create allocation record
    BufferAllocation allocation;
    allocation.bufferPairIndex = pairIndex;  // FIX: Use actual pair index
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

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    std::cout << "[POOL] === ALLOCATION COMPLETE === " << duration << std::endl;

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
    if (!allocation.isValid()) return;

    auto startTime = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(poolMutex);

    std::cout << "\n[POOL] === DEALLOCATING CHUNK ===" << std::endl;
    std::cout << "[POOL] Chunk (" << chunk->getChunkX() << ", "
        << chunk->getChunkY() << ", " << chunk->getChunkZ() << ")" << std::endl;

    if (allocation.bufferPairIndex >= bufferPairs.size())
    {
        std::cerr << "[POOL] Invalid buffer pair index: " << allocation.bufferPairIndex << std::endl;
        return;
    }

    BufferPair* pair = bufferPairs[allocation.bufferPairIndex].get();


    // Deallocate from buddy allocators
    if (allocation.vertexBlockHandle)
    {
        pair->vertexAllocator->deallocate(
            static_cast<BuddyAllocator::Block*>(allocation.vertexBlockHandle));
    }
    if (allocation.indexBlockHandle)
    {
        pair->indexAllocator->deallocate(
            static_cast<BuddyAllocator::Block*>(allocation.indexBlockHandle));
    }

    // Remove from chunk allocations
    pair->chunkAllocations.erase(chunk);
    pair->activeChunks--;
    pair->needsIndirectUpdate = true;

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    std::cout << "[POOL] Deallocation complete. Active chunks in pair: " << pair->activeChunks << std::endl;
    std::cout << "[POOL] === DEALLOCATION COMPLETE === Active chunks in pair: " << pair->activeChunks << " Dealloc time: " << duration << std::endl;
}

DynamicBufferPool::BufferAllocation
DynamicBufferPool::updateChunk(Chunk* chunk, const BufferAllocation& oldAllocation,
    size_t newVertexCount, size_t newIndexCount,
    const void* vertexData, const void* indexData)
{
    std::cout << "[POOL] Updating chunk allocation" << std::endl;

    // If size fits in existing allocation, update in place
    if (oldAllocation.isValid() &&
        newVertexCount <= oldAllocation.vertexCount &&
        newIndexCount <= oldAllocation.indexCount)
    {
        std::cout << "[POOL] Updating in-place" << std::endl;

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
    std::cout << "[POOL] Reallocating (size doesn't fit)" << std::endl;
    deallocateChunk(chunk, oldAllocation);
    return allocateChunk(chunk, newVertexCount, newIndexCount, vertexData, indexData);
}

void DynamicBufferPool::updateIndirectBuffer(BufferPair* pair)
{
    if (!pair->needsIndirectUpdate) return;

    // Reset arrays
    memset(pair->drawCommands, 0, (MAX_CHUNKS_PER_BUFFER * 4 + 1) * sizeof(uint32_t));
    memset(pair->chunkInstanceData, 0, MAX_CHUNKS_PER_BUFFER * sizeof(ChunkInstanceData));

    std::cout << "\n[POOL] === UPDATING INDIRECT BUFFER ===" << std::endl;
    std::cout << "[POOL] Active chunks: " << pair->chunkAllocations.size() << std::endl;

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

        // Validate offsets
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

        std::cout << "[POOL] Updated buffers with " << pair->drawCommands[0] << " draw commands" << std::endl;
    }

    pair->needsIndirectUpdate = false;
    std::cout << "[POOL] === INDIRECT BUFFER UPDATE COMPLETE ===" << std::endl;
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
            std::cout << "[POOL] Buffer pair fragmentation - vertex: "
                << vertexFrag << ", index: " << indexFrag << std::endl;
            // Defragmentation for buddy allocator is complex and may not be needed
            // Consider if this is really necessary for your use case
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