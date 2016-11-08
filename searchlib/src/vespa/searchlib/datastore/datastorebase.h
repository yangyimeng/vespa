// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include <vector>

#include "bufferstate.h"
#include <vespa/vespalib/util/generationholder.h>
#include <vespa/searchlib/util/memoryusage.h>

namespace search {
namespace datastore {

class DataStoreBase
{
private:
    DataStoreBase(const DataStoreBase &rhs);

    DataStoreBase &
    operator=(const DataStoreBase &rhs);
protected:
    typedef vespalib::GenerationHandler::generation_t generation_t;
    typedef vespalib::GenerationHandler::sgeneration_t sgeneration_t;

    std::vector<void *> _buffers;	// For fast mapping with known types
    std::vector<uint32_t> _activeBufferIds;	// typeId -> active buffer

    // Hold list before freeze, before knowing how long elements must be held
    class ElemHold1ListElem
    {
    public:
        EntryRef _ref;
        uint64_t _len;	// Aligned length

        ElemHold1ListElem(EntryRef ref, uint64_t len)
            : _ref(ref),
              _len(len)
        {
        }
    };

    // Hold list at freeze, when knowing how long elements must be held
    class ElemHold2ListElem : public ElemHold1ListElem
    {
    public:
        generation_t _generation;

        ElemHold2ListElem(const ElemHold1ListElem &hold1,
                          generation_t generation)
            : ElemHold1ListElem(hold1),
              _generation(generation)
        {
        }
    };

    typedef vespalib::Array<ElemHold1ListElem> ElemHold1List;
    typedef std::deque<ElemHold2ListElem> ElemHold2List;

    class FallbackHold : public vespalib::GenerationHeldBase
    {
    public:
        BufferState::Alloc _buffer;
        size_t _usedElems;
        BufferTypeBase *_typeHandler;
        uint32_t _typeId;

        FallbackHold(size_t size,
                     BufferState::Alloc &&buffer,
                     size_t usedElems,
                     BufferTypeBase *typeHandler,
                     uint32_t typeId);

        virtual
        ~FallbackHold(void);
    };

    class BufferHold;

public:
    class MemStats
    {
    public:
        uint64_t _allocElems;
        uint64_t _usedElems;
        uint64_t _deadElems;
        uint64_t _holdElems;
        uint64_t _allocBytes;
        uint64_t _usedBytes;
        uint64_t _deadBytes;
        uint64_t _holdBytes;
        uint32_t _freeBuffers;
        uint32_t _activeBuffers;
        uint32_t _holdBuffers;

        MemStats(void)
            : _allocElems(0),
              _usedElems(0),
              _deadElems(0),
              _holdElems(0),
              _allocBytes(0),
              _usedBytes(0),
              _deadBytes(0),
              _holdBytes(0),
              _freeBuffers(0),
              _activeBuffers(0),
              _holdBuffers(0)
        {
        }

        MemStats &
        operator+=(const MemStats &rhs)
        {
            _allocElems += rhs._allocElems;
            _usedElems += rhs._usedElems;
            _deadElems += rhs._deadElems;
            _holdElems += rhs._holdElems;
            _allocBytes += rhs._allocBytes;
            _usedBytes += rhs._usedBytes;
            _deadBytes += rhs._deadBytes;
            _holdBytes += rhs._holdBytes;
            _freeBuffers += rhs._freeBuffers;
            _activeBuffers += rhs._activeBuffers;
            _holdBuffers += rhs._holdBuffers;
            return *this;
        }
    };

protected:
    std::vector<BufferState> _states;
    std::vector<BufferTypeBase *> _typeHandlers; // TypeId -> handler

    std::vector<BufferState::FreeListList> _freeListLists;
    bool _freeListsEnabled;

    ElemHold1List _elemHold1List;
    ElemHold2List _elemHold2List;

    uint32_t _numBuffers;
    size_t _maxClusters;

    vespalib::GenerationHolder _genHolder;

    DataStoreBase(uint32_t numBuffers, size_t maxClusters);

    virtual
    ~DataStoreBase(void);

    /**
     * Get next buffer id
     *
     * @param bufferId		current buffer id
     * @return			next buffer id
     */
    uint32_t
    nextBufferId(uint32_t bufferId)
    {
        uint32_t ret = bufferId + 1;
        if (ret == _numBuffers)
            ret = 0;
        return ret;
    }

    /**
     * Get active buffer
     *
     * @return			active buffer
     */
    void *
    activeBuffer(uint32_t typeId)
    {
        return _buffers[_activeBufferIds[typeId]];
    }

    /**
     * Trim elem hold list, freeing elements that no longer needs to be held.
     *
     * @param usedGen		lowest generation that is still used.
     */
    virtual void
    trimElemHoldList(generation_t usedGen) = 0;

    virtual void
    clearElemHoldList(void) = 0;

public:
    void
    addType(BufferTypeBase *typeHandler);

    void
    initActiveBuffers(void);

    /**
     * Ensure that active buffer has a given number of elements free at end.
     * Switch to new buffer if current buffer is too full.
     *
     * @param typeId		registered data type for buffer.
     * @param sizeNeeded	Number of elements needed to be free
     */
    void
    ensureBufferCapacity(uint32_t typeId, size_t sizeNeeded)
    {
        if (__builtin_expect(sizeNeeded >
                             _states[_activeBufferIds[typeId]].remaining(),
                             false)) {
            switchActiveBuffer(typeId, sizeNeeded);
        }
    }

    /**
     * Put buffer on hold list, as part of compaction.
     *
     * @param bufferId		Id of buffer to be held.
     */
    void
    holdBuffer(uint32_t bufferId);

    /**
     * Switch to new active buffer, typically in preparation for compaction
     * or when current active buffer no longer has free space.
     *
     * @param typeId		registered data type for buffer.
     * @param sizeNeeded	Number of elements needed to be free
     */
    void
    switchActiveBuffer(uint32_t typeId, size_t sizeNeeded);

    MemoryUsage getMemoryUsage() const;

    /**
     * Get active buffer id for the given type id.
     */
    uint32_t
    getActiveBufferId(uint32_t typeId) const
    {
        return _activeBufferIds[typeId];
    }

    const BufferState &
    getBufferState(uint32_t bufferId) const
    {
        return _states[bufferId];
    }

    BufferState &
    getBufferState(uint32_t bufferId)
    {
        return _states[bufferId];
    }

    uint32_t
    getNumBuffers(void) const
    {
        return _numBuffers;
    }

    bool
    hasElemHold1(void) const
    {
        return !_elemHold1List.empty();
    }

    /**
     * Transfer element holds from hold1 list to hold2 list.
     */
    void
    transferElemHoldList(generation_t generation);

    /**
     * Transfer holds from hold1 to hold2 lists, assigning generation.
     */
    void
    transferHoldLists(generation_t generation);

    /**
     * Hold of buffer has ended.
     */
    void
    doneHoldBuffer(uint32_t bufferId);

    /**
     * Trim hold lists, freeing buffers that no longer needs to be held.
     *
     * @param usedGen		lowest generation that is still used.
     */
    void
    trimHoldLists(generation_t usedGen);

    void
    clearHoldLists(void);

    template <typename EntryType>
    EntryType *
    getBufferEntry(uint32_t bufferId, uint64_t offset)
    {
        return static_cast<EntryType *>(_buffers[bufferId]) +
            offset;
    }

    template <typename EntryType>
    const EntryType *
    getBufferEntry(uint32_t bufferId, uint64_t offset) const
    {
        return static_cast<const EntryType *>(_buffers[bufferId]) +
            offset;
    }

    void
    dropBuffers(void);


    void
    incDead(uint32_t bufferId, uint64_t dead)
    {
        BufferState &state = _states[bufferId];
        state._deadElems += dead;
    }

    /**
     * Enable free list management.  This only works for fixed size elements.
     */
    void
    enableFreeLists(void);

    /**
     * Disable free list management.
     */
    void
    disableFreeLists(void);

    /**
     * Enable free list management.  This only works for fixed size elements.
     */
    void
    enableFreeList(uint32_t bufferId);

    /**
     * Disable free list management.
     */
    void
    disableFreeList(uint32_t bufferId);

    void
    disableElemHoldList(void);

    /**
     * Returns the free list for the given type id.
     */
    BufferState::FreeListList &
    getFreeList(uint32_t typeId)
    {
        return _freeListLists[typeId];
    }

    MemStats
    getMemStats(void) const;

    /**
     * Switch buffer state to active.
     *
     * @param bufferId		Id of buffer to be active.
     * @param typeId		registered data type for buffer.
     * @param sizeNeeded	Number of elements needed to be free
     * @param maxSize		number of clusters expressable via reference
     * 				type
     */
    void
    onActive(uint32_t bufferId, uint32_t typeId,
             size_t sizeNeeded,
             size_t maxSize);

    uint32_t
    getTypeId(uint32_t bufferId) const
    {
        return _states[bufferId].getTypeId();
    }

    std::vector<uint32_t>
    startCompact(uint32_t typeId);

    void
    finishCompact(const std::vector<uint32_t> &toHold);

    void
    fallbackResize(uint32_t bufferId,
                   uint64_t newSize);

    vespalib::GenerationHolder &
    getGenerationHolder(void)
    {
        return _genHolder;
    }

    uint32_t startCompactWorstBuffer(uint32_t typeId);
};


} // namespace datastore
} // namespace search
