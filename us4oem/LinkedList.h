#pragma once

/*

This header defines an universal linked list structure and interfaces to use said linked lists.

There were a few design considerations that led to this data structure:
- One of the main users of this will be the list of DMA allocations, which might get long if we have a lot of them
  (which might be the case if we need to store a lot of data = a lot of scatter-gather buffers).
- We should be able to really quickly push items to the end of the list.
- The only case in which we will get items from the list is when we need to free them, so we don't need to particularly
  optimize for that case.

The resulting structure is a simple two-way linked list with a head and a tail pointer. The structure allocates memory for each entry,
and free()'s it when removing an item from the list. It also free()'s the item itself when removing it from the list.

*/

// Memory management can be overriden by defining the macros below
#ifndef __LINKED_LIST_MM
#define __LINKED_LIST_ALLOC(size) MmAllocateNonCachedMemory(size)
#define __LINKED_LIST_FREE(ptr, size) MmFreeNonCachedMemory(ptr, size)
#define __LINKED_LIST_ZERO(ptr, size) RtlZeroMemory(ptr, size)
#define __LINKED_LIST_MM
#endif

// Defines a type to store a linked list entry for a given type.
#define LINKED_LIST_ENTRY_TYPE_FOR(Type) \
    Type##_LIST_ENTRY

// Defines a type to store a linked list entry for a given type.
#define USE_IN_LINKED_LISTS(Type) \
    typedef struct _##Type##_LIST_ENTRY { \
        struct _##Type##_LIST_ENTRY* Prev; \
        struct _##Type##_LIST_ENTRY* Next; \
        Type* Item; \
    } Type##_LIST_ENTRY, *P##Type##_LIST_ENTRY;

// Used in structs that will contain linked lists.
#define LINKED_LIST_POINTERS(Type, Name) \
    Type##_LIST_ENTRY* Name##Head; \
    Type##_LIST_ENTRY* Name##Tail;

// Used to get the head of a linked list.
#define LINKED_LIST_HEAD(Type, Where) \
    (Where##Head)

// Used to get the tail of a linked list.
#define LINKED_LIST_TAIL(Type, Where) \
    (Where##Tail)

// Pushes an item (Type*) to the end of a linked list (Type_LIST_ENTRY).
// Note that WhereHead and WhereTail could both be NULL, in which case the item will be the first and only item in the list.
#define LINKED_LIST_PUSH(Type, Where, What) \
    { \
        Type##_LIST_ENTRY* entry = (Type##_LIST_ENTRY*)__LINKED_LIST_ALLOC(sizeof(Type##_LIST_ENTRY)); \
        if (entry == NULL) { \
            return; \
        } \
        __LINKED_LIST_ZERO(entry, sizeof(Type##_LIST_ENTRY)); \
        entry->Item = What; \
        entry->Next = NULL; \
        if ((Where##Head) == NULL) { \
            (Where##Head) = entry; \
            (Where##Tail) = entry; \
        } else { \
            (Where##Tail)->Next = entry; \
            entry->Prev = (Where##Tail); \
            (Where##Tail) = entry; \
        } \
    }

// Iterates through a linked list (Type_LIST_ENTRY) - used like a for loop.
#define LINKED_LIST_FOR_EACH(Type, Where, Var) \
    for (Type##_LIST_ENTRY* Var = (Where##Head); Var != NULL; Var = Var->Next)

// Removes an entry
// Note we need to check if we're removing the head or the tail, and update the pointers accordingly.
#define LINKED_LIST_REMOVE(Type, Where, Entry) \
    { \
        if ((Entry)->Prev != NULL) { \
            (Entry)->Prev->Next = (Entry)->Next; \
        } else { \
            (Where##Head) = (Entry)->Next; \
        } \
        if ((Entry)->Next != NULL) { \
            (Entry)->Next->Prev = (Entry)->Prev; \
        } else { \
            (Where##Tail) = (Entry)->Prev; \
        } \
        __LINKED_LIST_FREE((Entry)->Item, sizeof(Type)); \
        __LINKED_LIST_FREE(Entry, sizeof(Type##_LIST_ENTRY)); \
    }

// Clears the whole linked list, freeing all entries and items.
#define LINKED_LIST_CLEAR(Type, Where) \
    { \
        Type##_LIST_ENTRY* entry = (Where##Head); \
        while (entry != NULL) { \
            Type##_LIST_ENTRY* next = entry->Next; \
            if (entry->Item) { \
                __LINKED_LIST_FREE(entry->Item, sizeof(Type)); \
            } \
            __LINKED_LIST_FREE(entry, sizeof(Type##_LIST_ENTRY)); \
            entry = next; \
        } \
        (Where##Head) = NULL; \
        (Where##Tail) = NULL; \
    }