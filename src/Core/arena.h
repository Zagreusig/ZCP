#ifndef ARENA_H
#define ARENA_H

#include <algorithm>
#include <cstddef>
#include <stdlib.h>

class ArenaAllocator {
   struct MemChunk {
      std::byte* buffer;
      std::byte* offset;
      size_t size;
      MemChunk* next = nullptr;
   };
public:
   inline explicit ArenaAllocator(size_t chunkSize) :
      m_chunkSize(chunkSize), m_head(newChunk(chunkSize)) {}

   inline ~ArenaAllocator() {
      MemChunk* c = m_head;
      while (c) {
         MemChunk* n = c->next;
         free(c->buffer);
         delete c;
         c = n;
      }
   }

   template<typename T>
   inline T* alloc() {
      if (m_head->offset + sizeof(T) > m_head->buffer + m_head->size) {
         size_t size = std::max(m_chunkSize, sizeof(T));
         MemChunk* chunk = newChunk(size);
         chunk->next = m_head;
         m_head = chunk;
      }
      void* ptr = m_head->offset;
      m_head->offset += sizeof(T);
      return new (ptr) T{};
   } 

private:
   MemChunk* newChunk(size_t size) {
      MemChunk* chunk = new MemChunk();
      chunk->buffer = static_cast<std::byte*>(malloc(size));
      chunk->offset = chunk->buffer;
      chunk->size = size;
      return chunk;
   }

   size_t m_chunkSize;
   MemChunk* m_head;
};

#endif // ARENA_H