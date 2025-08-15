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

    std::cout << "[BUDDY] Root block - offset: " << root->offset
        << ", size: " << root->size << std::endl;
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
    size_t blockSize = totalSize;
    for (int level = 0; level <= maxLevel; level++)
    {
        if (blockSize <= size)
        {
            return level;
        }
        blockSize /= 2;
    }
    return maxLevel;
}

DynamicBufferPool::BuddyAllocator::Block*
DynamicBufferPool::BuddyAllocator::allocate(size_t size)
{
    size_t requiredSize = roundUpToPowerOfTwo(size);
    int targetLevel = getSizeLevel(requiredSize);

    // Find a free block at target level or higher
    for (int level = targetLevel; level >= 0; level--)
    {
        if (!freeLists[level].empty())
        {
            Block* block = freeLists[level].back();
            freeLists[level].pop_back();

            // Split block if it's larger than needed
            while (block->level < targetLevel)
            {
                splitBlock(block);
                // Use left child, right child goes to free list
                block = block->left;
            }

            block->isFree = false;

            std::cout << "[BUDDY] Allocated block at offset: " << block->offset
                << ", size: " << block->size << std::endl;

            return block;
        }
    }

    return nullptr;  // No space available
}

void DynamicBufferPool::BuddyAllocator::splitBlock(Block* block)
{
    if (block->level >= maxLevel) return;

    size_t halfSize = block->size / 2;

    std::cout << "[BUDDY] Splitting block - parent offset: " << block->offset
        << ", size: " << block->size << std::endl;

    // Create left and right children
    block->left = createBlock(block->offset, halfSize, block->level + 1);
    block->right = createBlock(block->offset + halfSize, halfSize, block->level + 1);

    std::cout << "[BUDDY] Created left child - offset: " << block->left->offset
        << ", size: " << block->left->size << std::endl;
    std::cout << "[BUDDY] Created right child - offset: " << block->right->offset
        << ", size: " << block->right->size << std::endl;

    block->left->parent = block;
    block->right->parent = block;
    block->left->buddy = block->right;
    block->right->buddy = block->left;

    // Mark right child as free
    block->right->isFree = true;
    freeLists[block->level + 1].push_back(block->right);
}

void DynamicBufferPool::BuddyAllocator::deallocate(Block* block)
{
    if (!block || block->isFree) return;

    block->isFree = true;

    // Try to merge with buddy
    Block* current = block;
    while (current->parent && current->buddy && current->buddy->isFree)
    {
        mergeBlock(current);
        current = current->parent;
    }

    // Add to free list
    freeLists[current->level].push_back(current);
}

void DynamicBufferPool::BuddyAllocator::mergeBlock(Block* block)
{
    Block* buddy = block->buddy;
    Block* parent = block->parent;

    if (!buddy || !buddy->isFree || !parent) return;

    // Remove buddy from free list
    auto& freeList = freeLists[buddy->level];
    freeList.erase(std::remove(freeList.begin(), freeList.end(), buddy), freeList.end());

    // Remove block from free list if it's there
    auto& blockList = freeLists[block->level];
    blockList.erase(std::remove(blockList.begin(), blockList.end(), block), blockList.end());

    // Parent becomes the merged block
    parent->left = nullptr;
    parent->right = nullptr;
    parent->isFree = true;
}

bool DynamicBufferPool::BuddyAllocator::canAllocate(size_t size) const
{
    size_t requiredSize = roundUpToPowerOfTwo(size);
    int targetLevel = getSizeLevel(requiredSize);

    // Check if any free block exists at target level or higher
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
    // Collect all allocated blocks
    std::vector<Block*> allocatedBlocks;
    std::function<void(Block*)> collectAllocated = [&](Block* block) {
        if (!block) return;
        if (!block->isFree && !block->left && !block->right)
        {
            allocatedBlocks.push_back(block);
        }
        collectAllocated(block->left);
        collectAllocated(block->right);
        };
    collectAllocated(root);

    // Sort by size (largest first for better packing)
    std::sort(allocatedBlocks.begin(), allocatedBlocks.end(),
        [](const Block* a, const Block* b) { return a->size > b->size; });

    // Reset allocator
    freeLists.clear();
    freeLists.resize(maxLevel + 1);
    blockPool.clear();
    root = createBlock(0, totalSize, 0);
    root->isFree = true;
    freeLists[0].push_back(root);

    // Reallocate blocks compactly
    for (Block* oldBlock : allocatedBlocks)
    {
        Block* newBlock = allocate(oldBlock->size);
        if (newBlock && newBlock->offset != oldBlock->offset)
        {
            moveCallback(oldBlock->offset, newBlock->offset, oldBlock->size);
        }
    }
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
}

void DynamicBufferPool::BufferPair::initialize()
{
    // Create dynamic buffers
    vertexBuffer = bgfx::createDynamicVertexBuffer(
        VERTEX_BUFFER_SIZE / sizeof(Vertex),
        Vertex::getBgfxLayout(),
        BGFX_BUFFER_ALLOW_RESIZE
    );

    indexBuffer = bgfx::createDynamicIndexBuffer(
        INDEX_BUFFER_SIZE / sizeof(uint32_t),
        BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_INDEX32
    );

    drawCommandBuffer = bgfx::createDynamicIndexBuffer(
        MAX_CHUNKS_PER_BUFFER * 4 + 1,
        BGFX_BUFFER_ALLOW_RESIZE | BGFX_BUFFER_INDEX32
    );

    instanceBuffer = bgfx::createDynamicVertexBuffer(
        MAX_CHUNKS_PER_BUFFER,
        ChunkInstanceData::getBgfxLayout(),
        BGFX_BUFFER_ALLOW_RESIZE | BGFX_BUFFER_COMPUTE_WRITE
    );

    // Create indirect buffer for draw commands#
    indirectBuffer = bgfx::createIndirectBuffer(MAX_CHUNKS_PER_BUFFER);

    // Initialize buddy allocators
    vertexAllocator = std::make_unique<BuddyAllocator>(VERTEX_BUFFER_SIZE);
    indexAllocator = std::make_unique<BuddyAllocator>(INDEX_BUFFER_SIZE);

    drawCommands[0] = 0;
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
    std::cout << "Ctor DynamicBufferPool called" << std::endl;

    // Start with one buffer pair
    auto firstPair = std::make_unique<BufferPair>();
    firstPair->initialize();

    bufferPairs.push_back(std::move(firstPair));

    std::cout << "DynamicBufferPool initialized with buffer sizes:" << std::endl;
    std::cout << "  Vertex Buffer: " << VERTEX_BUFFER_SIZE << " bytes ("
        << VERTEX_BUFFER_SIZE / sizeof(Vertex) << " vertices)" << std::endl;
    std::cout << "  Index Buffer: " << INDEX_BUFFER_SIZE << " bytes ("
        << INDEX_BUFFER_SIZE / sizeof(uint32_t) << " indices)" << std::endl;
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
    for (auto& pair : bufferPairs)
    {
        if (pair->vertexAllocator->canAllocate(vertexSize) &&
            pair->indexAllocator->canAllocate(indexSize))
        {
            return pair.get();
        }
    }

    // Create new buffer pair
    std::cout << "Creating new buffer pair #" << bufferPairs.size()
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
    std::lock_guard<std::mutex> lock(poolMutex);

    size_t vertexSize = vertexCount * sizeof(Vertex);
    size_t indexSize = indexCount * sizeof(uint32_t);

    // Find or create suitable buffer pair
    BufferPair* pair = findOrCreateBufferPair(vertexSize, indexSize);
    if (!pair)
    {
        std::cerr << "Failed to allocate buffer space for chunk!" << std::endl;
        return BufferAllocation();
    }

    // Allocate from buddy allocators
    auto* vertexBlock = pair->vertexAllocator->allocate(vertexSize);
    auto* indexBlock = pair->indexAllocator->allocate(indexSize);

    if (!vertexBlock || !indexBlock)
    {
        // Cleanup partial allocation
        if (vertexBlock) pair->vertexAllocator->deallocate(vertexBlock);
        if (indexBlock) pair->indexAllocator->deallocate(indexBlock);

        // Try creating new buffer pair
        auto newPair = std::make_unique<BufferPair>();
        newPair->initialize();
        pair = newPair.get();
        bufferPairs.push_back(std::move(newPair));

        vertexBlock = pair->vertexAllocator->allocate(vertexSize);
        indexBlock = pair->indexAllocator->allocate(indexSize);

        if (!vertexBlock || !indexBlock)
        {
            std::cerr << "Failed to allocate even in new buffer!" << std::endl;
            return BufferAllocation();
        }
    }

    // Create allocation record
    BufferAllocation allocation;
    allocation.bufferPairIndex = bufferPairs.size() - 1;
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

    // Update draw commands
    pair->needsIndirectUpdate = true;

    std::cout << "[POOL] First allocation? Vertex allocator empty: "
        << (pair->vertexAllocator->freeLists[0].size() == 1) << std::endl;

    std::cout << "[POOL] Allocated vOffset: "
        << vertexBlock->offset << std::endl;

    std::cout << "[POOL] Allocated iOffset: "
        << indexBlock->offset << std::endl;

    return allocation;
}

void DynamicBufferPool::uploadToBuffer(BufferPair* pair, const BufferAllocation& allocation,
    const void* vertexData, const void* indexData)
{
    // Update vertex buffer
    if (vertexData && allocation.vertexCount > 0)
    {
        bgfx::update(pair->vertexBuffer,
            allocation.vertexOffset / sizeof(Vertex),
            bgfx::makeRef(vertexData, allocation.vertexCount * sizeof(Vertex)));
    }

    // Update index buffer
    if (indexData && allocation.indexCount > 0)
    {
        bgfx::update(pair->indexBuffer,
            allocation.indexOffset / sizeof(uint32_t),
            bgfx::makeRef(indexData, allocation.indexCount * sizeof(uint32_t)));
    }
}

void DynamicBufferPool::deallocateChunk(Chunk* chunk, const BufferAllocation& allocation)
{
    if (!allocation.isValid()) return;

    std::lock_guard<std::mutex> lock(poolMutex);

    if (allocation.bufferPairIndex >= bufferPairs.size()) return;

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

    // Reset the array to all zeros
    memset(pair->drawCommands, 0, (pair->drawCommands[0] * 4 + 1) * sizeof(uint32_t));
    memset(pair->chunkInstanceData, 0, (pair->drawCommands[0]) * sizeof(ChunkInstanceData));

    // Build draw commands from chunk allocations
    uint32_t cmdIndex = 1;
    uint32_t k = 0;
    for (const auto& [chunk, allocation] : pair->chunkAllocations)
    {
        pair->drawCommands[cmdIndex++] = allocation.indexCount;
        pair->drawCommands[cmdIndex++] = 1;
        pair->drawCommands[cmdIndex++] = allocation.indexOffset / sizeof(uint32_t);
        pair->drawCommands[cmdIndex++] = allocation.vertexOffset / sizeof(Vertex);

        pair->chunkInstanceData[k].x = chunk->getChunkX();
        pair->chunkInstanceData[k].y = chunk->getChunkY();
        pair->chunkInstanceData[k].z = chunk->getChunkZ();
        pair->chunkInstanceData[k].pad = 0;

        std::cout << "Commands in buffer: iCount: " << pair->drawCommands[cmdIndex - 4] << ", amount: " << pair->drawCommands[cmdIndex - 3] << ", iOffset: " << pair->drawCommands[cmdIndex - 2] << ", vOffset: " << pair->drawCommands[cmdIndex - 1] << std::endl;
        std::cout << "Chunk indices: " << chunk->getIndices().size() << ", vCount: " << chunk->getVertices().size()  << std::endl;
        std::cout << "---------------"  << std::endl;

        ++k;
        //break;
    }

    pair->drawCommands[0] = (cmdIndex - 1) / 4;

    // Update command buffer
    if (pair->drawCommands[0] > 0)
    {
        if (pair->drawCommands[0] > 1)
        {
            std::cout << "Commands in buffer: " << pair->drawCommands[0] << ", iCount: " << pair->drawCommands[5] << ", amount: " << pair->drawCommands[6] << ", iOffset: " << pair->drawCommands[7] << ", vOffset: " << pair->drawCommands[8] << std::endl;
        }
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

        if (pair->drawCommands[0] == 0) continue;

        bgfx::setBuffer(0, pair->drawCommandBuffer, bgfx::Access::Read);
        bgfx::setBuffer(1, pair->indirectBuffer, bgfx::Access::ReadWrite);

        bgfx::dispatch(0, computeProgram, 1, 1, 1);

        // Set buffers
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
            std::cout << "Defragmenting buffer pair (fragmentation: vertex="
                << vertexFrag << ", index=" << indexFrag << ")" << std::endl;
            defragmentPair(pair.get());
        }
    }
}

void DynamicBufferPool::defragmentPair(BufferPair* pair)
{
    // Copy current buffer data to staging
    pair->vertexStagingBuffer.resize(VERTEX_BUFFER_SIZE);
    pair->indexStagingBuffer.resize(INDEX_BUFFER_SIZE);

    // Read back current buffer data (Note: This is expensive!)
    // In production, you might want to maintain CPU-side copies

    // Map to track chunk movement
    std::unordered_map<Chunk*, BufferAllocation> newAllocations;

    // Defragment vertex buffer
    pair->vertexAllocator->defragment(
        [&](size_t oldOffset, size_t newOffset, size_t size) {
            // Move vertex data in staging buffer
            if (oldOffset != newOffset)
            {
                std::memmove(&pair->vertexStagingBuffer[newOffset],
                    &pair->vertexStagingBuffer[oldOffset], size);
            }
        });

    // Defragment index buffer
    pair->indexAllocator->defragment(
        [&](size_t oldOffset, size_t newOffset, size_t size) {
            // Move index data in staging buffer
            if (oldOffset != newOffset)
            {
                std::memmove(&pair->indexStagingBuffer[newOffset],
                    &pair->indexStagingBuffer[oldOffset], size);
            }
        });

    // Re-upload entire buffers
    bgfx::update(pair->vertexBuffer, 0,
        bgfx::makeRef(pair->vertexStagingBuffer.data(), VERTEX_BUFFER_SIZE));
    bgfx::update(pair->indexBuffer, 0,
        bgfx::makeRef(pair->indexStagingBuffer.data(), INDEX_BUFFER_SIZE));

    pair->needsIndirectUpdate = true;
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