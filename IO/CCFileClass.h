#pragma once

// CCFileClass (and the rest of the file I/O class hierarchy: RawFileClass,
// BufferIOFileClass, CDFileClass, FileFindClass, FileSystem) is defined in
// IO/FileSystem.h. This header is retained for the conventional include path
// (<IO/CCFileClass.h>) and simply re-exports the real definition without
// duplicating the class.
#include "FileSystem.h"
