#pragma once

// ============================================================================
// Node<T> - self-contained doubly-linked list node carrying a value of type T.
// Used by generic linked-list containers. (The inheritable GenericNode<T> base
// used by INIClass/MixFileClass lives in Containers/ListClass.h.)
// ============================================================================
template<typename T> struct Node {
    T Data;
    Node* Next;
    Node* Prev;
    Node() : Next(nullptr), Prev(nullptr) {}
    Node(T d) : Data(d), Next(nullptr), Prev(nullptr) {}
};
