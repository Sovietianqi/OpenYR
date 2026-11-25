#include "VoxClass.h"
#include "VocClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdlib>
#include <chrono>

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

// ============================================================================
// EVA 队列与播放通道
//
// asm 观测到的全局结构：
//   stru_B1D3C8        待播队列（双向环形链表）
//   Voc_X.FShift1 ...  三条按阵营分开的队列（Flash/停止时逐条清）
//   节点 +0x00/+0x04   前驱/后继      (VocClass_RemoveFromList)
//   节点 +0x0C         VoxClass*      (sub_752680: cmp [eax+0Ch],edi)
//   节点 +0x14         优先级         (PlayEVASideSpecific: [esi+14h])
//   节点 +0x18         类型           (PlayFromIndex: cmp [eax+18h],ebp)
//
// 音频后端（AudioStream_Play_WAV）未建模：以可注入回调落点，默认只
// 记录样本名并视为开播成功；计时用单调时钟等价替代原版的 64 位 tick。
// ============================================================================

namespace
{
    struct VoxQueueNode
    {
        VoxQueueNode* NearLink;   // +0x00
        VoxQueueNode* FarLink;    // +0x04
        int32         Unk08;      // +0x08
        VoxClass*     Entry;      // +0x0C
        int32         Unk10;      // +0x10
        int32         Priority;   // +0x14
        int32         Type;       // +0x18

        VoxQueueNode()
            : NearLink(this), FarLink(this), Unk08(0), Entry(nullptr),
              Unk10(0), Priority(0), Type(0) {}
    };

    VoxQueueNode g_MainQueue;                  // asm: stru_B1D3C8
    VoxQueueNode g_SideQueues[3];              // asm: Voc_X 起的三条阵营队列

    VoxClass*    g_CurrentVox   = nullptr;     // asm: SoundIndices+20h
    // 原版 +28h 由 AudioStream_Play_WAV 初始化后一直非空；本工程无后端，
    // 用一个非空哨兵表示"通道就绪"，与 asm 的 test/jz 判定等价。
    void*        g_VoiceHandle  = reinterpret_cast<void*>(static_cast<intptr_t>(1));
    int32        g_SideIndex    = 0;           // asm: SoundIndices+24h
    int32        g_PlaybackStart = 0;          // asm: SoundIndices+2Ch
    int32        g_PlaybackLength = 0;         // asm: SoundIndices+30h
    int32        g_Flag38       = 0;           // asm: Voc_X.field_38
    bool         g_SpeechEnabled = true;       // asm: dword_B1D3D8
    char         g_LastSample[0x104] = { 0 };

    VoxClass::VoiceBackendProc g_VoiceBackend = nullptr;

    int32 NowMilliseconds()
    {
        using namespace std::chrono;
        static const steady_clock::time_point s_Start = steady_clock::now();
        return static_cast<int32>(
            duration_cast<milliseconds>(steady_clock::now() - s_Start).count());
    }

    // asm: VocCtrl_Add (asm:566) —— 插入到表头之后
    void InsertAfterHead(VoxQueueNode* pHead, VoxQueueNode* pNode)
    {
        pNode->NearLink = pHead;
        pNode->FarLink  = pHead->FarLink;
        pHead->FarLink  = pNode;
        pNode->FarLink->NearLink = pNode;
    }

    // asm: VocCtrl_Add 的尾部版本（Type=QUEUE 时按入队顺序追加）
    void InsertBeforeHead(VoxQueueNode* pHead, VoxQueueNode* pNode)
    {
        pNode->NearLink = pHead->NearLink;
        pNode->FarLink  = pHead;
        pHead->NearLink->FarLink = pNode;
        pHead->NearLink = pNode;
    }

    // asm: VocClass_RemoveFromList (asm:588)
    void UnlinkNode(VoxQueueNode* pNode)
    {
        pNode->FarLink->NearLink = pNode->NearLink;
        pNode->NearLink->FarLink = pNode->FarLink;
        pNode->NearLink = pNode;
        pNode->FarLink  = pNode;
    }

    // asm: VocClass_GetPreviousFromHead (asm:518) —— 表头 -> 最旧一条
    VoxQueueNode* OldestOf(VoxQueueNode* pHead)
    {
        VoxQueueNode* pPrev = pHead->NearLink;
        if (pPrev == nullptr || pPrev == pHead)
            return nullptr;
        return pPrev;
    }

    // asm: VocClass_GetPrevious (asm:551)
    VoxQueueNode* PrevOf(VoxQueueNode* pNode)
    {
        if (pNode == nullptr)
            return nullptr;
        VoxQueueNode* pPrev = pNode->NearLink;
        if (pPrev == nullptr || pPrev == pNode)
            return nullptr;
        return pPrev;
    }

    // asm: sub_752680 —— 在队列里找"属于该 VoxClass*"的第一条
    VoxQueueNode* FindQueued(VoxQueueNode* pHead, VoxClass* pEntry)
    {
        for (VoxQueueNode* pIt = OldestOf(pHead); pIt != nullptr; pIt = PrevOf(pIt))
        {
            if (pIt->Entry == pEntry)      // asm: cmp [eax+0Ch],edi
                return pIt;
        }
        return nullptr;
    }

    // asm: sub_752370 —— 逐条摘链、复位 Entry 的 +0x50、释放节点
    void ClearQueue(VoxQueueNode* pHead)
    {
        VoxQueueNode* pNode = OldestOf(pHead);
        while (pNode != nullptr)
        {
            VoxQueueNode* pPrev = PrevOf(pNode);
            UnlinkNode(pNode);                       // asm: VocClass_RemoveFromList
            if (pNode->Entry != nullptr)
                pNode->Entry->field_50 = 0;          // asm: mov [ecx+50h],ebx
            delete pNode;
            pNode = pPrev;
        }
    }

    VoxQueueNode* TakeOldest()
    {
        VoxQueueNode* pNode = OldestOf(&g_MainQueue);    // asm: 先看 stru_B1D3C8
        if (pNode == nullptr)
        {
            for (int32 i = 0; i < 3 && pNode == nullptr; ++i)
                pNode = OldestOf(&g_SideQueues[i]);      // asm: 逐条阵营队列
        }
        if (pNode != nullptr)
            UnlinkNode(pNode);
        return pNode;
    }

    // asm: sub_752590（未导出，按 Type token 语义固化）
    //   STANDARD         先清空主队列再入队
    //   QUEUE            追加到队尾（按入队顺序播）
    //   INTERRUPT        插到队首（立即插播）
    //   QUEUED_INTERRUPT 插到队首但保留队列
    void Enqueue(VoxClass* pEntry, int32 type, int32 priority)
    {
        VoxQueueNode* pNode = new VoxQueueNode();
        if (pNode == nullptr)
            return;

        pNode->Entry    = pEntry;                   // asm: mov [esi+0Ch],edi
        pNode->Priority = priority;                 // asm: [esi+14h]
        pNode->Type     = type;                     // asm: [esi+18h]

        if (type == VoxClass::VoxType_Standard)
        {
            ClearQueue(&g_MainQueue);
            InsertAfterHead(&g_MainQueue, pNode);
        }
        else if (type == VoxClass::VoxType_Queue)
        {
            InsertBeforeHead(&g_MainQueue, pNode);
        }
        else
        {
            InsertAfterHead(&g_MainQueue, pNode);
        }
    }

    // asm: AudioStream_Play_WAV —— 把 "名字.WAV" 交给音频后端。
    //
    // 原版这里是把样本塞进一个固定的语音通道；本工程的等价路径是：
    //   1) 若宿主注册了后端回调（录像/测试用），交给它；
    //   2) 否则从 audio.idx + audio.bag 取样本、解码，再走混音器播放。
    // 取不到样本时返回 false —— 不谎报成功，调用方的失败分支会保持状态。
    VocClass* g_CurrentSample = nullptr;      // 正在播的 EVA 样本

    bool PlayVoiceFile(const char* pFileName, float volume)
    {
        int32 len = static_cast<int32>(std::strlen(pFileName));
        if (len > static_cast<int32>(sizeof(g_LastSample)) - 1)
            len = static_cast<int32>(sizeof(g_LastSample)) - 1;
        std::memcpy(g_LastSample, pFileName, len);
        g_LastSample[len] = '\0';

        if (g_VoiceBackend != nullptr)
            return g_VoiceBackend(pFileName);

        VocClass* pVoc = VocClass::FindOrLoadShared(pFileName);
        if (pVoc == nullptr)
        {
            static bool s_Warned = false;
            if (!s_Warned)
            {
                s_Warned = true;
                std::fprintf(stderr,
                             "[AUDIO] audio.idx 里找不到 \"%s\"（或未挂载音频库）；"
                             "EVA 语音将保持静音。\n", pFileName);
            }
            return false;
        }

        if (g_CurrentSample != nullptr && g_CurrentSample != pVoc)
            g_CurrentSample->Stop();

        int32 vol = static_cast<int32>(volume * 255.0f);
        if (vol < 0)   vol = 0;
        if (vol > 255) vol = 255;
        pVoc->SetVolume(vol);
        pVoc->Play();

        g_CurrentSample = pVoc;
        return true;
    }

    // 打断/清空语音时同步停掉真正在播的那一路样本。
    void StopCurrentSample()
    {
        if (g_CurrentSample != nullptr)
        {
            g_CurrentSample->Stop();
            g_CurrentSample = nullptr;
        }
    }
}

// ============================================================================
// asm: VoxClass_FindIndex (asm:1012)
//   if (!name) return -1;
//   for (i = 0; i < length; ++i) if (!__strcmpi(name, data[i]->ID)) return i;
//   return -1;
// ============================================================================
int32 VoxClass::FindIndex(const char* pID)
{
    if (pID == nullptr)                             // asm: test edi,edi / jnz
        return -1;
    if (Array == nullptr)
        return -1;

    for (int32 i = 0; i < Array->Count; ++i)        // asm: cmp esi,[+0Ch] / jl
    {
        VoxClass* pEntry = (*Array)[i];
        if (pEntry != nullptr && _strcmpi(pID, pEntry->ID) == 0)
            return i;
    }
    return -1;
}

// ============================================================================
// asm: VoxClass_FindByIndex (asm:25)
//   if (idx == -1 || idx < 0 || idx >= length) return "none";
//   return data[idx];        ; VoxClass 的首字段即 ID[0x28]
// ============================================================================
const char* VoxClass::FindByIndex(int32 index)
{
    if (index == -1)                                // asm: cmp ecx,0FFFFFFFFh
        return "none";
    if (index < 0)                                  // asm: test ecx,ecx / jl
        return "none";
    if (Array == nullptr || index >= Array->Count)  // asm: cmp ecx,[+0Ch] / jge
        return "none";

    VoxClass* pEntry = (*Array)[index];
    return (pEntry != nullptr) ? pEntry->ID : "none";
}

// ============================================================================
// asm: VoxClass_PlayFromIndex (asm:59)
//
//   if (!SoundIndices+28h) return;                  ; 通道未就绪
//   if (idx < 0 || idx >= length) return;
//   if (dword_B1D3D8) return;                       ; 语音静音
//   ebx = data[idx];
//   if (ebx == SoundIndices+20h) return;            ; 已在播同一条
//   if (Type_Flags == -1) Type = [ebx+4Ch];
//   if (Priority_Flags == -1) Priority = [ebx+48h];
//   if (SoundIndices+20h && Type == 2) {            ; INTERRUPT
//       清空 stru_B1D3C8；当前条 +50h = 2；当前 = 0；
//       Stop_Sample(channel)；清阵营队列；计时归零；
//   }
//   node = sub_752680(ebx);
//   if (!node || [node+18h] != Type) sub_752590(ebx, Type, Priority);
//   call VoxClass_PlayEVASideSpecific();
// ============================================================================
void VoxClass::PlayFromIndex(int32 index, int32 typeFlags, int32 priorityFlags)
{
    if (g_VoiceHandle == nullptr)                   // asm: mov eax,[+28h] / test / jz
        return;
    if (index < 0 || Array == nullptr || index >= Array->Count)
        return;                                     // asm: test ecx / jl ; cmp / jge
    if (!g_SpeechEnabled)                           // asm: mov eax,dword_B1D3D8
        return;

    VoxClass* pEntry = (*Array)[index];             // asm: mov ebx,[eax+ecx*4]
    if (pEntry == nullptr)
        return;
    if (pEntry == g_CurrentVox)                     // asm: cmp ebx,[+20h] / jz
        return;

    int32 type = (typeFlags == -1) ? pEntry->Type : typeFlags;          // asm: [ebx+4Ch]
    int32 priority = (priorityFlags == -1) ? pEntry->Priority : priorityFlags; // [ebx+48h]

    if (g_CurrentVox != nullptr && type == VoxType_Interrupt)
    {
        ClearQueue(&g_MainQueue);                   // asm: GetPreviousFromHead 循环删除
        if (g_CurrentVox != nullptr)
        {
            g_CurrentVox->field_50 = 2;             // asm: mov dword ptr [edx+50h],2
            g_CurrentVox = nullptr;                 // asm: [+20h] = 0
        }
        // asm: Stop_Sample(channel) 只是停掉当前样本，+28h 通道本身仍然有效
        StopCurrentSample();
        for (int32 i = 0; i < 3; ++i)
            ClearQueue(&g_SideQueues[i]);           // asm: call sub_752370
        g_PlaybackStart  = 0;                       // asm: [+2Ch] = 0
        g_PlaybackLength = 0;                       // asm: [+30h] = 0
    }

    VoxQueueNode* pNode = FindQueued(&g_MainQueue, pEntry);   // asm: sub_752680
    if (pNode == nullptr || pNode->Type != type)              // asm: cmp [eax+18h],ebp
        Enqueue(pEntry, type, priority);                      // asm: sub_752590

    PlayEVASideSpecific();                          // asm: call VoxClass_PlayEVASideSpecific
}

// ============================================================================
// asm: VoxClass_PlayFromName (asm:171)
//   if (!name) return;
//   for (i = 0; i < length; ++i) if (!__strcmpi(name, data[i])) -> idx = i;
//   未命中 idx = -1；两条路径都转 PlayFromIndex(idx, a2, a3)。
// ============================================================================
void VoxClass::PlayFromName(const char* pName, int32 typeFlags, int32 priorityFlags)
{
    if (pName == nullptr)                           // asm: test edi,edi / jz
        return;

    int32 index = -1;                               // asm: or ecx,-1
    const int32 found = FindIndex(pName);
    if (found >= 0)
        index = found;

    // OPENYR_AUDIODBG：把"谁想播、有没有查到这个 ID"打出来。EVA 静音时
    // 第一步要区分"压根没触发"和"触发了但查不到/没样本"。
    static const bool s_AudioDbg = (std::getenv("OPENYR_AUDIODBG") != nullptr);
    if (s_AudioDbg)
        std::fprintf(stderr, "[VOX] PlayFromName %s idx=%d\n", pName, index);

    PlayFromIndex(index, typeFlags, priorityFlags); // asm: call VoxClass_PlayFromIndex
}

// ============================================================================
// asm: VoxClass_PlayEVASideSpecific (asm:243)
//
//   if (Debug_Quiet || !audioThread) return;
//   if (!SoundIndices+28h) return;
//   if (sub_408070(channel)) return;                 ; 通道忙
//   now = Steam_Player_4093B0();
//   if (now <= start + length) return;               ; 上一段还没播完
//   if (Voc_X.field_38) return;
//   if (current) { current->+50h = 2; current = 0; }
//   node = 依次从 stru_B1D3C8 / 三条阵营队列取一条；
//   entry = node->+0Ch; pri14 = node->+14h; pri18 = node->+18h; delete node;
//   if (!entry) return;
//   entry->+50h = 2;
//   name = (side == 0) ? AllVoice : (side == 1 ? SovVoice : YuriVoice);
//   sample = name + ".WAV";
//   if (!AudioStream_Play_WAV(channel, sample, 1)) return;
//   start = 500; length = 0; current = entry; [+50h] = 0;
// ============================================================================
void VoxClass::PlayEVASideSpecific()
{
    if (!g_SpeechEnabled)                            // asm: cmp al,bl / jnz
        return;
    if (g_VoiceHandle == nullptr)                    // asm: cmp [+28h],0 / jz
        return;
    if (g_Flag38 != 0)                               // asm: mov eax,[field_38] / jnz
        return;

    // asm: 若上一段仍在播（now <= start + length）直接返回
    const int32 now = NowMilliseconds();
    if (now <= g_PlaybackStart + g_PlaybackLength)
        return;

    if (g_CurrentVox != nullptr)                     // asm: mov eax,[+20h] / jz
    {
        g_CurrentVox->field_50 = 2;                  // asm: mov [eax+50h],2
        g_CurrentVox = nullptr;                      // asm: [+20h] = 0
    }

    VoxQueueNode* pNode = TakeOldest();              // asm: 三条链依次 GetPreviousFromHead
    if (pNode == nullptr)
        return;

    VoxClass* pEntry  = pNode->Entry;                // asm: mov ebp,[esi+0Ch]
    int32     priority = pNode->Priority;            // asm: mov [var_104],[esi+18h]
    int32     type     = pNode->Type;                // asm: mov [var_108],[esi+14h]
    delete pNode;                                    // asm: call delete
    (void)priority;
    (void)type;

    if (pEntry == nullptr)
        return;

    pEntry->field_50 = 2;                            // asm: mov [ebp+50h],2

    // asm: side==0 -> AllVoice(+3Eh,本工程 Allied)；side==1 -> SovVoice(+35h,
    //      本工程 Russian)；否则 YuriVoice(+2Ch,本工程 Yuri)。
    const char* pVoice;
    if (g_SideIndex == 0)      pVoice = pEntry->Allied;
    else if (g_SideIndex == 1) pVoice = pEntry->Russian;
    else                       pVoice = pEntry->Yuri;

    char sampleName[0x104];
    std::strncpy(sampleName, pVoice, sizeof(sampleName) - 5);
    sampleName[sizeof(sampleName) - 5] = '\0';
    std::strcat(sampleName, ".WAV");                 // asm: 追加 ".WAV"

    // 音量来自 [DialogList] 该条的 Volume（0..1）；原版在把样本塞进通道时
    // 用它折算通道音量。
    if (!PlayVoiceFile(sampleName, pEntry->Volume))  // asm: AudioStream_Play_WAV
        return;

    g_PlaybackStart  = 500;                          // asm: mov [+2Ch],500
    g_PlaybackLength = 0;                            // asm: mov [+30h],0
    g_CurrentVox     = pEntry;                       // asm: mov [+20h],ebp
    pEntry->field_50 = 0;                            // asm: mov [ebp+50h],ebx
}

// ============================================================================
// asm: VoxClass_StopIndex (asm:456)
//   if (idx < 0 || idx >= length) return;
//   entry = data[idx];
//   while (node = sub_752680(entry)) {
//       if ([node+4] != node) RemoveFromList(node);
//       if (node == SoundIndices+14h) SoundIndices+14h = 0;
//       [node+0Ch]->+50h = 2;
//       delete node;
//   }
// ============================================================================
void VoxClass::StopIndex(int32 index)
{
    if (Array == nullptr || index < 0 || index >= Array->Count)   // asm: jl / jge
        return;

    VoxClass* pEntry = (*Array)[index];
    if (pEntry == nullptr)
        return;

    for (;;)
    {
        VoxQueueNode* pNode = FindQueued(&g_MainQueue, pEntry);   // asm: sub_752680
        if (pNode == nullptr)
        {
            for (int32 i = 0; i < 3 && pNode == nullptr; ++i)
                pNode = FindQueued(&g_SideQueues[i], pEntry);
        }
        if (pNode == nullptr)
            break;

        if (pNode->FarLink != pNode)                 // asm: cmp [esi+4],esi / jz
            UnlinkNode(pNode);                       // asm: VocClass_RemoveFromList

        if (pNode->Entry == g_CurrentVox)            // asm: cmp esi,[+14h]
            g_CurrentVox = nullptr;

        if (pNode->Entry != nullptr)
            pNode->Entry->field_50 = 2;              // asm: mov [ecx+50h],2

        delete pNode;                                // asm: call delete
    }
}

// ============================================================================
// asm: VoxClass_DeleteAll (asm:929)
//   1) current: +50h = 2, current = 0
//   2) Stop_Sample(channel)
//   3) sub_752370 —— 清阵营队列
//   4) 逐个删除表项（动态数组 Remove，保持顺序）
// ============================================================================
void VoxClass::DeleteAll()
{
    if (g_CurrentVox != nullptr)                     // asm: mov eax,[+20h] / jz
    {
        g_CurrentVox->field_50 = 2;                  // asm: mov [eax+50h],2
        g_CurrentVox = nullptr;                      // asm: mov [+20h],0
    }

    ClearQueue(&g_MainQueue);                        // asm: call sub_752370
    for (int32 i = 0; i < 3; ++i)
        ClearQueue(&g_SideQueues[i]);

    // asm: Stop_Sample(channel) —— 通道保留，仅停当前样本
    StopCurrentSample();
    g_PlaybackStart  = 0;
    g_PlaybackLength = 0;

    if (Array == nullptr)
        return;

    while (Array->Count > 0)                         // asm: 循环删除首项
    {
        VoxClass* pEntry = Array->Items[0];
        Array->Remove(0);
        delete pEntry;                               // asm: call delete
    }
}

bool VoxClass::Get_Speech_Enabled()
{
    return g_SpeechEnabled;                          // asm: dword_B1D3D8
}

void VoxClass::Set_Speech_Enabled(bool on)
{
    g_SpeechEnabled = on;
}

void VoxClass::Set_Voice_Side(int32 side)
{
    g_SideIndex = side;                              // asm: SoundIndices+24h
}

int32 VoxClass::Get_Voice_Side()
{
    return g_SideIndex;
}

VoxClass* VoxClass::Get_Current()
{
    return g_CurrentVox;                             // asm: SoundIndices+20h
}

void VoxClass::Set_Voice_Backend(VoiceBackendProc pProc)
{
    g_VoiceBackend = pProc;
}
