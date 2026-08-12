#pragma once

// DynamicVectorClass<T> is defined in Core/Memory.h (alongside the YRMemory
// allocator). This header is retained for the conventional include path
// (<Containers/DynamicVectorClass.h>) and simply pulls in the real template
// definition plus the sibling VectorClass without duplicating any code.
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
