#include "temp_core.h"
#include <ntstrsafe.h>

// Pool tags for memory allocation tracking
#define TEMP_POOL_TAG 'pmeT' // 'Temp' backwards
#define TEMP_CHUNK_TAG 'hCeT'
#define TEMP_HASH_TAG 'hHeT'

// Hash function using xxHash-like algorithm optimized for sector addresses
ULONG64 TempHashFunction(ULONG64 SectorAddress)
{
    // Simple but effective hash for sector addresses
    ULONG64 hash = SectorAddress;
    hash ^= hash >> 33;
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= hash >> 33;
    hash *= 0xc4ceb9fe1a85ec53ULL;
    hash ^= hash >> 33;
    return hash;
}

ULONG TempGetBucketIndex(ULONG64 Hash)
{
    return (ULONG)(Hash % TEMP_BUCKET_COUNT);
}

NTSTATUS TempInitializeBucket(PTEMP_BUCKET Bucket, ULONG MaxChunks)
{
    NTSTATUS status = STATUS_SUCCESS;

    if (!Bucket || MaxChunks == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Initialize the bucket
    RtlZeroMemory(Bucket, sizeof(TEMP_BUCKET));
    KeInitializeSpinLock(&Bucket->Lock);

    // Allocate chunk pointer array
    Bucket->Chunks = (PTEMP_CHUNK *)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        MaxChunks * sizeof(PTEMP_CHUNK),
        TEMP_POOL_TAG);

    if (!Bucket->Chunks)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Bucket->Chunks, MaxChunks * sizeof(PTEMP_CHUNK));
    Bucket->MaxChunks = MaxChunks;

    // Initialize hash table (start with reasonable size)
    Bucket->HashTableSize = MaxChunks * 2; // Load factor of 0.5
    if (Bucket->HashTableSize < 64)
    {
        Bucket->HashTableSize = 64;
    }

    Bucket->HashTable = (PTEMP_HASH_ENTRY)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        Bucket->HashTableSize * sizeof(TEMP_HASH_ENTRY),
        TEMP_HASH_TAG);

    if (!Bucket->HashTable)
    {
        ExFreePoolWithTag(Bucket->Chunks, TEMP_POOL_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Initialize hash table with empty entries
    for (ULONG i = 0; i < Bucket->HashTableSize; i++)
    {
        Bucket->HashTable[i].Key = 0;
        Bucket->HashTable[i].ChunkIndex = MAXULONG64;
        Bucket->HashTable[i].Offset = 0;
    }

    return status;
}

VOID TempCleanupBucket(PTEMP_BUCKET Bucket)
{
    if (!Bucket)
    {
        return;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&Bucket->Lock, &oldIrql);

    // Free all chunks
    if (Bucket->Chunks)
    {
        for (ULONG i = 0; i < Bucket->ChunkCount; i++)
        {
            if (Bucket->Chunks[i])
            {
                ExFreePool(Bucket->Chunks[i]);
            }
        }
        ExFreePool(Bucket->Chunks);
        Bucket->Chunks = NULL;
    }

    // Free hash table
    if (Bucket->HashTable)
    {
        ExFreePool(Bucket->HashTable);
        Bucket->HashTable = NULL;
    }

    KeReleaseSpinLock(&Bucket->Lock, oldIrql);
}

NTSTATUS TempAllocateChunk(PTEMP_BUCKET Bucket, PTEMP_CHUNK *Chunk)
{
    if (!Bucket || !Chunk)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *Chunk = NULL;

    if (Bucket->ChunkCount >= Bucket->MaxChunks)
    {
        // Need to evict oldest chunk using LRU-like policy
        InterlockedIncrement64(&Bucket->EvictionCount);

        // Find chunk with lowest generation
        ULONG64 oldestGeneration = MAXULONG64;
        ULONG oldestIndex = 0;

        for (ULONG i = 0; i < Bucket->ChunkCount; i++)
        {
            if (Bucket->Chunks[i] &&
                Bucket->Chunks[i]->Generation < oldestGeneration &&
                Bucket->Chunks[i]->RefCount == 0)
            {
                oldestGeneration = Bucket->Chunks[i]->Generation;
                oldestIndex = i;
            }
        }

        if (oldestGeneration != MAXULONG64)
        {
            // Reuse the oldest chunk
            *Chunk = Bucket->Chunks[oldestIndex];
            (*Chunk)->Generation = InterlockedIncrement64(&Bucket->Generation);
            RtlZeroMemory((*Chunk)->Data, TEMP_CHUNK_SIZE);
            return STATUS_SUCCESS;
        }

        // If no chunk can be evicted, allocation fails
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Allocate new chunk
    PTEMP_CHUNK newChunk = (PTEMP_CHUNK)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(TEMP_CHUNK),
        TEMP_CHUNK_TAG);

    if (!newChunk)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Initialize chunk
    RtlZeroMemory(newChunk, sizeof(TEMP_CHUNK));
    newChunk->Generation = InterlockedIncrement64(&Bucket->Generation);
    newChunk->RefCount = 0;

    // Add to bucket
    Bucket->Chunks[Bucket->ChunkCount] = newChunk;
    Bucket->ChunkCount++;

    *Chunk = newChunk;
    return STATUS_SUCCESS;
}

VOID TempReleaseChunk(PTEMP_BUCKET Bucket, PTEMP_CHUNK Chunk)
{
    if (!Bucket || !Chunk)
    {
        return;
    }

    InterlockedDecrement(&Chunk->RefCount);
}

NTSTATUS TempHashTableInsert(PTEMP_BUCKET Bucket, ULONG64 Key, ULONG64 ChunkIndex, ULONG64 Offset)
{
    if (!Bucket || !Bucket->HashTable)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Linear probing for collision resolution
    ULONG startIndex = (ULONG)(Key % Bucket->HashTableSize);
    ULONG index = startIndex;

    do
    {
        if (Bucket->HashTable[index].Key == 0 || Bucket->HashTable[index].Key == Key)
        {
            // Found empty slot or updating existing entry
            Bucket->HashTable[index].Key = Key;
            Bucket->HashTable[index].ChunkIndex = ChunkIndex;
            Bucket->HashTable[index].Offset = Offset;
            return STATUS_SUCCESS;
        }

        index = (index + 1) % Bucket->HashTableSize;
    } while (index != startIndex);

    // Hash table is full
    return STATUS_INSUFFICIENT_RESOURCES;
}

BOOLEAN TempHashTableLookup(PTEMP_BUCKET Bucket, ULONG64 Key, PULONG64 ChunkIndex, PULONG64 Offset)
{
    if (!Bucket || !Bucket->HashTable || !ChunkIndex || !Offset)
    {
        return FALSE;
    }

    // Linear probing for lookup
    ULONG startIndex = (ULONG)(Key % Bucket->HashTableSize);
    ULONG index = startIndex;

    do
    {
        if (Bucket->HashTable[index].Key == Key)
        {
            *ChunkIndex = Bucket->HashTable[index].ChunkIndex;
            *Offset = Bucket->HashTable[index].Offset;
            return TRUE;
        }

        if (Bucket->HashTable[index].Key == 0)
        {
            // Empty slot means key not found
            return FALSE;
        }

        index = (index + 1) % Bucket->HashTableSize;
    } while (index != startIndex);

    return FALSE;
}

NTSTATUS TempInitializeMemoryManager(PTEMP_MEMORY_MANAGER MemoryManager, ULONG64 MaxSize)
{
    NTSTATUS status = STATUS_SUCCESS;

    if (!MemoryManager || MaxSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Initialize memory manager
    RtlZeroMemory(MemoryManager, sizeof(TEMP_MEMORY_MANAGER));
    MemoryManager->MaxSize = MaxSize;

    // Calculate max chunks per bucket
    ULONG maxChunksPerBucket = (ULONG)((MaxSize / TEMP_BUCKET_COUNT) / sizeof(TEMP_CHUNK));
    if (maxChunksPerBucket < 1)
    {
        maxChunksPerBucket = 1;
    }

    // Initialize all buckets
    for (ULONG i = 0; i < TEMP_BUCKET_COUNT; i++)
    {
        status = TempInitializeBucket(&MemoryManager->Buckets[i], maxChunksPerBucket);
        if (!NT_SUCCESS(status))
        {
            // Cleanup already initialized buckets
            for (ULONG j = 0; j < i; j++)
            {
                TempCleanupBucket(&MemoryManager->Buckets[j]);
            }
            return status;
        }
    }

    return status;
}

VOID TempCleanupMemoryManager(PTEMP_MEMORY_MANAGER MemoryManager)
{
    if (!MemoryManager)
    {
        return;
    }

    // Cleanup all buckets
    for (ULONG i = 0; i < TEMP_BUCKET_COUNT; i++)
    {
        TempCleanupBucket(&MemoryManager->Buckets[i]);
    }

    RtlZeroMemory(MemoryManager, sizeof(TEMP_MEMORY_MANAGER));
}

NTSTATUS TempReadSectors(PTEMP_MEMORY_MANAGER MemoryManager, ULONG64 StartSector, ULONG SectorCount, PVOID Buffer, ULONG SectorSize)
{
    if (!MemoryManager || !Buffer || SectorCount == 0 || SectorSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    InterlockedIncrement64(&MemoryManager->TotalReads);

    PUCHAR bufferPtr = (PUCHAR)Buffer;
    NTSTATUS status = STATUS_SUCCESS;

    for (ULONG i = 0; i < SectorCount; i++)
    {
        ULONG64 sectorAddress = StartSector + i;
        ULONG64 hash = TempHashFunction(sectorAddress);
        ULONG bucketIndex = TempGetBucketIndex(hash);
        PTEMP_BUCKET bucket = &MemoryManager->Buckets[bucketIndex];

        KIRQL oldIrql;
        KeAcquireSpinLock(&bucket->Lock, &oldIrql);

        ULONG64 chunkIndex, offset;
        if (TempHashTableLookup(bucket, hash, &chunkIndex, &offset))
        {
            // Cache hit
            InterlockedIncrement64(&bucket->HitCount);
            InterlockedIncrement64(&MemoryManager->TotalHits);

            if (chunkIndex < bucket->ChunkCount && bucket->Chunks[chunkIndex])
            {
                PTEMP_CHUNK chunk = bucket->Chunks[chunkIndex];
                InterlockedIncrement(&chunk->RefCount);

                // Update generation for LRU
                chunk->Generation = InterlockedIncrement64(&bucket->Generation);

                // Copy data
                if (offset + SectorSize <= TEMP_CHUNK_SIZE)
                {
                    RtlCopyMemory(bufferPtr + (i * SectorSize),
                                  chunk->Data + offset,
                                  SectorSize);
                }
                else
                {
                    // Data spans multiple chunks or is invalid
                    RtlZeroMemory(bufferPtr + (i * SectorSize), SectorSize);
                }

                TempReleaseChunk(bucket, chunk);
            }
            else
            {
                // Invalid chunk index, treat as miss
                InterlockedIncrement64(&bucket->MissCount);
                InterlockedIncrement64(&MemoryManager->TotalMisses);
                RtlZeroMemory(bufferPtr + (i * SectorSize), SectorSize);
            }
        }
        else
        {
            // Cache miss - return zeros (uninitialized data)
            InterlockedIncrement64(&bucket->MissCount);
            InterlockedIncrement64(&MemoryManager->TotalMisses);
            RtlZeroMemory(bufferPtr + (i * SectorSize), SectorSize);
        }

        KeReleaseSpinLock(&bucket->Lock, oldIrql);
    }

    return status;
}

NTSTATUS TempWriteSectors(PTEMP_MEMORY_MANAGER MemoryManager, ULONG64 StartSector, ULONG SectorCount, PVOID Buffer, ULONG SectorSize)
{
    if (!MemoryManager || !Buffer || SectorCount == 0 || SectorSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    InterlockedIncrement64(&MemoryManager->TotalWrites);

    PUCHAR bufferPtr = (PUCHAR)Buffer;
    NTSTATUS status = STATUS_SUCCESS;

    for (ULONG i = 0; i < SectorCount; i++)
    {
        ULONG64 sectorAddress = StartSector + i;
        ULONG64 hash = TempHashFunction(sectorAddress);
        ULONG bucketIndex = TempGetBucketIndex(hash);
        PTEMP_BUCKET bucket = &MemoryManager->Buckets[bucketIndex];

        KIRQL oldIrql;
        KeAcquireSpinLock(&bucket->Lock, &oldIrql);

        ULONG64 chunkIndex, offset;
        PTEMP_CHUNK chunk = NULL;

        if (TempHashTableLookup(bucket, hash, &chunkIndex, &offset))
        {
            // Update existing chunk
            if (chunkIndex < bucket->ChunkCount && bucket->Chunks[chunkIndex])
            {
                chunk = bucket->Chunks[chunkIndex];
                InterlockedIncrement(&chunk->RefCount);
            }
        }
        else
        {
            // Allocate new chunk
            NTSTATUS allocStatus = TempAllocateChunk(bucket, &chunk);
            if (NT_SUCCESS(allocStatus) && chunk)
            {
                // Find a suitable offset in the chunk
                offset = 0; // For simplicity, use start of chunk

                // Add to hash table
                for (ULONG j = 0; j < bucket->ChunkCount; j++)
                {
                    if (bucket->Chunks[j] == chunk)
                    {
                        TempHashTableInsert(bucket, hash, j, offset);
                        break;
                    }
                }

                InterlockedIncrement(&chunk->RefCount);
            }
        }

        if (chunk)
        {
            // Update generation for LRU
            chunk->Generation = InterlockedIncrement64(&bucket->Generation);

            // Copy data
            if (offset + SectorSize <= TEMP_CHUNK_SIZE)
            {
                RtlCopyMemory(chunk->Data + offset,
                              bufferPtr + (i * SectorSize),
                              SectorSize);
            }

            TempReleaseChunk(bucket, chunk);
        }
        else
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }

        KeReleaseSpinLock(&bucket->Lock, oldIrql);

        if (!NT_SUCCESS(status))
        {
            break;
        }
    }

    return status;
}

NTSTATUS TempFormatDisk(PTEMP_MEMORY_MANAGER MemoryManager, ULONG64 DiskSize, ULONG SectorSize)
{
    if (!MemoryManager || DiskSize == 0 || SectorSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Clear all buckets to simulate disk format
    for (ULONG i = 0; i < TEMP_BUCKET_COUNT; i++)
    {
        PTEMP_BUCKET bucket = &MemoryManager->Buckets[i];

        KIRQL oldIrql;
        KeAcquireSpinLock(&bucket->Lock, &oldIrql);

        // Clear hash table
        for (ULONG j = 0; j < bucket->HashTableSize; j++)
        {
            bucket->HashTable[j].Key = 0;
            bucket->HashTable[j].ChunkIndex = MAXULONG64;
            bucket->HashTable[j].Offset = 0;
        }

        // Clear all chunks
        for (ULONG j = 0; j < bucket->ChunkCount; j++)
        {
            if (bucket->Chunks[j])
            {
                RtlZeroMemory(bucket->Chunks[j]->Data, TEMP_CHUNK_SIZE);
                bucket->Chunks[j]->Generation = 0;
                bucket->Chunks[j]->RefCount = 0;
            }
        }

        // Reset statistics
        bucket->HitCount = 0;
        bucket->MissCount = 0;
        bucket->EvictionCount = 0;
        bucket->Generation = 0;

        KeReleaseSpinLock(&bucket->Lock, oldIrql);
    }

    // Reset global statistics
    MemoryManager->TotalReads = 0;
    MemoryManager->TotalWrites = 0;
    MemoryManager->TotalHits = 0;
    MemoryManager->TotalMisses = 0;

    return STATUS_SUCCESS;
}