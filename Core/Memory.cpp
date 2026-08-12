#include "Core/Memory.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <atomic>
#include <new>
#include <cstdint>

// ============================================================================
// Memory.cpp - Game memory management implementation
// ============================================================================
// Standalone engine reconstruction of YRMemory.
// In the original game these call the game's own CRT allocator at:
//   Allocate:   0x7C8E17
//   Deallocate: 0x7C8B3D
// Here we implement a standalone wrapper around malloc/free with additional
// production-grade facilities: aligned allocation, fixed-size pool
// allocators, per-allocation tracking with synthetic callstacks, leak
// detection, heap validation and a statistics dump.
// ============================================================================

namespace YRMemory
{

// ----------------------------------------------------------------------------
// LeakCallback - invoked once per live allocation by DetectLeaks. The engine
// typically forwards these to a logging sink; passing nullptr suppresses the
// per-allocation callback and only the count is returned.
// ----------------------------------------------------------------------------
using LeakCallback = void (*)(void* pointer, size_t size, uint32_t tag,
                              uint32_t sequence, const uint32_t* callstack,
                              int callstackDepth, void* user);

// ----------------------------------------------------------------------------
// Global allocation statistics (thread-safe)
// ----------------------------------------------------------------------------
static std::atomic<size_t> g_AllocatedBytes{0};
static std::atomic<size_t> g_AllocationCount{0};
static std::atomic<size_t> g_DeallocationCount{0};
static std::atomic<size_t> g_PeakAllocatedBytes{0};
static std::atomic<size_t> g_TotalAllocatedBytes{0};
static std::atomic<size_t> g_AlignedAllocationCount{0};
static std::atomic<size_t> g_PoolAllocationCount{0};
static std::atomic<size_t> g_PoolDeallocationCount{0};
static std::atomic<size_t> g_AllocationSequence{0};

// ----------------------------------------------------------------------------
// Allocation tracking record.
//
// In a real engine the callstack would be captured via StackWalk64/CaptureStackBackTrace.
// Because the standalone build targets -fno-exceptions/-fno-rtti and does not link
// against dbghelp, we synthesize a deterministic pseudo-callstack from the allocation
// sequence number and the requested size. This gives every allocation a stable,
// reproducible identity that the leak detector can report.
// ----------------------------------------------------------------------------
struct AllocationRecord
{
    void*       Pointer;        // User-visible pointer returned to the caller
    size_t      Size;           // Requested size in bytes (excluding header)
    size_t      Alignment;      // Alignment used (0 for plain malloc)
    uint32_t    Sequence;       // Monotonic allocation sequence number
    uint32_t    Tag;            // User-supplied tag (typically a CRC of __FILE__)
    uint32_t    Callstack[8];   // Synthetic callstack fingerprint
    bool        IsPool;         // True if sourced from a PoolAllocator
    bool        IsAligned;      // True if produced by AllocateAligned
    AllocationRecord* Next;     // Intrusive singly-linked list
    AllocationRecord* Prev;
};

// Head of a doubly-linked list of live allocation records. The list is
// protected by a simple spinlock-style flag (we use a busy-wait atomic
// exchange because the engine is single-threaded for allocation purposes
// during gameplay, and the overhead must be deterministic).
static AllocationRecord* g_RecordHead = nullptr;
static AllocationRecord* g_RecordTail = nullptr;
static std::atomic<uint32_t> g_TrackingLock{0};

struct ScopedTrackingLock
{
    ScopedTrackingLock()
    {
        uint32_t expected = 0;
        while (!g_TrackingLock.compare_exchange_weak(expected, 1,
                    std::memory_order_acquire)) {
            expected = 0;
        }
    }
    ~ScopedTrackingLock()
    {
        g_TrackingLock.store(0, std::memory_order_release);
    }
};

// Insert/remove a record from the tracking list. O(1) operations.
static void InsertRecord(AllocationRecord* rec)
{
    ScopedTrackingLock lock;
    rec->Next = nullptr;
    rec->Prev = g_RecordTail;
    if (g_RecordTail) {
        g_RecordTail->Next = rec;
    } else {
        g_RecordHead = rec;
    }
    g_RecordTail = rec;
}

static void RemoveRecord(AllocationRecord* rec)
{
    ScopedTrackingLock lock;
    if (rec->Prev) {
        rec->Prev->Next = rec->Next;
    } else {
        g_RecordHead = rec->Next;
    }
    if (rec->Next) {
        rec->Next->Prev = rec->Prev;
    } else {
        g_RecordTail = rec->Prev;
    }
    rec->Next = nullptr;
    rec->Prev = nullptr;
}

// Build a deterministic synthetic callstack for an allocation. The values
// mix the sequence number, tag and size so identical allocations from
// different sites produce different fingerprints.
static void BuildSyntheticCallstack(uint32_t seq, uint32_t tag, size_t size,
                                    uint32_t out[8])
{
    uint32_t s = seq ^ 0x5A5A5A5Au;
    uint32_t mix = static_cast<uint32_t>(size) ^ tag ^ (seq * 2654435761u);
    for (int i = 0; i < 8; ++i) {
        mix ^= mix << 13;
        mix ^= mix >> 17;
        mix ^= mix << 5;
        s   ^= mix + (i * 0x9E3779B9u);
        out[i] = s;
    }
}

// ----------------------------------------------------------------------------
// AllocateChecked - Allocate with null-check and abort on failure
// ----------------------------------------------------------------------------
void* AllocateChecked(size_t size) noexcept
{
    // Zero-size allocation: return a small non-null sentinel
    if (size == 0) {
        size = 1;
    }

    void* ptr = Allocate(size);
    if (!ptr) {
        // Out of memory - abort with error code
        // Original game uses exit code 0x30000000 | size
        std::abort();
    }
    return ptr;
}

// ----------------------------------------------------------------------------
// GetAllocatedBytes - Return current total allocated bytes
// ----------------------------------------------------------------------------
size_t GetAllocatedBytes() noexcept
{
    return g_AllocatedBytes.load(std::memory_order_relaxed);
}

// ----------------------------------------------------------------------------
// GetAllocationCount - Return total number of allocations performed
// ----------------------------------------------------------------------------
size_t GetAllocationCount() noexcept
{
    return g_AllocationCount.load(std::memory_order_relaxed);
}

// ----------------------------------------------------------------------------
// GetDeallocationCount - Return total number of deallocations performed
// ----------------------------------------------------------------------------
size_t GetDeallocationCount() noexcept
{
    return g_DeallocationCount.load(std::memory_order_relaxed);
}

// ----------------------------------------------------------------------------
// GetPeakAllocatedBytes - Return peak memory usage
// ----------------------------------------------------------------------------
size_t GetPeakAllocatedBytes() noexcept
{
    return g_PeakAllocatedBytes.load(std::memory_order_relaxed);
}

// ----------------------------------------------------------------------------
// GetTotalAllocatedBytes - Return cumulative bytes allocated (lifetime)
// ----------------------------------------------------------------------------
size_t GetTotalAllocatedBytes() noexcept
{
    return g_TotalAllocatedBytes.load(std::memory_order_relaxed);
}

size_t GetAlignedAllocationCount() noexcept
{
    return g_AlignedAllocationCount.load(std::memory_order_relaxed);
}

size_t GetPoolAllocationCount() noexcept
{
    return g_PoolAllocationCount.load(std::memory_order_relaxed);
}

size_t GetPoolDeallocationCount() noexcept
{
    return g_PoolDeallocationCount.load(std::memory_order_relaxed);
}

// ----------------------------------------------------------------------------
// Internal tracking update - called from Allocate/Deallocate
// ----------------------------------------------------------------------------
void TrackAllocation(size_t size) noexcept
{
    g_AllocationCount.fetch_add(1, std::memory_order_relaxed);
    size_t current = g_AllocatedBytes.fetch_add(size, std::memory_order_relaxed) + size;
    g_TotalAllocatedBytes.fetch_add(size, std::memory_order_relaxed);

    // Update peak
    size_t peak = g_PeakAllocatedBytes.load(std::memory_order_relaxed);
    while (current > peak) {
        if (g_PeakAllocatedBytes.compare_exchange_weak(peak, current,
                std::memory_order_relaxed)) {
            break;
        }
    }
}

void TrackDeallocation(size_t size) noexcept
{
    g_DeallocationCount.fetch_add(1, std::memory_order_relaxed);
    g_AllocatedBytes.fetch_sub(size, std::memory_order_relaxed);
}

// ----------------------------------------------------------------------------
// Tagged allocation tracking. Records a live allocation in the global tracking
// list so it can be enumerated for leak detection and statistics. The returned
// pointer is the user pointer; the record header is stashed just before it.
// ----------------------------------------------------------------------------
static inline AllocationRecord* RecordForPointer(void* userPtr)
{
    return reinterpret_cast<AllocationRecord*>(
        static_cast<uint8*>(userPtr) - sizeof(AllocationRecord));
}

static inline void* PointerForRecord(AllocationRecord* rec)
{
    return reinterpret_cast<uint8*>(rec) + sizeof(AllocationRecord);
}

void* AllocateTracked(size_t size, uint32_t tag) noexcept
{
    if (size == 0) size = 1;

    // Allocate room for the record header plus user payload.
    size_t total = sizeof(AllocationRecord) + size;
    AllocationRecord* rec = static_cast<AllocationRecord*>(std::malloc(total));
    if (!rec) return nullptr;
    std::memset(rec, 0, sizeof(AllocationRecord));
    std::memset(PointerForRecord(rec), 0, size);

    rec->Pointer   = PointerForRecord(rec);
    rec->Size      = size;
    rec->Alignment = 0;
    rec->Sequence  = static_cast<uint32_t>(g_AllocationSequence.fetch_add(1, std::memory_order_relaxed));
    rec->Tag       = tag;
    rec->IsPool    = false;
    rec->IsAligned = false;
    BuildSyntheticCallstack(rec->Sequence, tag, size, rec->Callstack);

    InsertRecord(rec);
    TrackAllocation(size);
    return rec->Pointer;
}

void DeallocateTracked(void* ptr) noexcept
{
    if (!ptr) return;
    AllocationRecord* rec = RecordForPointer(ptr);
    size_t sz = rec->Size;
    RemoveRecord(rec);
    TrackDeallocation(sz);
    std::free(rec);
}

// ----------------------------------------------------------------------------
// AllocateAligned - allocate memory aligned to a power-of-two boundary.
// We use a header-prefix scheme so DeallocateAligned can recover the original
// malloc pointer without depending on aligned_free (portable across toolchains).
// ----------------------------------------------------------------------------
void* AllocateAligned(size_t size, size_t alignment) noexcept
{
    if (size == 0) size = 1;

    // Round alignment up to a valid power of two, minimum sizeof(void*).
    if (alignment == 0) alignment = sizeof(void*);
    if (alignment < sizeof(void*)) alignment = sizeof(void*);
    // Guarantee power-of-two.
    size_t a = alignment;
    while (a & (a - 1)) {
        a = (a | (a - 1)) + 1;
    }
    alignment = a;

    size_t total = size + alignment + sizeof(void*);
    void* raw = std::malloc(total);
    if (!raw) return nullptr;

    // Find the aligned address with room for one pointer of bookkeeping before it.
    uintptr_t base = reinterpret_cast<uintptr_t>(raw) + sizeof(void*);
    uintptr_t aligned = (base + alignment - 1) & ~(alignment - 1);
    void** bookkeeping = reinterpret_cast<void**>(aligned - sizeof(void*));
    *bookkeeping = raw;

    std::memset(reinterpret_cast<void*>(aligned), 0, size);

    g_AlignedAllocationCount.fetch_add(1, std::memory_order_relaxed);
    TrackAllocation(size);
    return reinterpret_cast<void*>(aligned);
}

void DeallocateAligned(void* ptr, size_t size) noexcept
{
    if (!ptr) return;
    void** bookkeeping = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(ptr) - sizeof(void*));
    void* raw = *bookkeeping;
    if (size > 0) {
        TrackDeallocation(size);
    }
    std::free(raw);
}

// ============================================================================
// PoolAllocator - fixed-size block pool.
//
// A PoolAllocator hands out fixed-size blocks from a contiguous backing slab.
// Free blocks are kept on an intrusive free-list. This is the pattern the
// original game uses for high-frequency per-class allocations (e.g. CellClass,
// InfantryClass). The pool is not thread-safe; callers must serialise.
// ============================================================================
struct PoolBlock
{
    PoolBlock* Next;
};

struct PoolAllocator
{
    size_t      BlockSize;
    size_t      BlockAlign;
    size_t      BlocksPerSlab;
    PoolBlock*  FreeList;
    uint8**     Slabs;
    size_t      SlabCount;
    size_t      SlabCapacity;
    size_t      AllocatedBlocks;
    size_t      TotalBlocks;
};

static const size_t kDefaultBlocksPerSlab = 256;

PoolAllocator* PoolAllocator_Create(size_t blockSize, size_t blockAlign,
                                    size_t blocksPerSlab) noexcept
{
    if (blockSize == 0) return nullptr;
    if (blockAlign == 0) blockAlign = sizeof(void*);
    if (blocksPerSlab == 0) blocksPerSlab = kDefaultBlocksPerSlab;

    // Round blockSize up to a multiple of blockAlign and ensure room for the
    // PoolBlock freelist header.
    size_t alignedBlock = ((blockSize + sizeof(PoolBlock) + blockAlign - 1) / blockAlign) * blockAlign;
    if (alignedBlock < sizeof(PoolBlock)) alignedBlock = sizeof(PoolBlock);

    PoolAllocator* pool = static_cast<PoolAllocator*>(std::malloc(sizeof(PoolAllocator)));
    if (!pool) return nullptr;
    std::memset(pool, 0, sizeof(PoolAllocator));
    pool->BlockSize      = alignedBlock;
    pool->BlockAlign     = blockAlign;
    pool->BlocksPerSlab  = blocksPerSlab;
    pool->FreeList       = nullptr;
    pool->Slabs          = nullptr;
    pool->SlabCount      = 0;
    pool->SlabCapacity   = 0;
    pool->AllocatedBlocks = 0;
    pool->TotalBlocks    = 0;
    return pool;
}

void PoolAllocator_Destroy(PoolAllocator* pool) noexcept
{
    if (!pool) return;
    for (size_t i = 0; i < pool->SlabCount; ++i) {
        if (pool->Slabs[i]) {
            std::free(pool->Slabs[i]);
        }
    }
    if (pool->Slabs) {
        std::free(pool->Slabs);
    }
    std::memset(pool, 0, sizeof(PoolAllocator));
    std::free(pool);
}

static bool PoolAllocator_GrowSlab(PoolAllocator* pool) noexcept
{
    if (!pool) return false;
    if (pool->SlabCount >= pool->SlabCapacity) {
        size_t newCap = pool->SlabCapacity == 0 ? 4 : pool->SlabCapacity * 2;
        uint8** newSlabs = static_cast<uint8**>(std::malloc(newCap * sizeof(uint8*)));
        if (!newSlabs) return false;
        if (pool->Slabs && pool->SlabCount > 0) {
            std::memcpy(newSlabs, pool->Slabs, pool->SlabCount * sizeof(uint8*));
        }
        if (pool->Slabs) std::free(pool->Slabs);
        pool->Slabs = newSlabs;
        pool->SlabCapacity = newCap;
    }

    size_t slabBytes = pool->BlockSize * pool->BlocksPerSlab;
    uint8* slab = static_cast<uint8*>(std::malloc(slabBytes));
    if (!slab) return false;
    std::memset(slab, 0, slabBytes);

    // Carve the slab into blocks and push onto the free list.
    for (size_t i = 0; i < pool->BlocksPerSlab; ++i) {
        PoolBlock* blk = reinterpret_cast<PoolBlock*>(slab + i * pool->BlockSize);
        blk->Next = pool->FreeList;
        pool->FreeList = blk;
    }
    pool->Slabs[pool->SlabCount++] = slab;
    pool->TotalBlocks += pool->BlocksPerSlab;
    return true;
}

void* PoolAllocator_Alloc(PoolAllocator* pool) noexcept
{
    if (!pool) return nullptr;
    if (!pool->FreeList) {
        if (!PoolAllocator_GrowSlab(pool)) return nullptr;
    }
    PoolBlock* blk = pool->FreeList;
    pool->FreeList = blk->Next;
    blk->Next = nullptr;
    std::memset(blk, 0, pool->BlockSize);
    ++pool->AllocatedBlocks;
    g_PoolAllocationCount.fetch_add(1, std::memory_order_relaxed);
    TrackAllocation(pool->BlockSize);
    return blk;
}

void PoolAllocator_Free(PoolAllocator* pool, void* ptr) noexcept
{
    if (!pool || !ptr) return;
    PoolBlock* blk = static_cast<PoolBlock*>(ptr);
    // Verify the pointer lies within one of our slabs before recycling.
    bool inRange = false;
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    for (size_t i = 0; i < pool->SlabCount; ++i) {
        uintptr_t base = reinterpret_cast<uintptr_t>(pool->Slabs[i]);
        uintptr_t end  = base + pool->BlockSize * pool->BlocksPerSlab;
        if (p >= base && p < end) { inRange = true; break; }
    }
    if (!inRange) return;

    blk->Next = pool->FreeList;
    pool->FreeList = blk;
    --pool->AllocatedBlocks;
    g_PoolDeallocationCount.fetch_add(1, std::memory_order_relaxed);
    TrackDeallocation(pool->BlockSize);
}

size_t PoolAllocator_FreeCount(const PoolAllocator* pool) noexcept
{
    if (!pool) return 0;
    size_t n = 0;
    for (PoolBlock* b = pool->FreeList; b; b = b->Next) ++n;
    return n;
}

size_t PoolAllocator_AllocatedCount(const PoolAllocator* pool) noexcept
{
    return pool ? pool->AllocatedBlocks : 0;
}

size_t PoolAllocator_TotalCount(const PoolAllocator* pool) noexcept
{
    return pool ? pool->TotalBlocks : 0;
}

bool PoolAllocator_Validate(const PoolAllocator* pool) noexcept
{
    if (!pool) return false;
    // Walk every slab and confirm the backing memory is non-null and the
    // recorded dimensions are internally consistent.
    for (size_t i = 0; i < pool->SlabCount; ++i) {
        if (!pool->Slabs[i]) return false;
    }
    if (pool->AllocatedBlocks > pool->TotalBlocks) return false;
    // Count free blocks and make sure allocated + free == total.
    size_t freeCnt = 0;
    for (PoolBlock* b = pool->FreeList; b; b = b->Next) {
        ++freeCnt;
        // Defensive: detect a corrupted/cyclic freelist by capping the walk.
        if (freeCnt > pool->TotalBlocks) return false;
    }
    return (freeCnt + pool->AllocatedBlocks) == pool->TotalBlocks;
}

// ============================================================================
// Heap validation - walk the live allocation list and confirm that each
// record is well-formed and still reachable.
// ============================================================================
bool ValidateHeap() noexcept
{
    ScopedTrackingLock lock;
    AllocationRecord* rec = g_RecordHead;
    size_t counted = 0;
    while (rec) {
        if (!rec->Pointer) return false;
        if (rec->Size == 0) return false;
        if (rec->Sequence == 0xFFFFFFFFu) return false;
        // Forward/prev links must be consistent.
        if (rec->Prev && rec->Prev->Next != rec) return false;
        if (rec->Next && rec->Next->Prev != rec) return false;
        if (!rec->Prev && g_RecordHead != rec) return false;
        if (!rec->Next && g_RecordTail != rec) return false;
        rec = rec->Next;
        ++counted;
        if (counted > g_AllocationCount.load(std::memory_order_relaxed) + 16) {
            return false;  // list corruption - cycle detected
        }
    }
    return true;
}

// ============================================================================
// Leak detection - enumerate every allocation still on the tracking list.
// Returns the number of leaks found and optionally writes a textual report
// to the provided FILE* (caller may pass nullptr to skip the report).
// ============================================================================
size_t DetectLeaks(LeakCallback callback, void* user) noexcept
{
    ScopedTrackingLock lock;
    size_t leaks = 0;
    AllocationRecord* rec = g_RecordHead;
    while (rec) {
        if (callback) {
            callback(rec->Pointer, rec->Size, rec->Tag, rec->Sequence,
                     rec->Callstack, 8, user);
        }
        ++leaks;
        rec = rec->Next;
    }
    return leaks;
}

// ============================================================================
// DumpStatistics - write a human-readable summary to a C-style FILE*.
// Passing nullptr writes to stdout.
// ============================================================================
void DumpStatistics(void* filePtr) noexcept
{
    FILE* f = static_cast<FILE*>(filePtr);
    if (!f) f = stdout;

    size_t current   = g_AllocatedBytes.load(std::memory_order_relaxed);
    size_t peak      = g_PeakAllocatedBytes.load(std::memory_order_relaxed);
    size_t total     = g_TotalAllocatedBytes.load(std::memory_order_relaxed);
    size_t allocs    = g_AllocationCount.load(std::memory_order_relaxed);
    size_t deallocs  = g_DeallocationCount.load(std::memory_order_relaxed);
    size_t aligned   = g_AlignedAllocationCount.load(std::memory_order_relaxed);
    size_t poolAlloc = g_PoolAllocationCount.load(std::memory_order_relaxed);
    size_t poolFree  = g_PoolDeallocationCount.load(std::memory_order_relaxed);

    std::fprintf(f, "================ YRMemory Statistics ================\n");
    std::fprintf(f, "  Current allocated bytes : %zu\n", current);
    std::fprintf(f, "  Peak allocated bytes    : %zu\n", peak);
    std::fprintf(f, "  Lifetime total bytes    : %zu\n", total);
    std::fprintf(f, "  Allocations             : %zu\n", allocs);
    std::fprintf(f, "  Deallocations           : %zu\n", deallocs);
    std::fprintf(f, "  Outstanding             : %zu\n",
                 (allocs > deallocs) ? (allocs - deallocs) : 0);
    std::fprintf(f, "  Aligned allocations     : %zu\n", aligned);
    std::fprintf(f, "  Pool allocations        : %zu\n", poolAlloc);
    std::fprintf(f, "  Pool deallocations      : %zu\n", poolFree);
    std::fprintf(f, "  Heap valid              : %s\n",
                 ValidateHeap() ? "yes" : "no");

    // Live allocation tally - counted under the lock to stay consistent.
    ScopedTrackingLock lock;
    size_t live = 0;
    for (AllocationRecord* r = g_RecordHead; r; r = r->Next) ++live;
    std::fprintf(f, "  Tracked live allocations: %zu\n", live);
    std::fprintf(f, "======================================================\n");
}

// ============================================================================
// ResetTracking - clears all tracking state. Intended for use between
// scenarios/missions so the leak detector starts from a clean baseline.
// ============================================================================
void ResetTracking() noexcept
{
    ScopedTrackingLock lock;
    g_RecordHead = nullptr;
    g_RecordTail = nullptr;
    g_AllocatedBytes.store(0, std::memory_order_relaxed);
    g_AllocationCount.store(0, std::memory_order_relaxed);
    g_DeallocationCount.store(0, std::memory_order_relaxed);
    g_PeakAllocatedBytes.store(0, std::memory_order_relaxed);
    g_TotalAllocatedBytes.store(0, std::memory_order_relaxed);
    g_AlignedAllocationCount.store(0, std::memory_order_relaxed);
    g_PoolAllocationCount.store(0, std::memory_order_relaxed);
    g_PoolDeallocationCount.store(0, std::memory_order_relaxed);
    g_AllocationSequence.store(0, std::memory_order_relaxed);
}

} // namespace YRMemory

// ============================================================================
// Global operator new/delete overloads
// These ensure all allocations go through YRMemory
// ============================================================================

void* operator new(size_t size)
{
    void* ptr = YRMemory::Allocate(size);
    if (!ptr) {
        // No exceptions - abort on allocation failure
        std::abort();
    }
    YRMemory::TrackAllocation(size);
    return ptr;
}

void* operator new[](size_t size)
{
    void* ptr = YRMemory::Allocate(size);
    if (!ptr) {
        std::abort();
    }
    YRMemory::TrackAllocation(size);
    return ptr;
}

void operator delete(void* ptr) noexcept
{
    if (ptr) {
        YRMemory::Deallocate(ptr);
        // Note: we don't track size for delete since we don't know it
    }
}

void operator delete[](void* ptr) noexcept
{
    if (ptr) {
        YRMemory::Deallocate(ptr);
    }
}

void operator delete(void* ptr, size_t size) noexcept
{
    if (ptr) {
        YRMemory::Deallocate(ptr);
        YRMemory::TrackDeallocation(size);
    }
}

void operator delete[](void* ptr, size_t size) noexcept
{
    if (ptr) {
        YRMemory::Deallocate(ptr);
        YRMemory::TrackDeallocation(size);
    }
}

// ============================================================================
// No-throw operator new (for use with -fno-exceptions)
// ============================================================================

void* operator new(size_t size, const std::nothrow_t&) noexcept
{
    void* ptr = YRMemory::Allocate(size);
    if (ptr) {
        YRMemory::TrackAllocation(size);
    }
    return ptr;
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept
{
    void* ptr = YRMemory::Allocate(size);
    if (ptr) {
        YRMemory::TrackAllocation(size);
    }
    return ptr;
}
