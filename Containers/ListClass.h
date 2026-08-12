#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// GenericNode - standalone linked list node (used by INIClass, MixFileClass)
// ============================================================================

template<typename T>
struct GenericNode {
    GenericNode* Next;
    GenericNode* Prev;
    GenericNode() : Next(nullptr), Prev(nullptr) {}
    virtual ~GenericNode() = default;

    void Unlink() {
        if (Prev) Prev->Next = Next;
        if (Next) Next->Prev = Prev;
        Next = nullptr;
        Prev = nullptr;
    }
};

// ============================================================================
// Node<T> - alias for GenericNode<T> (used by INIClass, MixFileClass)
// ============================================================================

template<typename T>
using Node = GenericNode<T>;

// ============================================================================
// List<T> - standalone linked list for pointer types
// ============================================================================

template<typename T>
class List {
public:
    Node<T>* Head;
    Node<T>* Tail;
    int32 Count;

    List() : Head(nullptr), Tail(nullptr), Count(0) {}

    ~List() { Clear(); }

    void Clear() {
        Node<T>* current = Head;
        while (current) {
            Node<T>* next = current->Next;
            delete current;
            current = next;
        }
        Head = nullptr;
        Tail = nullptr;
        Count = 0;
    }

    void AddHead(T item) {
        Node<T>* node = new Node<T>();
        node->Next = Head;
        if (Head) Head->Prev = node;
        else Tail = node;
        Head = node;
        ++Count;
    }

    void AddTail(T item) {
        Node<T>* node = new Node<T>();
        node->Prev = Tail;
        if (Tail) Tail->Next = node;
        else Head = node;
        Tail = node;
        ++Count;
    }

    bool Remove(T item) {
        Node<T>* current = Head;
        while (current) {
            // For pointer types, compare by pointer
            if (current == reinterpret_cast<Node<T>*>(item)) {
                if (current->Prev) current->Prev->Next = current->Next;
                if (current->Next) current->Next->Prev = current->Prev;
                if (current == Head) Head = current->Next;
                if (current == Tail) Tail = current->Prev;
                delete current;
                --Count;
                return true;
            }
            current = current->Next;
        }
        return false;
    }

    bool IsEmpty() const { return Count == 0; }
    int32 Size() const { return Count; }
    int32 GetCount() const { return Count; }

    T First() const { return Head ? reinterpret_cast<T>(Head) : nullptr; }
    T Last() const { return Tail ? reinterpret_cast<T>(Tail) : nullptr; }
};

// ============================================================================
// ListClass<T> - more feature-rich linked list
// ============================================================================

template<typename T>
class ListClass {
public:
    struct Node {
        T Data;
        Node* Next;
        Node* Prev;
        Node() : Next(nullptr), Prev(nullptr) {}
    };

    Node* Head;
    Node* Tail;
    int32 Count;

    ListClass() : Head(nullptr), Tail(nullptr), Count(0) {}

    ~ListClass() {
        Clear();
    }

    void Clear() {
        Node* current = Head;
        while (current) {
            Node* next = current->Next;
            delete current;
            current = next;
        }
        Head = nullptr;
        Tail = nullptr;
        Count = 0;
    }

    void Add(const T& item) {
        Node* node = new Node();
        node->Data = item;
        if (!Head) {
            Head = node;
            Tail = node;
        } else {
            Tail->Next = node;
            node->Prev = Tail;
            Tail = node;
        }
        ++Count;
    }

    bool Remove(const T& item) {
        Node* current = Head;
        while (current) {
            if (current->Data == item) {
                if (current->Prev) current->Prev->Next = current->Next;
                if (current->Next) current->Next->Prev = current->Prev;
                if (current == Head) Head = current->Next;
                if (current == Tail) Tail = current->Prev;
                delete current;
                --Count;
                return true;
            }
            current = current->Next;
        }
        return false;
    }

    Node* GetHead() const { return Head; }
    Node* GetTail() const { return Tail; }
    int32 GetCount() const { return Count; }
    bool IsEmpty() const { return Count == 0; }
};