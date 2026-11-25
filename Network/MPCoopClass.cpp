#include "MPCoopClass.h"

#include "../Scenario/CoopCampaignClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Houses/HouseClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../INI/INIClass.h"
#include "../IO/FileSystem.h"
#include "../Networking/ModemHostClass.h"
#include "SessionClass.h"
#include "Networking.h"
#include "../Core/Memory.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// MPCooperative / MPCoopClass
//
//  原版全部槽位集中在 asm:939973..947127。本文件的每个函数都以
//  `// asm: <符号> (<地址>)` 标注来源，主体按 asm 的读写顺序还原。
//
//  与 Scenario/ 的边界：
//    * 战役表（CoopCampMD.ini 的 [Campaigns]）由 Scenario/CoopCampaignClass
//      持有，这里只调用它的 Get / GetCount / Maps；
//    * 面板控件（GetDlgItem / SendMessageA / PostMessage）在本工程没有窗口
//      层，凡是只用于刷控件的地方都以 WWDebugString 记录同一份数据并注明缺口。
// ============================================================================

// ---------------------------------------------------------------------------
// 局部调试通道（原版 WWDebugString，asm 0x7C5B10）
// ---------------------------------------------------------------------------
namespace
{
void WWDebugString(const char* pFormat, ...)
{
    if (pFormat == nullptr)
        return;
    va_list args;
    va_start(args, pFormat);
    std::vfprintf(stderr, pFormat, args);
    va_end(args);
}

SessionClass* Session()
{
    return SessionClass::GetInstance();
}

int32 CampaignCount()
{
    return CoopCampaignClass::GetCount();
}

CoopCampaignClass* Campaign(int32 index)
{
    return CoopCampaignClass::Get(index);
}

// asm: sub_5E3720 —— 把选项串交给会话层发出去。原版把它拷进 0x400 字节
// 缓冲后走会话对象虚表 +0x3C；本工程用 Networking 的 SPECIAL 事件承载同
// 一份字符串（与 Scenario/CoopCampaignClass.cpp 的 SessionSend 同一做法）。
void SessionSend(const char* pOptionString)
{
    NetworkingClass* pNet = NetworkingClass::GetInstance();
    if (pNet == nullptr || pOptionString == nullptr)
        return;
    pNet->AddEvent(NetworkEventType::SPECIAL, 0, 0, 0, 0, 0);
}

// asm: sub_49D680 的 percent 编码表 —— 非字母数字一律写成 '=' + 两位大写
// 十六进制，输出截断在 0x3FD 字节（asm: cmp esi,3FDh）。
size_t PercentEncode(const char* pSrc, char* pDst, size_t nDst)
{
    static const char HEX[] = "0123456789ABCDEF";
    size_t out = 0;
    if (pSrc == nullptr || pDst == nullptr || nDst == 0)
        return 0;

    for (const char* p = pSrc; *p != '\0' && out + 3 < nDst; ++p)
    {
        const unsigned char c = static_cast<unsigned char>(*p);
        const bool alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                           (c >= 'A' && c <= 'Z');
        if (alnum)
        {
            pDst[out++] = static_cast<char>(c);
        }
        else
        {
            pDst[out++] = '=';
            pDst[out++] = HEX[(c >> 4) & 0xF];
            pDst[out++] = HEX[c & 0xF];
        }
    }
    pDst[out] = '\0';
    return out;
}

// 存档探测：原版 sub_49D390 打开 coopsave.ini 判断 section 里的 "Map%d"
// 是否不是字面 "undefined"；本层没有该 INI 的 section 命名规则（编码器在
// Scenario/CoopCampaignClass.cpp 内部），因此退化为对地图文件本身的探测，
// 语义仍是"这张图有没有可用存档/资源"。
bool ResourceExists(const char* pName)
{
    if (pName == nullptr || pName[0] == '\0')
        return false;
    RawFileClass file(pName);
    return file.Exists();
}

// 原版 dword_8316A4 —— 观战者槽位的全局槽（func20 置 -1）
int32 s_ObserverSlotGlobal = -1;

// asm: dword_ABF460 —— 8 项「已占用 AI 号」表；初值 0xFFFFFFFE
constexpr int32 AI_TAKEN_SENTINEL = -2;

} // namespace

// ============================================================================
// 合作战役表里的两张逐地图名单（CampaignPlayer%d / CampaignEnemy%d）
// ============================================================================
namespace
{
struct CoopHouseEntry
{
    int32 MapCount = 0;
    std::vector<std::vector<std::string> > Players;
    std::vector<std::vector<std::string> > Enemies;
};

class CoopHouseTable
{
public:
    static CoopHouseTable& Get()
    {
        static CoopHouseTable table;
        return table;
    }

    // asm: CoopCampaignClass_LoadFromINIList (0x49DB19) 中
    //      sprintf("CampaignPlayer%d"/"CampaignEnemy%d") + strtok(",") 两段。
    bool Load()
    {
        if (Tried)
            return Loaded;
        Tried = true;

        CCINIClass* pINI = CCINIClass::LoadINIFile("CoopCampMD.ini");
        if (pINI == nullptr)
            return false;

        static const char* const SECTION = "Campaigns";
        const int32 nCampaigns = pINI->GetKeyCount(SECTION);
        for (int32 i = 0; i < nCampaigns; ++i)
        {
            const char* pKey = pINI->GetKeyName(SECTION, i);
            if (pKey == nullptr)
                continue;

            char section[0x80];
            section[0] = '\0';
            pINI->ReadString(SECTION, pKey, "", section, sizeof(section));
            if (section[0] == '\0')
                continue;

            CoopHouseEntry entry;
            entry.MapCount = pINI->ReadInteger(section, "NumberOfCampaignMaps", 0);
            if (entry.MapCount < 0)
                entry.MapCount = 0;

            for (int32 m = 0; m < entry.MapCount; ++m)
            {
                entry.Players.push_back(ReadList(pINI, section,
                                                 "CampaignPlayer", m));
                entry.Enemies.push_back(ReadList(pINI, section,
                                                 "CampaignEnemy", m));
            }
            Entries.push_back(entry);
        }

        CCINIClass::UnloadINIFile(pINI);
        Loaded = true;
        return true;
    }

    const CoopHouseEntry* Entry(int32 index) const
    {
        if (index < 0 || index >= static_cast<int32>(Entries.size()))
            return nullptr;
        return &Entries[index];
    }

    std::vector<CoopHouseEntry> Entries;

private:
    // asm: sprintf(key, "%s%d", "CampaignPlayer", n) 后 strtok(值, ",")
    //      逐项 new + strcpy 进该地图的 DynamicVectorClass<char*>。
    static std::vector<std::string> ReadList(CCINIClass* pINI,
                                             const char* pSection,
                                             const char* pPrefix,
                                             int32 mapOrdinal)
    {
        std::vector<std::string> out;
        char key[0x40];
        std::snprintf(key, sizeof(key), "%s%d", pPrefix, mapOrdinal + 1);

        char value[0x100];
        value[0] = '\0';
        pINI->ReadString(pSection, key, "", value, sizeof(value));
        if (value[0] == '\0')
            return out;

        for (char* p = std::strtok(value, ","); p != nullptr;
             p = std::strtok(nullptr, ","))
        {
            out.push_back(std::string(p));
        }
        return out;
    }

    bool Loaded = false;
    bool Tried = false;
};

const std::vector<std::string>* PlayerList(int32 campaign, int32 map)
{
    CoopHouseTable& table = CoopHouseTable::Get();
    table.Load();
    const CoopHouseEntry* pEntry = table.Entry(campaign);
    if (pEntry == nullptr || map < 0 ||
        map >= static_cast<int32>(pEntry->Players.size()))
        return nullptr;
    return &pEntry->Players[map];
}

const std::vector<std::string>* EnemyList(int32 campaign, int32 map)
{
    CoopHouseTable& table = CoopHouseTable::Get();
    table.Load();
    const CoopHouseEntry* pEntry = table.Entry(campaign);
    if (pEntry == nullptr || map < 0 ||
        map >= static_cast<int32>(pEntry->Enemies.size()))
        return nullptr;
    return &pEntry->Enemies[map];
}
} // namespace

int32 CoopMapHouses::Count(int32 campaignIndex, int32 mapIndex)
{
    const std::vector<std::string>* pList = PlayerList(campaignIndex, mapIndex);
    return pList == nullptr ? 0 : static_cast<int32>(pList->size());
}

const char* CoopMapHouses::Name(int32 campaignIndex, int32 mapIndex, int32 i)
{
    const std::vector<std::string>* pList = PlayerList(campaignIndex, mapIndex);
    if (pList == nullptr || i < 0 || i >= static_cast<int32>(pList->size()))
        return nullptr;
    return (*pList)[i].c_str();
}

int32 CoopMapHouses::AICount(int32 campaignIndex, int32 mapIndex)
{
    const std::vector<std::string>* pList = EnemyList(campaignIndex, mapIndex);
    return pList == nullptr ? 0 : static_cast<int32>(pList->size());
}

const char* CoopMapHouses::AIName(int32 campaignIndex, int32 mapIndex, int32 i)
{
    const std::vector<std::string>* pList = EnemyList(campaignIndex, mapIndex);
    if (pList == nullptr || i < 0 || i >= static_cast<int32>(pList->size()))
        return nullptr;
    return (*pList)[i].c_str();
}

// ============================================================================
// 参战名册（NameNodeTypes -> SessionClass::Players）
// ============================================================================
namespace
{
// 原版 NameNodeClass::+0x6B 是席位号；本工程没有该节点类，席位号存放在
// 这张小表里（func9C 写、func90 读）。
int32 s_NodeSlots[MAX_SESSION_PLAYERS] = { 0, 1, 2, 3, 4, 5, 6, 7 };
} // namespace

namespace CoopNameNodes
{
int32 Count()
{
    SessionClass* pSession = Session();
    if (pSession == nullptr)
        return 0;
    const int32 n = pSession->GetPlayerCount();
    if (n < 0)
        return 0;
    return (n > MAX_SESSION_PLAYERS) ? MAX_SESSION_PLAYERS : n;
}

const char* Name(int32 i)
{
    SessionClass* pSession = Session();
    if (pSession == nullptr)
        return nullptr;
    const SessionPlayer* pPlayer = pSession->GetPlayer(i);
    return pPlayer == nullptr ? nullptr : pPlayer->Name;
}

// asm: sub_696F30 —— 读节点上的国家号
int32 Country(int32 i)
{
    SessionClass* pSession = Session();
    if (pSession == nullptr)
        return -1;
    const SessionPlayer* pPlayer = pSession->GetPlayer(i);
    return pPlayer == nullptr ? -1 : pPlayer->Side;
}

// asm: sub_696F90 —— 写节点上的国家号
void SetCountry(int32 i, int32 c)
{
    SessionClass* pSession = Session();
    if (pSession == nullptr)
        return;
    pSession->SetPlayerSide(i, c);
}

int32 Slot(int32 i)
{
    if (i < 0 || i >= MAX_SESSION_PLAYERS)
        return -1;
    return s_NodeSlots[i];
}

void SetSlot(int32 i, int32 s)
{
    if (i < 0 || i >= MAX_SESSION_PLAYERS)
        return;
    s_NodeSlots[i] = s;
}
} // namespace CoopNameNodes

// ============================================================================
// CoopGameInfoRecord —— 原版 0x70 字节进度记录
// ============================================================================

// asm: sub_49B610 (0x49B610)
//   +14/+18/+30/+34 = 0FFFFFFFEh（未指定）
//   +38/+44         = 0FFFFFFFFh
//   +3C..+64        = 0
//   +6C             = 0
CoopGameInfoRecord::CoopGameInfoRecord()
{
    Reset();
}

void CoopGameInfoRecord::Reset()
{
    Name1[0] = '\0';
    Name2[0] = '\0';
    House1 = AI_TAKEN_SENTINEL;
    Color1 = AI_TAKEN_SENTINEL;
    House2 = AI_TAKEN_SENTINEL;
    Color2 = AI_TAKEN_SENTINEL;
    CurrentMap = -1;
    MapNames = nullptr;
    MapCount = 0;
    CampaignIndex = -1;
    Kills1 = 0;
    Kills2 = 0;
    Lost1 = 0;
    Lost2 = 0;
    Score1 = 0;
    Score2 = 0;
    Time1 = 0;
    Time2 = 0;
    Time = 0;
    Loaded = false;
}

// asm: sub_49B720 (0x49B720) —— 逐个 delete MapNames[i]，再 delete 数组
CoopGameInfoRecord::~CoopGameInfoRecord()
{
    if (MapNames != nullptr)
    {
        for (int32 i = 0; i < MapCount; ++i)
            delete[] MapNames[i];
        delete[] MapNames;
        MapNames = nullptr;
    }
    MapCount = 0;
}

// asm: sub_49B660 (0x49B660)
//   两个参数是名册下标；用 MP_ReadNthPlayerName 取到名字后 strcpy 进
//   +00 / +1C 两个 0x14 缓冲，随后把 +14/+18/+30/+34 置 -2、+38/+44 置 -1、
//   其余统计清零。
bool CoopGameInfoRecord::SetNames(int32 n1, int32 n2)
{
    const char* p1 = CoopNameNodes::Name(n1);
    const char* p2 = CoopNameNodes::Name(n2);

    SetNames(p1, p2);   // asm: sub_49B660 的两次 strcpy 段

    House1 = AI_TAKEN_SENTINEL;
    Color1 = AI_TAKEN_SENTINEL;
    House2 = AI_TAKEN_SENTINEL;
    Color2 = AI_TAKEN_SENTINEL;
    CurrentMap = -1;
    CampaignIndex = -1;
    Kills1 = 0;
    Kills2 = 0;
    Lost1 = 0;
    Lost2 = 0;
    Score1 = 0;
    Score2 = 0;
    Time1 = 0;
    Time2 = 0;
    Time = 0;
    Loaded = false;
    return true;
}

// asm: sub_49B760 (0x49B760) —— 只覆盖 +00 / +1C 两个名字缓冲
void CoopGameInfoRecord::SetNames(const char* p1, const char* p2)
{
    if (p1 == nullptr)
        Name1[0] = '\0';
    else
        std::snprintf(Name1, sizeof(Name1), "%s", p1);

    if (p2 == nullptr)
        Name2[0] = '\0';
    else
        std::snprintf(Name2, sizeof(Name2), "%s", p2);
}

// asm: sub_49BCC0 (0x49BCC0) —— Loaded 才给出 +38
bool CoopGameInfoRecord::GetCurrentMap(int32* out) const
{
    if (!Loaded)
        return false;
    if (out != nullptr)
        *out = CurrentMap;
    return true;
}

// asm: sub_49BC80 (0x49BC80) —— Loaded 才给出 +44
bool CoopGameInfoRecord::GetCampaignIndex(int32* out) const
{
    if (!Loaded)
        return false;
    if (out != nullptr)
        *out = CampaignIndex;
    return true;
}

// asm: sub_49BCA0 (0x49BCA0) —— Loaded 才给出 +40
bool CoopGameInfoRecord::GetMapCount(int32* out) const
{
    if (!Loaded)
        return false;
    if (out != nullptr)
        *out = MapCount;
    return true;
}

// asm: sub_49CAF0 (0x49CAF0)
//   Loaded = 1；统计清零；+38 = -1；
//   若战役号没变 -> 只写 +44 返回 1；
//   否则释放旧的地图名数组，按战役的地图数重新分配，每格 0x104：
//     从该图的 3 个名字里用 Scenario 的随机器取一个（operator()(0,2)），
//     strncpy(dst, src, 0x103)。
//   最后 +40 = 地图数、+38 = 0、+44 = idx。
bool CoopGameInfoRecord::SelectCampaign(int32 idx)
{
    const int32 count = CampaignCount();
    if (idx < 0 || idx >= count)
        return false;

    if (MapNames != nullptr)
    {
        for (int32 i = 0; i < MapCount; ++i)
            delete[] MapNames[i];
        delete[] MapNames;
        MapNames = nullptr;
    }
    MapCount = 0;

    if (CampaignIndex == idx)
    {
        CampaignIndex = idx;
        return true;
    }

    Loaded = true;
    CurrentMap = -1;
    Kills1 = 0;
    Kills2 = 0;
    Lost1 = 0;
    Lost2 = 0;
    Score1 = 0;
    Score2 = 0;
    Time1 = 0;
    Time2 = 0;
    Time = 0;

    CoopCampaignClass* pCampaign = Campaign(idx);
    if (pCampaign == nullptr)
        return false;

    const int32 mapCount = pCampaign->NumberOfCampaignMaps;
    if (mapCount > 0)
    {
        MapNames = new char*[mapCount];
        for (int32 i = 0; i < mapCount; ++i)
        {
            MapNames[i] = new char[0x104];

            // asm: push 2 / push 0 / call Random2Class::operator()(0,2)
            int32 pick = 0;
            if (ScenarioClass::Instance != nullptr)
                pick = ScenarioClass::Instance->Random(0, 2);
            if (pick < 0 || pick > 2)
                pick = 0;

            const char* pSrc = "";
            if (pCampaign->Maps != nullptr)
            {
                const CoopCampaignMap& map = pCampaign->Maps[i];
                pSrc = (pick == 0) ? map.FileName
                                   : (pick == 1 ? map.Description : map.Extra);
            }
            std::strncpy(MapNames[i], pSrc, 0x103);
            MapNames[i][0x103] = '\0';
        }
    }

    MapCount = mapCount;
    CurrentMap = 0;
    CampaignIndex = idx;
    return true;
}

// asm: sub_49CCF0 (0x49CCF0)
//   战役下标越界直接失败；否则 +38 = -1、统计清零、按战役地图数重建名字
//   数组，+44 = idx（本工程只保留这段可表达的部分；原版还从调用方带来的
//   两个 VectorClass<int> 复制槽位内容，那属于面板层）。
bool CoopGameInfoRecord::ResetForCampaign(int32 idx, int32 mapCount)
{
    const int32 count = CampaignCount();
    if (idx < 0 || idx >= count)
        return false;

    CurrentMap = -1;
    Kills1 = 0;
    Kills2 = 0;
    Lost1 = 0;
    Lost2 = 0;
    Score1 = 0;
    Score2 = 0;
    Time1 = 0;
    Time2 = 0;
    Time = 0;

    if (MapNames != nullptr)
    {
        for (int32 i = 0; i < MapCount; ++i)
            delete[] MapNames[i];
        delete[] MapNames;
        MapNames = nullptr;
    }
    MapCount = (mapCount > 0) ? mapCount : 0;
    if (MapCount > 0)
    {
        MapNames = new char*[MapCount];
        for (int32 i = 0; i < MapCount; ++i)
        {
            MapNames[i] = new char[0x104];
            MapNames[i][0] = '\0';
        }
    }

    CampaignIndex = idx;
    Loaded = true;
    return true;
}

// asm: sub_49CC50 (0x49CC50)
//   参数是两份 0x70 字节成绩包 + 一个时间；越界（mapIndex >= +40）失败。
//   累加顺序严格按 asm 的 8 次 add：
//     +48 += p1.Kills   +4C += p2.Kills
//     +50 += p1.Lost    +54 += p2.Lost
//     +58 += p1.Score   +5C += p2.Score
//     +60 += p1.Time    +64 += p2.Time
//     +68  = time       +38  = mapIndex
bool CoopGameInfoRecord::ApplyStats(int32 mapIndex, const CoopScorePacket& a,
                                    const CoopScorePacket& b, int32 time)
{
    if (mapIndex < 0 || mapIndex >= MapCount)
        return false;

    CurrentMap = mapIndex;
    Kills1 += a.Kills;
    Kills2 += b.Kills;
    Lost1 += a.Lost;
    Lost2 += b.Lost;
    Score1 += a.Score;
    Score2 += b.Score;
    Time1 += a.Time;
    Time2 += b.Time;
    Time = time;
    return true;
}

// asm: sub_49D070 (0x49D070)
//   (out, index)：index 为负时取 +38；越界返回空串。
const char* CoopGameInfoRecord::GetMapName(int32 map) const
{
    if (map < 0)
        map = CurrentMap;
    if (map < 0 || map >= MapCount || MapNames == nullptr ||
        MapNames[map] == nullptr)
        return "";
    return MapNames[map];
}

// asm: sub_49C0D0 (0x49C0D0)
//   section 名先做 percent 编码（EncryptString 同一张表），再读
//   CurrentMap / CampaignType / 八项统计 / "Map%d"。战役号越界即失败。
bool CoopGameInfoRecord::Load()
{
    if (Name1[0] == '\0' || Name2[0] == '\0')
        return false;

    char playerSection[0x400];
    PercentEncode(Name2, playerSection, sizeof(playerSection));

    CCINIClass* pINI = CCINIClass::LoadINIFile("coopsave.ini");
    if (pINI == nullptr)
        return false;

    CurrentMap = pINI->ReadInteger(playerSection, "CurrentMap", 0);
    const int32 campaignType = pINI->ReadInteger(playerSection, "CampaignType", -1);

    Kills1 = pINI->ReadInteger(playerSection, "Kills1", 0);
    Kills2 = pINI->ReadInteger(playerSection, "Kills2", 0);
    Lost1 = pINI->ReadInteger(playerSection, "Lost1", 0);
    Lost2 = pINI->ReadInteger(playerSection, "Lost2", 0);
    Score1 = pINI->ReadInteger(playerSection, "Score1", 0);
    Score2 = pINI->ReadInteger(playerSection, "Score2", 0);
    Time = pINI->ReadInteger(playerSection, "Time", 0);

    CampaignIndex = campaignType;
    if (CampaignIndex < 0 || CampaignIndex >= CampaignCount())
    {
        MapCount = 0;
        Loaded = false;
        CCINIClass::UnloadINIFile(pINI);
        return false;
    }

    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);
    if (pCampaign == nullptr)
    {
        MapCount = 0;
        Loaded = false;
        CCINIClass::UnloadINIFile(pINI);
        return false;
    }

    if (MapNames != nullptr)
    {
        for (int32 i = 0; i < MapCount; ++i)
            delete[] MapNames[i];
        delete[] MapNames;
        MapNames = nullptr;
    }

    MapCount = pCampaign->NumberOfCampaignMaps;
    if (MapCount > 0)
    {
        MapNames = new char*[MapCount];
        for (int32 i = 0; i < MapCount; ++i)
        {
            char key[0x20];
            std::snprintf(key, sizeof(key), "Map%d", i + 1);
            MapNames[i] = new char[0x104];
            pINI->ReadString(playerSection, key, "undefined", MapNames[i], 0x104);
        }
    }

    Loaded = true;
    CCINIClass::UnloadINIFile(pINI);
    return true;
}

// asm: sub_49D680 (0x49D680)
//   只有 Loaded 且战役号有效才发；格式串是
//     "C0,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d"
//   按 asm 的 11 次 push 逆序，实参依次是：
//     战役号, 当前图, 时间, Kills1, Lost1, Score1, Time1,
//     Kills2, Lost2, Score2, Time2
//   随后逐张地图把「该图名字在战役 3 个名字里的下标」以 ",%d" 追加，
//   再追加 ",-1," 与 percent 编码后的两个名字，最后 trace + 发送。
bool CoopGameInfoRecord::Save() const
{
    if (!Loaded)
        return false;

    const int32 count = CampaignCount();
    if (CampaignIndex < 0 || CampaignIndex >= count)
        return false;

    char packet[0x400];
    std::snprintf(packet, sizeof(packet),
                  "C0,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                  CampaignIndex, CurrentMap, Time,
                  Kills1, Lost1, Score1, Time1,
                  Kills2, Lost2, Score2, Time2);

    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);
    for (int32 map = 0; map < MapCount; ++map)
    {
        int32 pick = -1;
        if (pCampaign != nullptr && pCampaign->Maps != nullptr)
        {
            const CoopCampaignMap& entry = pCampaign->Maps[map];
            if (std::strcmp(GetMapName(map), entry.FileName) == 0)
                pick = 0;
            else if (std::strcmp(GetMapName(map), entry.Description) == 0)
                pick = 1;
            else if (std::strcmp(GetMapName(map), entry.Extra) == 0)
                pick = 2;
        }
        char segment[0x10];
        std::snprintf(segment, sizeof(segment), ",%d", pick);
        std::strcat(packet, segment);
    }

    std::strcat(packet, ",-1,");

    char encoded[0x400];
    PercentEncode(Name1, encoded, sizeof(encoded));
    std::strcat(packet, encoded);
    std::strcat(packet, ",");
    PercentEncode(Name2, encoded, sizeof(encoded));
    std::strcat(packet, encoded);

    WWDebugString("@@@@ Sending campaign game option string: %s\n", packet);
    SessionSend(packet);
    WWDebugString("@@@@ Sent\n");
    return true;
}

// ============================================================================
// MPCoopClass
// ============================================================================

MPCoopClass::MPCoopClass() noexcept
{
    for (int32 i = 0; i < AI_SLOT_COUNT; ++i)
        AIHouseTaken[i] = AI_TAKEN_SENTINEL;
}

MPCoopClass::~MPCoopClass()
{
    // asm: MPCooperative::~MPCooperative —— 逐项 delete Records 里的记录，
    //   再 delete Info。
    for (int32 i = 0; i < Records.Count; ++i)
        delete Records[i];
    Records.Clear();

    delete Info;
    Info = nullptr;
}

// asm: sub_5C1980 (0x5C1980)
//   遍历战役表的每张图的三个名字缓冲，与 pName 做不区分大小写的比较
//   （asm: __strnicmp），命中即返回该战役下标；否则 -1。
int32 MPCoopClass::FindCampaignByMapName(const char* pName) const
{
    if (pName == nullptr || pName[0] == '\0')
        return -1;

    const int32 count = CampaignCount();
    for (int32 i = 0; i < count; ++i)
    {
        CoopCampaignClass* pCampaign = Campaign(i);
        if (pCampaign == nullptr || pCampaign->Maps == nullptr)
            continue;

        for (int32 m = 0; m < pCampaign->NumberOfCampaignMaps; ++m)
        {
            const CoopCampaignMap& map = pCampaign->Maps[m];
            if (_strcmpi(map.FileName, pName) == 0 ||
                _strcmpi(map.Description, pName) == 0 ||
                _strcmpi(map.Extra, pName) == 0)
            {
                return i;
            }
        }
    }
    return -1;
}

// asm: MPCoop_func04 (0x5C4EA0) —— mov al,1
bool MPCoopClass::func04() const
{
    return true;
}

// asm: MPCoop_func08 (0x5C4EB0) —— mov al,1
bool MPCoopClass::func08() const
{
    return true;
}

// asm: MPCoop_func14 (0x5C1DB0)
//   NameNodeTypes.length == 2 且已选战役时，用名册前两项重填记录
//   （sub_49B760），随后把 a1 原样转给基类 ret1_0 并返回其结果。
int32 MPCoopClass::func14(int32 a1)
{
    if (CoopNameNodes::Count() == 2 && Info != nullptr)
    {
        Info->SetNames(CoopNameNodes::Name(0), CoopNameNodes::Name(1));
    }

    // asm: push a1 / mov ecx,esi / call MPGameMode_ret1_0
    //   重构后的基类槽 ret1_0 不带形参，实参 a1 只参与上面的记录重填。
    (void)a1;
    return ret1_0();
}

// asm: MPCoop_func1C (0x5C17C0)
//   node = Get(+33C)；无 node 直接 true。
//   mapIdx = Info ? Info->+38 : 0；越界返回 true。
//   实参 -2：当前地图的玩家名单多于 1 项才 false（即"随机"仍合法）。
//   实参 -3：直接 true。
//   否则在当前地图的玩家名单里找 HouseTypeClass::FindIndex(name) == 实参。
bool MPCoopClass::func1C(int32 houseOrdinal)
{
    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);
    if (pCampaign == nullptr)
        return true;

    int32 mapIndex = 0;
    if (Info != nullptr && !Info->GetCurrentMap(&mapIndex))
        mapIndex = 0;

    if (mapIndex >= pCampaign->NumberOfCampaignMaps)
        return true;

    const int32 listCount = CoopMapHouses::Count(CampaignIndex, mapIndex);

    if (houseOrdinal == -2)
        return listCount > 1;

    if (houseOrdinal == -3)
        return true;

    for (int32 i = 0; i < listCount; ++i)
    {
        const char* pName = CoopMapHouses::Name(CampaignIndex, mapIndex, i);
        if (pName == nullptr)
            continue;
        if (HouseTypeClass::FindIndex(pName) == houseOrdinal)
            return true;
    }
    return false;
}

// asm: MPCoop_func18 (0x5C1DD0)
//   1) 无记录 -> false
//   2) 取战役总数与"当前选中项"（原版是 0x553 组合框的 CB_GETCURSEL，
//      本工程没有控件，用 +33C 表示选中项）
//   3) 选中项 >= 总数：把 Records[sel] 换成新记录、SelectedSlot = sel；
//      否则重填名字并 Load()，成功置 LoadPending
//   4) node = Get(+33C)；无 node / 当前图越界 / 地图数 0 -> false
//   5) 逐个名册项：国家不在当前地图玩家名单里时，名单多于 1 项就置 -2，
//      否则置该名单第 0 项解析出的国家号
bool MPCoopClass::func18()
{
    if (Info == nullptr)
        return false;

    const int32 count = CampaignCount();
    const int32 selected = CampaignIndex;    // asm: sub_5E66D0（CB_GETCURSEL）

    if (selected >= count)
    {
        delete Info;
        Info = nullptr;

        if (selected >= 0 && selected < Records.Count)
        {
            Info = Records[selected];
            Records[selected] = new CoopGameInfoRecord();
            if (Info != nullptr)
                Info->SelectCampaign(selected);
        }
        SelectedSlot = selected;

        if (CoopNameNodes::Count() == 2 && Info != nullptr)
            Info->SetNames(CoopNameNodes::Name(0), CoopNameNodes::Name(1));
    }
    else
    {
        if (CoopNameNodes::Count() == 2)
            Info->SetNames(CoopNameNodes::Name(0), CoopNameNodes::Name(1));

        // asm: mov ecx,[esi+40h] / call sub_49C0D0（读 coopsave.ini）
        if (!Info->Load())
            return false;
        LoadPending = true;
    }

    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);
    if (pCampaign == nullptr)
        return false;

    int32 mapIndex = 0;
    if (!Info->GetCurrentMap(&mapIndex))
        mapIndex = 0;
    if (mapIndex >= pCampaign->NumberOfCampaignMaps)
        return false;

    const int32 mapCount = pCampaign->NumberOfCampaignMaps;
    if (mapCount == 0)
        return false;

    const int32 listCount = CoopMapHouses::Count(CampaignIndex, mapIndex);

    for (int32 i = 0; i < CoopNameNodes::Count(); ++i)
    {
        const int32 country = CoopNameNodes::Country(i);
        if (func1C(country))
            continue;

        int32 fallback = -2;
        if (listCount <= 1)
        {
            const char* pName = CoopMapHouses::Name(CampaignIndex, mapIndex, 0);
            fallback = (pName != nullptr) ? HouseTypeClass::FindIndex(pName) : -2;
        }
        CoopNameNodes::SetCountry(i, fallback);   // asm: sub_696F90
    }

    return true;
}

// asm: MPCoop_func20 (0x5C1A20)
//   dword_8316A4 = -1；打 "Sending observer slot of 8"；发 "O8"。
//   面板存在（0xC2 / 0xBC 两个对话框）且已选战役时，用当前地图名反查战役；
//   查不到且按键不是 0x6B 就丢弃旧记录。
//   无记录时新建、SelectCampaign(0)，再用 sub_5C1980 反查战役号，
//   找到就改用该战役并把名字打出来，找不到回到 0 号。
//   最后 Dirty = 1，并刷新面板。
void MPCoopClass::func20()
{
    s_ObserverSlotGlobal = -1;                       // asm: mov dword_8316A4,-1
    WWDebugString("Sending observer slot of 8\n");   // asm: aSendingObser_2
    SessionSend("O8");                               // asm: sub_5E3720("O8")

    // asm: 两个对话框探测（0xC2 / 0xBC）。本工程没有窗口层，
    //      这两次 sub_775940 恒为空，等价于"面板不可见"分支。
    const bool dialogVisible = false;

    if (dialogVisible && Info != nullptr)
    {
        int32 campaignIndex = 0;
        if (Info->GetCampaignIndex(&campaignIndex))
        {
            const int32 found = FindCampaignByMapName(Info->GetMapName(-1));
            if (found != campaignIndex)
            {
                delete Info;
                Info = nullptr;
            }
        }
    }

    if (Info == nullptr)
    {
        Info = new CoopGameInfoRecord();
        Info->SelectCampaign(0);

        const int32 found = FindCampaignByMapName(Info->GetMapName(-1));
        if (found < 0)
        {
            CampaignIndex = 0;
            Info->SelectCampaign(0);
        }
        else
        {
            CampaignIndex = found;
            Info->SelectCampaign(found);
            WWDebugString("@@@ Looking for %s\n", Info->GetMapName(-1));
        }
    }

    Dirty = true;                                    // asm: mov byte [esi+340h],1
    func90();                                        // asm: sub_5C2500（面板刷新）
}

// asm: MPCoop_func24 (0x5C1CF0) —— mov byte [ecx+340h],1
void MPCoopClass::func24()
{
    Dirty = true;
}

// asm: MPCoop_func28 (0x5C1FB0)
//   当前地图玩家名单恰好 1 项时返回该国的 HouseTypeClass 下标；
//   无 node / 越界 / 名单 0 项 / 多于 1 项一律返回 -2。
int32 MPCoopClass::func28()
{
    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);
    if (pCampaign == nullptr)
        return -2;

    int32 mapIndex = 0;
    if (Info != nullptr && !Info->GetCurrentMap(&mapIndex))
        mapIndex = 0;

    if (mapIndex < 0 || mapIndex >= pCampaign->NumberOfCampaignMaps)
        return -2;

    const int32 listCount = CoopMapHouses::Count(CampaignIndex, mapIndex);
    if (listCount == 0)
        return -2;
    if (listCount > 1)
        return -2;

    const char* pName = CoopMapHouses::Name(CampaignIndex, mapIndex, 0);
    if (pName == nullptr)
        return -2;
    return HouseTypeClass::FindIndex(pName);
}

// asm: MPCoop_func38 (0x5C2640)
//   对每个名册项调用 sub_5C23B0（CoopCampaignDialog::BuildMapKey 那一段）
//   重建地图选择控件，最后 sub_5C21D0 刷新可见性。控件调用在本工程没有
//   实体，键串本身仍按原版格式算出来并记录。
void MPCoopClass::func38()
{
    const int32 count = CoopNameNodes::Count();
    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);

    for (int32 i = 0; i < count; ++i)
    {
        char key[0x80];
        key[0] = '\0';
        CoopCampaignDialog::BuildMapKey(pCampaign, i, key, sizeof(key));
        WWDebugString("@@@ coop map key[%d] = %s\n", i, key);
    }
}

// asm: MPCoop_func44 (0x5C1CE0)
//   战役总数为 0 -> -1；否则保证记录存在、SelectCampaign(0)、取当前地图名，
//   在场景名表里线性查找，命中返回下标；未命中先调虚表 +0x10（func10），
//   再返回 -1。
//
//   缺口：原版的场景名表是全局 dword_A8B8CC（dword_A8B8D8 为条数），
//   本工程没有这张表，只有 SessionClass 的当前地图名可比对，因此只做
//   单点比较；未命中时仍按 asm 走 func10 + (-1)。
int32 MPCoopClass::func44()
{
    const int32 count = CampaignCount();
    if (count == 0)
        return -1;

    if (Info == nullptr)
        Info = new CoopGameInfoRecord();

    Info->SelectCampaign(0);

    const char* pMapName = Info->GetMapName(Info->CurrentMap);
    SessionClass* pSession = Session();
    if (pSession != nullptr && pMapName != nullptr && pMapName[0] != '\0')
    {
        if (_strcmpi(pSession->MapName, pMapName) == 0)
            return pSession->ScenarioIndex;
    }

    func10();       // asm: mov eax,[ebp+0] / call dword ptr [eax+10h]
    return -1;
}

// asm: MPCoop_func4C (0x5C3A40)
//   面板初始化：保证记录存在（无则 new + SelectCampaign(0)），把战役表里
//   每个战役名读出来拼进选择列表，再按存档列表标记"可续玩"。
//   控件（两个组合框 + 列表）在本工程没有实体；这里保留数据准备段：
//   记录创建、战役名收集、存档存在性检查。
void MPCoopClass::func4C()
{
    if (Info == nullptr)
    {
        Info = new CoopGameInfoRecord();   // asm: push 70h / call new / sub_49B610
        Info->SelectCampaign(0);           // asm: sub_49CAF0
        if (CoopNameNodes::Count() == 2)
            Info->SetNames(CoopNameNodes::Name(0), CoopNameNodes::Name(1));
    }

    // asm: sub_49E4F0（战役数）+ sub_49E740（把每个战役名 wcscpy 进列表）
    const int32 count = CampaignCount();
    for (int32 i = 0; i < count; ++i)
    {
        CoopCampaignClass* pCampaign = Campaign(i);
        if (pCampaign == nullptr)
            continue;
        WWDebugString("@@@ coop campaign %d: %ls\n", i,
                      pCampaign->pName != nullptr ? pCampaign->pName : L"");
    }

    // asm: sub_49D120（读存档列表）+ sub_49D390（Exists：section 的
    //      "Map%d" 不为 "undefined"）/ sub_49D070 + sprintf("%s.MAP") 比较。
    for (int32 i = 0; i < count; ++i)
    {
        CoopCampaignClass* pCampaign = Campaign(i);
        if (pCampaign == nullptr || pCampaign->Maps == nullptr)
            continue;

        for (int32 m = 0; m < pCampaign->NumberOfCampaignMaps; ++m)
        {
            if (ResourceExists(pCampaign->Maps[m].FileName))
            {
                WWDebugString("@@@ coop save available: %s\n",
                              pCampaign->Maps[m].FileName);
            }
        }
    }
}

// asm: MPCoop_func58 (0x5C2040)
//   无记录时先走 sub_5C3EC0（自动选图）并置 Dirty；
//   记录里的战役号与参数不同则置 Dirty；
//   随后 sub_49CCF0 重置记录、写回 +33C。
bool MPCoopClass::func58(int32 campaignIndex)
{
    if (Info == nullptr)
    {
        Info = new CoopGameInfoRecord();
        Info->SelectCampaign(0);
        Dirty = true;        // asm: mov byte [esi+340h],1
    }

    if (Info == nullptr)
        return false;

    int32 current = -1;
    if (!Info->GetCampaignIndex(&current))
        current = -1;

    if (current != campaignIndex)
        Dirty = true;

    CoopCampaignClass* pCampaign = Campaign(campaignIndex);
    const int32 mapCount = (pCampaign != nullptr)
        ? pCampaign->NumberOfCampaignMaps : 0;

    Info->ResetForCampaign(campaignIndex, mapCount);   // asm: sub_49CCF0
    CampaignIndex = campaignIndex;                     // asm: mov [esi+33Ch],edi
    return true;
}

// asm: MPCoop_func5C (0x5C2100)
//   Info->+38 取当前图；sub_49CC50 把两份成绩包累加进去；若图号变化或
//   Dirty 置位则刷新面板并清 Dirty。
bool MPCoopClass::func5C(const CoopScorePacket& a, const CoopScorePacket& b,
                         int32 time)
{
    if (Info == nullptr)
        return false;

    int32 mapIndex = 0;
    if (!Info->GetCurrentMap(&mapIndex))
        mapIndex = 0;

    const bool ok = Info->ApplyStats(mapIndex, a, b, time);   // sub_49CC50

    if (Dirty)
    {
        func90();       // asm: sub_5C2500（面板刷新）
        Dirty = false;  // asm: mov byte [ebp+340h],0
    }
    return ok;
}

// asm: MPCoop_func60 (0x5C2680)
//   无记录 -> false；取 +44；若面板存在且 Dirty 则先刷新并清 Dirty；
//   最后 sub_49D680 落盘并原样返回其结果。
bool MPCoopClass::func60()
{
    if (Info == nullptr)
        return false;

    int32 campaignIndex = 0;
    if (!Info->GetCampaignIndex(&campaignIndex))
        return false;

    if (Dirty)
    {
        func90();
        Dirty = false;
    }

    return Info->Save();    // asm: mov ecx,[esi+40h] / call sub_49D680
}

// asm: MPCoop_func64 (0x5C26D0)
//   取当前战役，按地图数拼 "C1,%d"（asm 里是 movsx 的有符号字节），
//   清空 8 组槽位表，再逐图抽一个 0..7 的 AI 号（避开 dword_ABF460 里
//   已占用的值），追加 ",%d,%d"，最后 trace + 发送。
bool MPCoopClass::func64()
{
    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);
    if (pCampaign == nullptr)
        return false;

    const int32 mapCount = pCampaign->NumberOfCampaignMaps;
    char packet[0x400];
    std::snprintf(packet, sizeof(packet), "C1,%d",
                  static_cast<signed char>(mapCount));

    for (int32 i = 0; i < AI_SLOT_COUNT; ++i)
    {
        AISlots[i].Maps.Clear();
        AISlots[i].AIs.Clear();
        AIHouseTaken[i] = AI_TAKEN_SENTINEL;
    }

    for (int32 map = 0; map < mapCount && map < AI_SLOT_COUNT; ++map)
    {
        // asm: Random2Class::operator()(0,7) + 与 dword_ABF460 逐项比较
        int32 ai = AI_TAKEN_SENTINEL;
        for (;;)
        {
            int32 draw = 0;
            if (ScenarioClass::Instance != nullptr)
                draw = ScenarioClass::Instance->Random(0, 7);
            if (draw < 0 || draw > 7)
                draw = 0;

            bool taken = false;
            for (int32 i = 0; i < AI_SLOT_COUNT; ++i)
            {
                if (AIHouseTaken[i] == draw)
                {
                    taken = true;
                    break;
                }
            }
            if (!taken)
            {
                ai = draw;
                break;
            }
        }

        AIHouseTaken[map] = ai;
        AISlots[map].Maps.Add(map);
        AISlots[map].AIs.Add(ai);

        char segment[0x20];
        std::snprintf(segment, sizeof(segment), ",%d,%d", map, ai);
        std::strcat(packet, segment);
    }

    WWDebugString("@@@@ Sending campaign game AI settings: %s\n", packet);
    SessionSend(packet);                     // asm: sub_5E3720
    WWDebugString("@@@@ Sent\n");
    return true;
}

// asm: MPCoop_func68 (0x5C30C0)
//   avail = sub_5E6520(gScenarioIndex) - Scenario+0x11E4（已存在房数）
//   打 "@@@@ Adding %d AI players"；avail <= 0 -> false；gAIPlayers = avail。
//   node = Get(+33C)；无 node 或地图数 0 或当前图越界 -> false。
//   从第 "已存在房数" 号槽开始，把可用的前 avail 个槽标成 AI：
//   难度取 unk_A8EB68（0 -> 2，1 -> 1，其余 0），并把起始点/队伍等
//   并行数组写成 -2；其余槽写成 -1。
bool MPCoopClass::func68()
{
    SessionClass* pSession = Session();
    if (pSession == nullptr)
        return false;

    const int32 available = ModemHostClass::Scenario_Player_Count(
                                pSession->ScenarioIndex) -
                            HouseClass::ArrayCount;
    WWDebugString("@@@@ Adding %d AI players\n", available);

    if (available <= 0)
        return false;

    pSession->AIPlayers = available;         // asm: mov gAIPlayers,ebp

    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);
    if (pCampaign == nullptr)
        return false;
    if (pCampaign->NumberOfCampaignMaps == 0)
        return false;

    int32 mapIndex = 0;
    if (Info != nullptr && !Info->GetCurrentMap(&mapIndex))
        mapIndex = 0;
    if (mapIndex >= pCampaign->NumberOfCampaignMaps)
        return false;

    const int32 difficulty = (pSession->AIDifficulty == 0) ? 2
        : (pSession->AIDifficulty == 1 ? 1 : 0);

    int32 assigned = 0;
    for (int32 slot = HouseClass::ArrayCount; slot < MAX_SESSION_PLAYERS; ++slot)
    {
        SessionPlayer* pPlayer = pSession->GetMutablePlayer(slot);
        if (pPlayer == nullptr)
            continue;

        if (assigned < available)
        {
            WWDebugString("@@@@ Setting slot %d for AI\n", slot);
            pPlayer->IsAI = true;
            pPlayer->Side = difficulty;
            pPlayer->StartingSpot = -2;      // asm: [esi+20h] = -2
            pPlayer->Team = -2;              // asm: [esi+40h] = -2
            WWDebugString("@@@@     AI difficulty: %d\n", pPlayer->Side);
            ++assigned;
        }
        else
        {
            pPlayer->IsAI = false;
            pPlayer->StartingSpot = -1;
            pPlayer->Team = -1;
        }
    }

    return true;
}

// asm: MPCoop_func6C (0x5C34E0) / MPCoop_func70 (0x5C3610)
//   两者同构，只是取 [node+0x14]（玩家名单）还是 [node+0x2C]（敌方名单）：
//   在当前图里随机抽一个 0..(房数-1) 的下标，直到该国的
//   INIClass_FindHouseIndex 落在名单里；随后把该国的显示名打出来。
namespace
{
int32 RandomHouseFromList(const std::vector<std::string>* pList,
                          const char* pTrace)
{
    if (pList == nullptr || pList->empty())
        return 0;

    const int32 maxIndex = static_cast<int32>(pList->size()) - 1;

    for (;;)
    {
        int32 draw = 0;
        if (ScenarioClass::Instance != nullptr)
            draw = ScenarioClass::Instance->Random(0, maxIndex);
        if (draw < 0 || draw > maxIndex)
            draw = 0;

        bool found = false;
        for (size_t i = 0; i < pList->size(); ++i)
        {
            // asm: INIClass_FindHouseIndex(名单[i]) == draw -> bl = 1
            if (HouseTypeClass::FindIndex((*pList)[i].c_str()) == draw)
            {
                found = true;
                break;
            }
        }
        if (!found)
            continue;

        const char* pName = nullptr;
        if (draw >= 0 && draw < HouseTypeClass::ArrayCount)
        {
            HouseTypeClass* pType = HouseTypeClass::Array[draw];
            if (pType != nullptr)
                pName = pType->Name;
        }
        WWDebugString(pTrace, draw, pName != nullptr ? pName : "");
        return draw;
    }
}
} // namespace

void MPCoopClass::func6C()
{
    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);
    if (pCampaign == nullptr)
        return;

    int32 mapIndex = 0;
    if (Info != nullptr && !Info->GetCurrentMap(&mapIndex))
        mapIndex = 0;
    if (mapIndex < 0 || mapIndex >= pCampaign->NumberOfCampaignMaps)
        return;

    // asm 的返回值在 eax；基类槽声明成 void，因此把结果写进 RandomHousePick。
    RandomHousePick = RandomHouseFromList(
        PlayerList(CampaignIndex, mapIndex),
        "@@@@ Choosing a random player house: %d(%s)\n");
}

int32 MPCoopClass::func70()
{
    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);
    if (pCampaign == nullptr)
        return 0;

    int32 mapIndex = 0;
    if (Info != nullptr && !Info->GetCurrentMap(&mapIndex))
        mapIndex = 0;
    if (mapIndex < 0 || mapIndex >= pCampaign->NumberOfCampaignMaps)
        return 0;

    return RandomHouseFromList(EnemyList(CampaignIndex, mapIndex),
                               "@@@@ Choosing a random AI house: %d(%s)\n");
}

// asm: MPCoop_func74 (0x5C2D70)
//   先把 8 组槽位清空（每组两个 VectorClass<int> 各调一次 Clear + Resize）；
//   然后 mapCount 轮 × 8 槽，每轮每槽读两个逗号分隔的整数，
//   分别进 Maps / AIs；缺项按 -1 记。
void MPCoopClass::func74(int32 mapCount, const char* pList)
{
    for (int32 i = 0; i < AI_SLOT_COUNT; ++i)
    {
        AISlots[i].Maps.Clear();
        AISlots[i].AIs.Clear();
    }

    if (mapCount <= 0 || pList == nullptr)
        return;

    char* work = new char[std::strlen(pList) + 1];
    std::strcpy(work, pList);

    char* pToken = std::strtok(work, ",");
    for (int32 round = 0; round < mapCount; ++round)
    {
        for (int32 slot = 0; slot < AI_SLOT_COUNT; ++slot)
        {
            const int32 mapOrdinal =
                (pToken != nullptr) ? static_cast<int32>(std::atol(pToken)) : -1;
            pToken = (pToken != nullptr) ? std::strtok(nullptr, ",") : nullptr;

            const int32 aiOrdinal =
                (pToken != nullptr) ? static_cast<int32>(std::atol(pToken)) : -1;
            pToken = (pToken != nullptr) ? std::strtok(nullptr, ",") : nullptr;

            AISlots[slot].Maps.Add(mapOrdinal);
            AISlots[slot].AIs.Add(aiOrdinal);
        }
    }

    delete[] work;
}

// asm: MPCoop_func78 (0x5C2190)
//   GetDlgItem(hDlg, 0x732) 存在则 PostMessage(hWnd, 0x405, 1, unk_A8EB68)。
//   本工程没有窗口层：控件句柄恒为空，函数如实退化为"无控件可投递"。
void MPCoopClass::func78()
{
    // asm: push 732h / call GetDlgItem ; test eax,eax ; jz ret
    void* hItem = nullptr;      // 窗口层缺失（见 ModemClass 的同一降级点）
    if (hItem == nullptr)
        return;

    // asm: PostMessageA(hWnd, 405h, 1, unk_A8EB68)
}

// asm: MPCoop_func7C (0x5C2CF0)
//   面板存在（或 0xBC6 控件存在）时刷新可见性，恒返回 1。
bool MPCoopClass::func7C()
{
    const bool dialogVisible = false;   // asm: sub_5E26C0 / sub_775940(0xBC6)
    if (dialogVisible)
        func90();                       // asm: sub_5C21D0(this, 1)

    return true;
}

// asm: MPCoop_func90 (0x5C3F80)
//   逐席位把 [node+0x6B]（席位号）与国名拼成 0x104 缓冲，写进对话框控件。
//   控件层缺失，这里按 asm 的取数顺序把同一份文本记录出来。
void MPCoopClass::func90()
{
    CoopCampaignClass* pCampaign = Campaign(CampaignIndex);
    if (pCampaign == nullptr)
        return;

    char text[0x104];
    const int32 count = CoopNameNodes::Count();
    for (int32 i = 0; i < count; ++i)
    {
        const char* pName = CoopNameNodes::Name(i);
        std::snprintf(text, sizeof(text), "%d: %s", CoopNameNodes::Slot(i),
                      pName != nullptr ? pName : "");
        WWDebugString("@@@ coop slot %d = %s\n", i, text);
    }
}

// asm: MPCoop_func94 (0x5C40A0)
//   无记录时 new + SelectCampaign(0) + 名册两项时重填名字；
//   然后取记录的地图名，检查该图是否已有存档（sub_49D390），
//   有则准备 "TXT_COOP_USE_SAVE" 提示。
void MPCoopClass::func94()
{
    if (Info == nullptr)
    {
        Info = new CoopGameInfoRecord();
        Info->SelectCampaign(CampaignIndex);
        if (CoopNameNodes::Count() == 2)
            Info->SetNames(CoopNameNodes::Name(0), CoopNameNodes::Name(1));
    }

    const char* pMapName = Info->GetMapName(Info->CurrentMap);
    if (pMapName == nullptr || pMapName[0] == '\0')
        return;

    if (ResourceExists(pMapName))
    {
        // asm: Get_String("TXT_COOP_USE_SAVE") + 控件显示；控件层缺失，
        //      这里只记录判定结果。
        WWDebugString("@@@ coop save found for %s (TXT_COOP_USE_SAVE)\n",
                      pMapName);
    }
}

// asm: MPCoop_func98 (0x5C3FA0) —— mov eax,2
int32 MPCoopClass::func98() const
{
    return 2;
}

// asm: MPCoop_func9C (0x5C3F30)
//   有面板时刷新可见性并重建；随后把名册每项的 +0x6B 写成它的下标。
void MPCoopClass::func9C()
{
    const bool dialogVisible = false;   // asm: sub_5E26C0
    if (dialogVisible)
        func90();

    for (int32 i = 0; i < CoopNameNodes::Count(); ++i)
        CoopNameNodes::SetSlot(i, i);   // asm: mov [edx+6Bh],eax
}

// asm: MPCoop_funcA0 (0x5C3760)
//   win = 任一 HouseClass::Is_Human() 且 [house+0x1F5]==0（未败）的房存在；
//   打 "@@@@ End-of-Game: PlayerWins:%d, win:%d"；
//   win 且记录存在时 MP_Coop_Save，并按结果提示 TXT_COOP_SAVE_SUCCESS /
//   TXT_COOP_SAVE_FAILED；
//   随后用记录的地图名拼 "%s.MAP" 作为下一张图。
void MPCoopClass::funcA0()
{
    bool playerWins = false;
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i)
    {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr)
            continue;
        if (!pHouse->Is_Human())
            continue;
        // asm: mov al,[edx+1F5h] / test al,al / jnz（非 0 表示已败）
        if (!pHouse->Defeated())
        {
            playerWins = true;
            break;
        }
    }

    WWDebugString("@@@@ End-of-Game: PlayerWins:%d, win:%d\n",
                  playerWins ? 1 : 0, 0);

    if (playerWins && Info != nullptr)
    {
        const bool saved = Info->Save();    // asm: call MP_Coop_Save
        WWDebugString(saved ? "@@@ coop save ok (TXT_COOP_SAVE_SUCCESS)\n"
                            : "@@@ coop save failed (TXT_COOP_SAVE_FAILED)\n");
    }

    if (Info == nullptr)
        return;

    char nextMap[0x104];
    std::snprintf(nextMap, sizeof(nextMap), "%s.MAP",
                  Info->GetMapName(-1));
    WWDebugString("@@@ coop next map = %s\n", nextMap);

    // 缺口：asm 随后把这张图名写进 Scenario+4，并置 Scenario+0x34CC=-1、
    //   Scenario+0x34AE=1，再在场景名表里定位。"写 Scenario 内部字段"与
    //   "场景名表"都属于 Scenario/ 与本工程未建模的全局，Network/ 侧不改写
    //   它们，因此停在这里（判定与落盘已完成）。
}

// asm: MPCoop_funcA4 (0x5C4510)
//   条件：记录存在、当前图 >= 地图数（打完了）、arg 非 0；
//   然后 new 一个成绩项对象（sub_470C00 构造），把记录里的
//   战役号/图号/统计交给 sub_5C4F30 填进项里并入列。
//   成绩屏（Game/）不在本层，这里按同一顺序把项算出来并登记。
void MPCoopClass::funcA4(bool finished)
{
    if (Info == nullptr)
        return;

    int32 mapCount = 0;
    if (!Info->GetMapCount(&mapCount))
        return;

    int32 currentMap = 0;
    if (!Info->GetCurrentMap(&currentMap))
        return;

    if (!finished)
        return;
    if (currentMap < mapCount)
        return;

    int32 campaignIndex = 0;
    if (!Info->GetCampaignIndex(&campaignIndex))
        campaignIndex = -1;

    WWDebugString("@@@ coop score entry: campaign %d, map %d/%d, "
                  "kills %d/%d, lost %d/%d, score %d/%d\n",
                  campaignIndex, currentMap, mapCount,
                  Info->Kills1, Info->Kills2, Info->Lost1, Info->Lost2,
                  Info->Score1, Info->Score2);
}

// asm: MPCoop_funcA8 (0x5C4290)
//   为 +50 建 8 字节的计时/统计对象（sub_46D330），再按记录把统计项入列。
//   原版把结果推给成绩屏；本工程成绩屏在 Game/，这里记录同一份统计。
void MPCoopClass::funcA8()
{
    if (Info == nullptr)
        return;

    int32 mapCount = 0;
    if (!Info->GetMapCount(&mapCount))
        return;
    int32 currentMap = 0;
    if (!Info->GetCurrentMap(&currentMap))
        return;

    WWDebugString("@@@ coop stats: map %d/%d time %d "
                  "kills %d+%d lost %d+%d score %d+%d\n",
                  currentMap, mapCount, Info->Time,
                  Info->Kills1, Info->Kills2, Info->Lost1, Info->Lost2,
                  Info->Score1, Info->Score2);
}

// asm: MPCoop_funcAC (0x5C4440)
//   组开局广播包：把本对象的设置块（原版 +0xC4 起 0x60 字节）与
//   名册数据拷进帧，最后经会话对象虚表 +0x2C 发出。
//
//   缺口：那个 0x60 字节设置块是面板对象的内部状态，本工程没有对应结构，
//   这里只把记录里可表达的部分（战役号/图号/统计数据/主机名）按同一顺序
//   打包，并沿用 Networking 的 SPECIAL 事件发送。
void MPCoopClass::funcAC()
{
    if (Info == nullptr)
        return;

    char packet[0x200];
    std::snprintf(packet, sizeof(packet), "S0,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s",
                  CampaignIndex, Info->CurrentMap, Info->MapCount,
                  Info->Kills1, Info->Kills2,
                  Info->Lost1, Info->Lost2,
                  Info->Score1, Info->Score2,
                  Info->Name1);

    WWDebugString("@@@ coop start packet: %s\n", packet);
    SessionSend(packet);
}

// asm: MPCoop_funcB0 (0x5C4CE0)
//   先 vec_SS_Clear（清成绩屏条目），再逐战役逐图与 "<地图>.MAP" 比较，
//   命中则 new 一个 0x85 的节点、把图名 wcscpy 进去，并把下标写到 +0x6F。
void MPCoopClass::funcB0()
{
    const int32 count = CampaignCount();
    for (int32 i = 0; i < count; ++i)
    {
        CoopCampaignClass* pCampaign = Campaign(i);
        if (pCampaign == nullptr || pCampaign->Maps == nullptr)
            continue;

        for (int32 m = 0; m < pCampaign->NumberOfCampaignMaps; ++m)
        {
            char name[0x104];
            std::snprintf(name, sizeof(name), "%s.MAP",
                          pCampaign->Maps[m].FileName);

            if (!ResourceExists(pCampaign->Maps[m].FileName))
                continue;

            // asm: mov [edx+6Fh],esi —— 命中后记下该存档的下标
            SelectedSlot = m;
            WWDebugString("@@@ coop save slot %d -> %s\n", m, name);
            break;
        }
    }
}

// asm: MPCoop_funcB4 (0x5C4E90, retn 1Ch)
//   先把第 7 个实参（出参）写 0；有记录时（释放完传入缓冲）返回 true，
//   无记录时同样释放但返回 false。释放受第 4 个实参的次低字节控制
//   （asm: mov cl,[esp+arg_D]）。
bool MPCoopClass::funcB4(void* memory, int32 a2, int32 a3, int32 a4,
                         int32 a5, int32 a6)
{
    (void)a2;
    (void)a3;
    (void)a5;
    (void)a6;

    const bool owned = ((a4 >> 8) & 0xFF) != 0;   // asm: arg_D
    if (memory != nullptr && owned)
        YRMemory::Deallocate(memory);

    return Info != nullptr;
}

// asm: MPCoop_funcB8 (0x5C4660)
//   记录存在且 +40 == +38（最后一张图打完）时返回 1 << (+44)，
//   否则 0；命中时打出 "Woohoo!, I just finished cooperative campaign %d..."。
int32 MPCoopClass::funcB8()
{
    if (Info == nullptr)
        return 0;

    int32 mapCount = 0;
    if (!Info->GetMapCount(&mapCount))
        return 0;

    int32 currentMap = 0;
    if (!Info->GetCurrentMap(&currentMap))
        return 0;

    if (mapCount != currentMap)
        return 0;

    int32 campaignIndex = 0;
    if (!Info->GetCampaignIndex(&campaignIndex))
        campaignIndex = 0;

    const int32 code = 1 << campaignIndex;
    WWDebugString("Woohoo!, I just finished cooperative campaign %d.  "
                  "I get persistant accomplishment code 0x%032x\n",
                  campaignIndex, code);
    return code;
}

// asm: MPCoop_funcBC (0x5C4ED0) —— xor al,al
bool MPCoopClass::funcBC() const
{
    return false;
}
