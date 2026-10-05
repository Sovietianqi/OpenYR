#include "VoxClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdlib>

// ============================================================================
// VoxClass - EVA / speech line registry (ra2md.ini [DialogList])
// ============================================================================

DynamicVectorClass<VoxClass*>* VoxClass::Array = nullptr;

VoxClass::VoxClass()
    : Volume(1.0f)
    , Priority(Priority_Normal)
    , Type(VoxType_Standard)
    , field_50(2)
{
    ID[0]     = '\0';
    Yuri[0]   = '\0';
    Russian[0] = '\0';
    Allied[0] = '\0';
}

VoxClass::~VoxClass()
{
}

// ============================================================================
// VoxClass_LoadFromINI - asm 0x752F6E..0x752FE9 (VoxClass_LoadFromINI)
//
//   INIClass_Reset is called first, then the section is located by this
//   object's ID.  A missing section leaves the object untouched and returns
//   false.  Every string is read into a 0x1F4 byte scratch buffer and the
//   three side names are copied with a 9-byte strncpy.
// ============================================================================
bool VoxClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return false;
    }

    const char* pSection = ID;
    Volume = 1.0f;

    if (pINI->GetSection(pSection) == nullptr) {
        return false;
    }

    char buffer[0x1F4];
    buffer[0] = '\0';

    Volume = static_cast<float>(pINI->ReadFixed(pSection, "Volume", Volume));

    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "Type", "", buffer, sizeof(buffer)) > 0)
    {
        if (_strcmpi(buffer, "QUEUE") == 0) {
            Type = VoxType_Queue;
        } else if (_strcmpi(buffer, "STANDARD") == 0) {
            Type = VoxType_Standard;
        } else if (_strcmpi(buffer, "INTERRUPT") == 0) {
            Type = VoxType_Interrupt;
        } else if (_strcmpi(buffer, "QUEUED_INTERRUPT") == 0) {
            Type = VoxType_QueuedInterrupt;
        }
    }

    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "Priority", "", buffer, sizeof(buffer)) > 0)
    {
        if (_strcmpi(buffer, "LOW") == 0) {
            Priority = Priority_Low;
        } else if (_strcmpi(buffer, "NORMAL") == 0) {
            Priority = Priority_Normal;
        } else if (_strcmpi(buffer, "IMPORTANT") == 0) {
            Priority = Priority_Important;
        } else if (_strcmpi(buffer, "CRITICAL") == 0) {
            Priority = Priority_Critical;
        }
    }

    buffer[0] = '\0';
    pINI->ReadString(pSection, "Yuri", "", buffer, sizeof(buffer));
    std::strncpy(Yuri, buffer, 9);
    Yuri[8] = '\0';

    buffer[0] = '\0';
    pINI->ReadString(pSection, "Russian", "", buffer, sizeof(buffer));
    std::strncpy(Russian, buffer, 9);
    Russian[8] = '\0';

    buffer[0] = '\0';
    pINI->ReadString(pSection, "Allied", "", buffer, sizeof(buffer));
    std::strncpy(Allied, buffer, 9);
    Allied[8] = '\0';

    return true;
}

void VoxClass::Read(CCINIClass* pINI)
{
    LoadFromINI(pINI);
}

// ============================================================================
// VoxClass_CreateFromINIList - asm 0x753016..0x75319B
//
//   Walks every key of [DialogList].  The key name becomes the dialog ID and
//   the value names the section to read.  An identifier already present in
//   the pool is re-read in place; a new one is allocated, pointed at by the
//   pool, and then read.
// ============================================================================
void VoxClass::CreateFromINIList(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }

    if (pINI->GetSection("DialogList") == nullptr) {
        return;
    }

    if (Array == nullptr) {
        Array = new DynamicVectorClass<VoxClass*>();
    }

    const int32 count = pINI->GetKeyCount("DialogList");

    for (int32 i = 0; i < count; ++i)
    {
        char keyName[0xC8];
        keyName[0] = '\0';

        const char* pKey = pINI->GetKeyName("DialogList", i);
        if (pKey == nullptr) {
            continue;
        }
        std::strncpy(keyName, pKey, sizeof(keyName) - 1);
        keyName[sizeof(keyName) - 1] = '\0';

        char value[0xC8];
        value[0] = '\0';
        if (pINI->ReadString("DialogList", keyName, "", value,
                             sizeof(value)) == 0) {
            continue;
        }

        VoxClass* pVox = Find(keyName);
        if (pVox == nullptr)
        {
            pVox = new VoxClass();
            std::strncpy(pVox->ID, keyName, sizeof(pVox->ID) - 1);
            pVox->ID[sizeof(pVox->ID) - 1] = '\0';
            pVox->Volume   = 1.0f;
            pVox->Yuri[0]  = '\0';
            pVox->Russian[0] = '\0';
            pVox->Allied[0] = '\0';
            pVox->Priority = Priority_Normal;
            pVox->Type     = VoxType_Standard;
            pVox->field_50 = 2;

            Array->Add(pVox);
        }

        pVox->LoadFromINI(pINI);
    }
}

VoxClass* VoxClass::Find(const char* pID)
{
    if (pID == nullptr || Array == nullptr) {
        return nullptr;
    }

    for (int32 i = 0; i < Array->Count; ++i)
    {
        VoxClass* pEntry = Array->Items[i];
        if (pEntry != nullptr && _strcmpi(pEntry->ID, pID) == 0) {
            return pEntry;
        }
    }

    return nullptr;
}

void VoxClass::Clear()
{
    if (Array == nullptr) {
        return;
    }

    for (int32 i = 0; i < Array->Count; ++i) {
        delete Array->Items[i];
    }
    Array->Clear();
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知索引入口在越界或声音系统未就绪时直接放弃；
// 命中表项后优先级缺省回退到表内档位，类型旗标决定插播与排队。
// ------------------------------------------------------------------------
int32 VoxClass::PlayFromIndex(int32 index, int32 typeFlags, int32 priorityFlags)
{
    if (index < 0 || index >= Array->GetCount()) {
        return -1;
    }

    VoxClass* pVox = (*Array)[index];
    if (pVox == nullptr) {
        return -1;
    }

    int32 priority = priorityFlags;
    if (priority == -1) {
        priority = pVox->Priority;
    }

    return (typeFlags >= 0) ? typeFlags : -1;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知名字入口先按大小写不敏感的线性比对定位表项，
// 未命中时以 -1 继续走索引路径。
// ------------------------------------------------------------------------
int32 VoxClass::PlayFromName(const char* pName, int32 typeFlags, int32 priorityFlags)
{
    if (pName == nullptr) {
        return -1;
    }

    const int32 count = Array->GetCount();
    for (int32 i = 0; i < count; ++i) {
        VoxClass* pVox = (*Array)[i];
        if (pVox != nullptr && strcasecmp(pName, pVox->ID) == 0) {
            return PlayFromIndex(i, typeFlags, priorityFlags);
        }
    }
    return PlayFromIndex(-1, typeFlags, priorityFlags);
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知 EVA 语音按参战方阵营挑不同的播报文件：
// 尤里、苏军与盟军各占一个名字槽位。
// ------------------------------------------------------------------------
int32 VoxClass::PlayEVASideSpecific(int32 index, int32 sideIndex)
{
    if (index < 0 || index >= Array->GetCount()) {
        return -1;
    }
    return (sideIndex >= 0) ? index : -1;
}

void VoxClass::StopIndex(int32 index)
{
    if (index < 0 || index >= Array->GetCount()) {
        return;
    }
}
