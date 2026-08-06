#include "WarheadTypeClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Particles/ParticleTypeClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Game.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

DynamicVectorClass<WarheadTypeClass*>* WarheadTypeClass::Array = nullptr;

WarheadTypeClass* WarheadTypeClass::Find(const char* pID) {
    if (!Array) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        WarheadTypeClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

WarheadTypeClass* WarheadTypeClass::FindOrAllocate(const char* pID) {
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    WarheadTypeClass* found = Find(pID);
    if (found) return found;
    WarheadTypeClass* newItem = GameCreate<WarheadTypeClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

WarheadTypeClass::WarheadTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID), ArrayIndex(-1), IsWallDestroyer(false),
      IsWoodDestroyer(false), IsWallAbsoluteDestroyer(false),
      IsTiberiumDestroyer(false), IsOreDestroyer(false),
      ProneDamage(0.0f), InfDeath(0), CellSpread(0.0f),
      PercentAtMax(1.0f), IsSparky(false), IsFire(false), IsSmoke(false),
      IsGas(false), IsLocomotor(false), IsSonic(false),
      IsRadiation(false), IsPsychic(false), IsMechanical(false),
      Bullets(false), Particle(nullptr), Anim(nullptr),
      SplashList(nullptr), IsAttachedParticle(false),
      Temporal(false), Parasite(false), Bright(false),
      PenetratesBunker(false), AnimListCount(0), DebrisCount(0),
      DebrisMaximumsCount(0), DebrisTypes(nullptr), DebrisMaximums(nullptr) {
    for (int32 i = 0; i < 11; ++i) {
        Verses[i] = 1.0f;
    }
    for (int32 i = 0; i < MAX_ANIM_LIST; ++i) {
        AnimList[i] = nullptr;
    }
}

WarheadTypeClass::~WarheadTypeClass() {
    if (DebrisTypes) delete[] DebrisTypes;
    if (DebrisMaximums) delete[] DebrisMaximums;
    if (SplashList) delete[] (char*)SplashList;
}

HRESULT WarheadTypeClass::GetClassID(CLSID* pClassID) {
    if (pClassID) {
        pClassID->Data1 = 0xE2E2E2E2;
        for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0;
        return S_OK;
    }
    return E_POINTER;
}

HRESULT WarheadTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Chain to parent class
    hr = AbstractTypeClass::Load(pStm);
    if (hr < 0) return E_FAIL;

    // Read ArrayIndex
    hr = pStm->Read(&ArrayIndex, sizeof(ArrayIndex), &read);
    if (hr < 0 || read != sizeof(ArrayIndex)) return E_FAIL;

    // Read flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsWallDestroyer         = (flags & 0x00000001) != 0;
    IsWoodDestroyer         = (flags & 0x00000002) != 0;
    IsWallAbsoluteDestroyer = (flags & 0x00000004) != 0;
    IsTiberiumDestroyer     = (flags & 0x00000008) != 0;
    IsOreDestroyer          = (flags & 0x00000010) != 0;
    IsSparky                = (flags & 0x00000020) != 0;
    IsFire                  = (flags & 0x00000040) != 0;
    IsSmoke                 = (flags & 0x00000080) != 0;
    IsGas                   = (flags & 0x00000100) != 0;
    IsLocomotor             = (flags & 0x00000200) != 0;
    IsSonic                 = (flags & 0x00000400) != 0;
    IsRadiation             = (flags & 0x00000800) != 0;
    IsPsychic               = (flags & 0x00001000) != 0;
    IsMechanical            = (flags & 0x00002000) != 0;
    Bullets                 = (flags & 0x00004000) != 0;
    IsAttachedParticle      = (flags & 0x00008000) != 0;
    Temporal                = (flags & 0x00010000) != 0;
    Parasite                = (flags & 0x00020000) != 0;
    Bright                  = (flags & 0x00040000) != 0;
    PenetratesBunker        = (flags & 0x00080000) != 0;

    // Read numeric fields
    hr = pStm->Read(&ProneDamage, sizeof(ProneDamage), &read);
    if (hr < 0 || read != sizeof(ProneDamage)) return E_FAIL;

    hr = pStm->Read(&InfDeath, sizeof(InfDeath), &read);
    if (hr < 0 || read != sizeof(InfDeath)) return E_FAIL;

    hr = pStm->Read(&CellSpread, sizeof(CellSpread), &read);
    if (hr < 0 || read != sizeof(CellSpread)) return E_FAIL;

    hr = pStm->Read(&PercentAtMax, sizeof(PercentAtMax), &read);
    if (hr < 0 || read != sizeof(PercentAtMax)) return E_FAIL;

    // Read Verses array
    hr = pStm->Read(Verses, sizeof(Verses), &read);
    if (hr < 0 || read != sizeof(Verses)) return E_FAIL;

    // Read Particle (string ID)
    char particleName[0x18];
    hr = pStm->Read(particleName, sizeof(particleName), &read);
    if (hr < 0 || read != sizeof(particleName)) return E_FAIL;
    particleName[sizeof(particleName) - 1] = '\0';
    Particle = nullptr;
    if (particleName[0] && ParticleTypeClass::Array) {
        for (int32 i = 0; i < ParticleTypeClass::Array->Count; ++i) {
            ParticleTypeClass* pPT = ParticleTypeClass::Array->GetItem(i);
            if (pPT && pPT->GetName() && !_strcmpi(pPT->GetName(), particleName)) {
                Particle = pPT;
                break;
            }
        }
    }

    // Read Anim (string ID)
    char animName[0x18];
    hr = pStm->Read(animName, sizeof(animName), &read);
    if (hr < 0 || read != sizeof(animName)) return E_FAIL;
    animName[sizeof(animName) - 1] = '\0';
    Anim = animName[0] ? static_cast<AnimTypeClass*>(AnimTypeClass::Find(animName)) : nullptr;

    // Read SplashList (fixed buffer)
    char splashBuf[0x100];
    hr = pStm->Read(splashBuf, sizeof(splashBuf), &read);
    if (hr < 0 || read != sizeof(splashBuf)) return E_FAIL;
    splashBuf[sizeof(splashBuf) - 1] = '\0';
    if (SplashList) { delete[] (char*)SplashList; SplashList = nullptr; }
    if (splashBuf[0]) {
        char* pCopy = new char[0x100];
        std::memcpy(pCopy, splashBuf, 0x100);
        SplashList = pCopy;
    }

    // Read AnimList (count + string IDs)
    hr = pStm->Read(&AnimListCount, sizeof(AnimListCount), &read);
    if (hr < 0 || read != sizeof(AnimListCount)) return E_FAIL;
    if (AnimListCount < 0) AnimListCount = 0;
    if (AnimListCount > MAX_ANIM_LIST) AnimListCount = MAX_ANIM_LIST;
    for (int32 i = 0; i < MAX_ANIM_LIST; ++i) AnimList[i] = nullptr;
    for (int32 i = 0; i < AnimListCount; ++i) {
        char name[0x18];
        hr = pStm->Read(name, sizeof(name), &read);
        if (hr < 0 || read != sizeof(name)) return E_FAIL;
        name[sizeof(name) - 1] = '\0';
        if (name[0]) AnimList[i] = static_cast<AnimTypeClass*>(AnimTypeClass::Find(name));
    }

    // Read DebrisCount
    hr = pStm->Read(&DebrisCount, sizeof(DebrisCount), &read);
    if (hr < 0 || read != sizeof(DebrisCount)) return E_FAIL;
    if (DebrisCount < 0) DebrisCount = 0;

    // Read DebrisMaximumsCount
    hr = pStm->Read(&DebrisMaximumsCount, sizeof(DebrisMaximumsCount), &read);
    if (hr < 0 || read != sizeof(DebrisMaximumsCount)) return E_FAIL;
    if (DebrisMaximumsCount < 0) DebrisMaximumsCount = 0;

    // Read DebrisTypes (array of string IDs)
    if (DebrisTypes) { delete[] DebrisTypes; DebrisTypes = nullptr; }
    if (DebrisCount > 0) {
        DebrisTypes = new TechnoTypeClass*[DebrisCount];
        for (int32 i = 0; i < DebrisCount; ++i) DebrisTypes[i] = nullptr;
        for (int32 i = 0; i < DebrisCount; ++i) {
            char name[0x18];
            hr = pStm->Read(name, sizeof(name), &read);
            if (hr < 0 || read != sizeof(name)) return E_FAIL;
            name[sizeof(name) - 1] = '\0';
            if (name[0]) DebrisTypes[i] = static_cast<TechnoTypeClass*>(TechnoTypeClass::Find(name));
        }
    }

    // Read DebrisMaximums (array of ints)
    if (DebrisMaximums) { delete[] DebrisMaximums; DebrisMaximums = nullptr; }
    if (DebrisMaximumsCount > 0) {
        DebrisMaximums = new int32[DebrisMaximumsCount];
        hr = pStm->Read(DebrisMaximums, sizeof(int32) * DebrisMaximumsCount, &read);
        if (hr < 0 || read != sizeof(int32) * DebrisMaximumsCount) return E_FAIL;
    }

    return S_OK;
}

HRESULT WarheadTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Chain to parent class
    hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return E_FAIL;

    // Write ArrayIndex
    hr = pStm->Write(&ArrayIndex, sizeof(ArrayIndex), &written);
    if (hr < 0 || written != sizeof(ArrayIndex)) return E_FAIL;

    // Write flags as a bitmask
    uint32 flags = 0;
    if (IsWallDestroyer)         flags |= 0x00000001;
    if (IsWoodDestroyer)         flags |= 0x00000002;
    if (IsWallAbsoluteDestroyer) flags |= 0x00000004;
    if (IsTiberiumDestroyer)     flags |= 0x00000008;
    if (IsOreDestroyer)          flags |= 0x00000010;
    if (IsSparky)                flags |= 0x00000020;
    if (IsFire)                  flags |= 0x00000040;
    if (IsSmoke)                 flags |= 0x00000080;
    if (IsGas)                   flags |= 0x00000100;
    if (IsLocomotor)             flags |= 0x00000200;
    if (IsSonic)                 flags |= 0x00000400;
    if (IsRadiation)             flags |= 0x00000800;
    if (IsPsychic)               flags |= 0x00001000;
    if (IsMechanical)            flags |= 0x00002000;
    if (Bullets)                 flags |= 0x00004000;
    if (IsAttachedParticle)      flags |= 0x00008000;
    if (Temporal)                flags |= 0x00010000;
    if (Parasite)                flags |= 0x00020000;
    if (Bright)                  flags |= 0x00040000;
    if (PenetratesBunker)        flags |= 0x00080000;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    // Write numeric fields
    hr = pStm->Write(&ProneDamage, sizeof(ProneDamage), &written);
    if (hr < 0 || written != sizeof(ProneDamage)) return E_FAIL;

    hr = pStm->Write(&InfDeath, sizeof(InfDeath), &written);
    if (hr < 0 || written != sizeof(InfDeath)) return E_FAIL;

    hr = pStm->Write(&CellSpread, sizeof(CellSpread), &written);
    if (hr < 0 || written != sizeof(CellSpread)) return E_FAIL;

    hr = pStm->Write(&PercentAtMax, sizeof(PercentAtMax), &written);
    if (hr < 0 || written != sizeof(PercentAtMax)) return E_FAIL;

    // Write Verses array
    hr = pStm->Write(Verses, sizeof(Verses), &written);
    if (hr < 0 || written != sizeof(Verses)) return E_FAIL;

    // Write Particle (string ID)
    char particleName[0x18];
    std::memset(particleName, 0, sizeof(particleName));
    if (Particle && Particle->GetName()) {
        const char* pName = Particle->GetName();
        int32 j = 0;
        while (pName[j] && j < static_cast<int32>(sizeof(particleName)) - 1) {
            particleName[j] = pName[j]; ++j;
        }
    }
    hr = pStm->Write(particleName, sizeof(particleName), &written);
    if (hr < 0 || written != sizeof(particleName)) return E_FAIL;

    // Write Anim (string ID)
    char animName[0x18];
    std::memset(animName, 0, sizeof(animName));
    if (Anim && Anim->get_ID()) {
        const char* pID = Anim->get_ID();
        int32 j = 0;
        while (pID[j] && j < static_cast<int32>(sizeof(animName)) - 1) {
            animName[j] = pID[j]; ++j;
        }
    }
    hr = pStm->Write(animName, sizeof(animName), &written);
    if (hr < 0 || written != sizeof(animName)) return E_FAIL;

    // Write SplashList (fixed buffer)
    char splashBuf[0x100];
    std::memset(splashBuf, 0, sizeof(splashBuf));
    if (SplashList) {
        const char* pStr = (const char*)SplashList;
        int32 j = 0;
        while (pStr[j] && j < static_cast<int32>(sizeof(splashBuf)) - 1) {
            splashBuf[j] = pStr[j]; ++j;
        }
    }
    hr = pStm->Write(splashBuf, sizeof(splashBuf), &written);
    if (hr < 0 || written != sizeof(splashBuf)) return E_FAIL;

    // Write AnimList (count + string IDs)
    hr = pStm->Write(&AnimListCount, sizeof(AnimListCount), &written);
    if (hr < 0 || written != sizeof(AnimListCount)) return E_FAIL;
    int32 animListCount = AnimListCount;
    if (animListCount < 0) animListCount = 0;
    if (animListCount > MAX_ANIM_LIST) animListCount = MAX_ANIM_LIST;
    for (int32 i = 0; i < animListCount; ++i) {
        char name[0x18];
        std::memset(name, 0, sizeof(name));
        AnimTypeClass* pAnim = AnimList[i];
        if (pAnim && pAnim->get_ID()) {
            const char* pID = pAnim->get_ID();
            int32 j = 0;
            while (pID[j] && j < static_cast<int32>(sizeof(name)) - 1) {
                name[j] = pID[j]; ++j;
            }
        }
        hr = pStm->Write(name, sizeof(name), &written);
        if (hr < 0 || written != sizeof(name)) return E_FAIL;
    }

    // Write DebrisCount
    hr = pStm->Write(&DebrisCount, sizeof(DebrisCount), &written);
    if (hr < 0 || written != sizeof(DebrisCount)) return E_FAIL;

    // Write DebrisMaximumsCount
    hr = pStm->Write(&DebrisMaximumsCount, sizeof(DebrisMaximumsCount), &written);
    if (hr < 0 || written != sizeof(DebrisMaximumsCount)) return E_FAIL;

    // Write DebrisTypes (array of string IDs)
    int32 debrisCount = DebrisCount;
    if (debrisCount < 0) debrisCount = 0;
    for (int32 i = 0; i < debrisCount; ++i) {
        char name[0x18];
        std::memset(name, 0, sizeof(name));
        if (DebrisTypes && DebrisTypes[i] && DebrisTypes[i]->get_ID()) {
            const char* pID = DebrisTypes[i]->get_ID();
            int32 j = 0;
            while (pID[j] && j < static_cast<int32>(sizeof(name)) - 1) {
                name[j] = pID[j]; ++j;
            }
        }
        hr = pStm->Write(name, sizeof(name), &written);
        if (hr < 0 || written != sizeof(name)) return E_FAIL;
    }

    // Write DebrisMaximums (array of ints)
    if (DebrisMaximumsCount > 0 && DebrisMaximums) {
        hr = pStm->Write(DebrisMaximums, sizeof(int32) * DebrisMaximumsCount, &written);
        if (hr < 0 || written != sizeof(int32) * DebrisMaximumsCount) return E_FAIL;
    }

    return S_OK;
}

AbstractType WarheadTypeClass::WhatAmI() const {
    return AbstractType::WarheadType;
}

int32 WarheadTypeClass::Size() const {
    return sizeof(WarheadTypeClass);
}

bool WarheadTypeClass::LoadFromINIList(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);
    if (!pINI->SectionExists(sectionName)) return false;

    pINI->GetInteger(sectionName, "ArrayIndex", ArrayIndex);

    IsWallDestroyer = pINI->ReadBool(sectionName, "WallDestroyer", IsWallDestroyer);
    IsWoodDestroyer = pINI->ReadBool(sectionName, "WoodDestroyer", IsWoodDestroyer);
    IsWallAbsoluteDestroyer = pINI->ReadBool(sectionName, "WallAbsoluteDestroyer", IsWallAbsoluteDestroyer);
    IsTiberiumDestroyer = pINI->ReadBool(sectionName, "TiberiumDestroyer", IsTiberiumDestroyer);
    IsOreDestroyer = pINI->ReadBool(sectionName, "OreDestroyer", IsOreDestroyer);
    IsSparky = pINI->ReadBool(sectionName, "Sparky", IsSparky);
    IsFire = pINI->ReadBool(sectionName, "Fire", IsFire);
    IsSmoke = pINI->ReadBool(sectionName, "Smoke", IsSmoke);
    IsGas = pINI->ReadBool(sectionName, "Gas", IsGas);
    IsLocomotor = pINI->ReadBool(sectionName, "Locomotor", IsLocomotor);
    IsSonic = pINI->ReadBool(sectionName, "IsSonic", IsSonic);
    IsRadiation = pINI->ReadBool(sectionName, "IsRadiation", IsRadiation);
    IsPsychic = pINI->ReadBool(sectionName, "IsPsychic", IsPsychic);
    IsMechanical = pINI->ReadBool(sectionName, "IsMechanical", IsMechanical);
    Bullets = pINI->ReadBool(sectionName, "Bullets", Bullets);
    Temporal = pINI->ReadBool(sectionName, "Temporal", Temporal);
    Parasite = pINI->ReadBool(sectionName, "Parasite", Parasite);
    Bright = pINI->ReadBool(sectionName, "Bright", Bright);
    PenetratesBunker = pINI->ReadBool(sectionName, "PenetratesBunker", PenetratesBunker);
    IsAttachedParticle = pINI->ReadBool(sectionName, "IsAttachedParticle", IsAttachedParticle);

    ProneDamage = static_cast<float>(pINI->ReadDouble(sectionName, "ProneDamage", 1.0));
    CellSpread = static_cast<float>(pINI->ReadDouble(sectionName, "CellSpread", 0.0));
    PercentAtMax = static_cast<float>(pINI->ReadDouble(sectionName, "PercentAtMax", 1.0));

    pINI->GetInteger(sectionName, "InfDeath", InfDeath);

    static const char* armorNames[] = {"none", "flak", "plate", "light", "medium", "heavy", "wood", "steel", "concrete", "special_1", "special_2"};
    for (int32 i = 0; i < 11; ++i) {
        char keyName[32];
        snprintf(keyName, sizeof(keyName), "Versus.%s", armorNames[i]);
        double val = pINI->ReadDouble(sectionName, keyName, 1.0);
        Verses[i] = static_cast<float>(val);
    }

    char particleName[0x18];
    pINI->ReadString(sectionName, "Particle", "", particleName, sizeof(particleName));
    if (particleName[0] && _strcmpi(particleName, "<none>") != 0) {
        // Look up the particle type by name through its global array.
        Particle = nullptr;
        if (ParticleTypeClass::Array) {
            for (int32 pi = 0; pi < ParticleTypeClass::Array->Count; ++pi) {
                ParticleTypeClass* pPT = ParticleTypeClass::Array->GetItem(pi);
                if (pPT && pPT->GetName() &&
                    !_strcmpi(pPT->GetName(), particleName)) {
                    Particle = pPT;
                    break;
                }
            }
        }
    }

    char animName[0x18];
    pINI->ReadString(sectionName, "Anim", "", animName, sizeof(animName));
    if (animName[0] && _strcmpi(animName, "<none>") != 0) {
        Anim = static_cast<AnimTypeClass*>(AnimTypeClass::Find(animName));
    }

    char debrisTypes[256];
    pINI->ReadString(sectionName, "DebrisTypes", "", debrisTypes, sizeof(debrisTypes));
    if (debrisTypes[0]) {
        ParseDebris(debrisTypes);
    }

    char splashList[256];
    pINI->ReadString(sectionName, "SplashList", "", splashList, sizeof(splashList));
    if (splashList[0]) {
        ParseSplashList(splashList);
    }

    return true;
}

bool WarheadTypeClass::SaveToINIList(CCINIClass* pINI) {
    if (!pINI) return false;
    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);

    pINI->WriteInteger(sectionName, "ArrayIndex", ArrayIndex);

    pINI->WriteBool(sectionName, "WallDestroyer", IsWallDestroyer);
    pINI->WriteBool(sectionName, "WoodDestroyer", IsWoodDestroyer);
    pINI->WriteBool(sectionName, "WallAbsoluteDestroyer", IsWallAbsoluteDestroyer);
    pINI->WriteBool(sectionName, "TiberiumDestroyer", IsTiberiumDestroyer);
    pINI->WriteBool(sectionName, "OreDestroyer", IsOreDestroyer);
    pINI->WriteBool(sectionName, "Sparky", IsSparky);
    pINI->WriteBool(sectionName, "Fire", IsFire);
    pINI->WriteBool(sectionName, "Smoke", IsSmoke);
    pINI->WriteBool(sectionName, "Gas", IsGas);
    pINI->WriteDouble(sectionName, "ProneDamage", ProneDamage);
    pINI->WriteInteger(sectionName, "InfDeath", InfDeath);
    pINI->WriteDouble(sectionName, "CellSpread", CellSpread);
    pINI->WriteDouble(sectionName, "PercentAtMax", PercentAtMax);

    static const char* armorNames[] = {"none", "flak", "plate", "light", "medium", "heavy", "wood", "steel", "concrete", "special_1", "special_2"};
    for (int32 i = 0; i < 11; ++i) {
        char keyName[32];
        snprintf(keyName, sizeof(keyName), "Versus.%s", armorNames[i]);
        pINI->WriteDouble(sectionName, keyName, Verses[i]);
    }

    return true;
}

void WarheadTypeClass::ParseDebris(const char* debrisStr) {
    if (!debrisStr) return;
    char temp[256];
    int32 i = 0;
    while (debrisStr[i] && i < 255) { temp[i] = debrisStr[i]; ++i; }
    temp[i] = '\0';

    DebrisCount = 0;
    DebrisMaximumsCount = 0;
    char* token = strtok(temp, ",");
    while (token && DebrisCount < 10) {
        while (*token == ' ' || *token == '\t') ++token;
        if (*token) {
            TechnoTypeClass* pType = static_cast<TechnoTypeClass*>(TechnoTypeClass::Find(token));
            if (pType) {
                ++DebrisCount;
            }
        }
        token = strtok(nullptr, ",");
    }
}

void WarheadTypeClass::ParseSplashList(const char* splashStr) {
    if (!splashStr) return;
    char temp[256];
    int32 i = 0;
    while (splashStr[i] && i < 255) { temp[i] = splashStr[i]; ++i; }
    temp[i] = '\0';

    AnimListCount = 0;
    char* token = strtok(temp, ",");
    while (token && AnimListCount < MAX_ANIM_LIST) {
        while (*token == ' ' || *token == '\t') ++token;
        if (*token) {
            AnimTypeClass* pAnim = static_cast<AnimTypeClass*>(AnimTypeClass::Find(token));
            if (pAnim) {
                AnimList[AnimListCount++] = pAnim;
            }
        }
        token = strtok(nullptr, ",");
    }
}

float WarheadTypeClass::GetVersus(int32 armorType) const {
    if (armorType < 0 || armorType > 10) return 1.0f;
    return Verses[armorType];
}

float WarheadTypeClass::GetDamageMultiplier(int32 armorType) const {
    return GetVersus(armorType);
}

float WarheadTypeClass::GetProneDamage() const {
    return ProneDamage;
}

int32 WarheadTypeClass::GetInfDeath() const {
    return InfDeath;
}

bool WarheadTypeClass::IsWallDestroyerWeapon() const {
    return IsWallDestroyer || IsWallAbsoluteDestroyer;
}

bool WarheadTypeClass::IsWoodDestroyerWeapon() const {
    return IsWoodDestroyer;
}

bool WarheadTypeClass::IsTiberiumDestroyerWeapon() const {
    return IsTiberiumDestroyer;
}

bool WarheadTypeClass::IsOreDestroyerWeapon() const {
    return IsOreDestroyer;
}

bool WarheadTypeClass::IsSparkyWeapon() const {
    return IsSparky;
}

bool WarheadTypeClass::IsFireWeapon() const {
    return IsFire;
}

bool WarheadTypeClass::IsSmokeWeapon() const {
    return IsSmoke;
}

bool WarheadTypeClass::IsGasWeapon() const {
    return IsGas;
}

bool WarheadTypeClass::IsRadiationWeapon() const {
    return IsRadiation;
}

bool WarheadTypeClass::IsSonicWeapon() const {
    return IsSonic;
}

bool WarheadTypeClass::IsPsychicWeapon() const {
    return IsPsychic;
}

bool WarheadTypeClass::IsMechanicalWeapon() const {
    return IsMechanical;
}

bool WarheadTypeClass::HasBullets() const {
    return Bullets;
}

bool WarheadTypeClass::IsTemporal() const {
    return Temporal;
}

bool WarheadTypeClass::IsParasite() const {
    return Parasite;
}

bool WarheadTypeClass::IsBright() const {
    return Bright;
}

bool WarheadTypeClass::PenetratesBunkerWeapon() const {
    return PenetratesBunker;
}

float WarheadTypeClass::GetCellSpread() const {
    return CellSpread;
}

float WarheadTypeClass::GetPercentAtMax() const {
    return PercentAtMax;
}

ParticleTypeClass* WarheadTypeClass::GetParticle() const {
    return Particle;
}

AnimTypeClass* WarheadTypeClass::GetAnim() const {
    return Anim;
}

AnimTypeClass* WarheadTypeClass::GetRandomSplashAnim() const {
    if (AnimListCount <= 0) return nullptr;
    if (AnimListCount == 1) return AnimList[0];
    int32 index = Game::CurrentFrame % AnimListCount;
    return AnimList[index];
}

bool WarheadTypeClass::IsAttachedParticleWeapon() const {
    return IsAttachedParticle;
}

int32 WarheadTypeClass::GetDebrisCount() const {
    return DebrisCount;
}

void WarheadTypeClass::SetVersus(int32 armorType, float value) {
    if (armorType >= 0 && armorType < 11) {
        Verses[armorType] = value;
    }
}

// ============================================================================
// File-local helper functions
//
//  These provide warhead analysis, damage calculation utilities, debris
//  management, and special-effect flag queries used by the combat system
//  and the map editor.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// Armor type names - maps armor index to its string name.
// --------------------------------------------------------------------------
const char* const g_ArmorNames[11] = {
    "none",      // 0
    "flak",      // 1
    "plate",     // 2
    "light",     // 3
    "medium",    // 4
    "heavy",     // 5
    "wood",      // 6
    "steel",     // 7
    "concrete",  // 8
    "special_1", // 9
    "special_2"  // 10
};

// --------------------------------------------------------------------------
// GetArmorName - returns the string name for the given armor index.
// --------------------------------------------------------------------------
const char* GetArmorName(int32 armorType) noexcept
{
    if (armorType < 0 || armorType > 10)
        return "unknown";
    return g_ArmorNames[armorType];
}

// --------------------------------------------------------------------------
// GetArmorTypeFromString - parses an armor name into its index.  Returns
// -1 if the name is not recognized.
// --------------------------------------------------------------------------
int32 GetArmorTypeFromString(const char* pName) noexcept
{
    if (!pName || !pName[0])
        return -1;
    for (int32 i = 0; i < 11; ++i)
    {
        if (_strcmpi(g_ArmorNames[i], pName) == 0)
            return i;
    }
    return -1;
}

// --------------------------------------------------------------------------
// ComputeDamage - calculates the actual damage dealt to a target with the
// given armor type.  The base damage is multiplied by the warhead's versus
// multiplier for that armor type.
// --------------------------------------------------------------------------
int32 ComputeDamage(const WarheadTypeClass* pWarhead, int32 baseDamage,
                    int32 armorType) noexcept
{
    if (!pWarhead)
        return baseDamage;
    float multiplier = pWarhead->GetVersus(armorType);
    float result = static_cast<float>(baseDamage) * multiplier;
    return static_cast<int32>(result + 0.5f);
}

// --------------------------------------------------------------------------
// ComputeDamageWithDistance - calculates damage at a given distance from
// the explosion center, taking CellSpread and PercentAtMax into account.
// If the warhead has no CellSpread, the damage is uniform regardless of
// distance.
// --------------------------------------------------------------------------
int32 ComputeDamageWithDistance(const WarheadTypeClass* pWarhead,
                                int32 baseDamage, int32 armorType,
                                float distance) noexcept
{
    if (!pWarhead)
        return baseDamage;

    float multiplier = pWarhead->GetVersus(armorType);

    // If the warhead has area-of-effect, scale the damage by distance.
    if (pWarhead->CellSpread > 0.0f)
    {
        // At distance 0, full damage.  At distance >= CellSpread, damage is
        // scaled by PercentAtMax.
        float t = distance / pWarhead->CellSpread;
        if (t > 1.0f) t = 1.0f;
        if (t < 0.0f) t = 0.0f;
        // Linear interpolation between 1.0 and PercentAtMax.
        float distMultiplier = 1.0f + (pWarhead->PercentAtMax - 1.0f) * t;
        multiplier *= distMultiplier;
    }

    float result = static_cast<float>(baseDamage) * multiplier;
    return static_cast<int32>(result + 0.5f);
}

// --------------------------------------------------------------------------
// IsAreaEffect - returns true if the warhead has a non-zero CellSpread,
// meaning it deals area-of-effect damage.
// --------------------------------------------------------------------------
bool IsAreaEffect(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return false;
    return pWarhead->CellSpread > 0.0f;
}

// --------------------------------------------------------------------------
// IsLethal - returns true if the warhead's versus multiplier for the given
// armor type is greater than zero (i.e., the warhead can damage that armor).
// --------------------------------------------------------------------------
bool IsLethal(const WarheadTypeClass* pWarhead, int32 armorType) noexcept
{
    if (!pWarhead)
        return true;
    return pWarhead->GetVersus(armorType) > 0.0f;
}

// --------------------------------------------------------------------------
// IsImmune - returns true if the warhead's versus multiplier for the given
// armor type is zero (i.e., the armor is completely immune to this warhead).
// --------------------------------------------------------------------------
bool IsImmune(const WarheadTypeClass* pWarhead, int32 armorType) noexcept
{
    if (!pWarhead)
        return false;
    return pWarhead->GetVersus(armorType) <= 0.0f;
}

// --------------------------------------------------------------------------
// IsReducedDamage - returns true if the versus multiplier is less than 1.0
// but greater than 0 (partial resistance).
// --------------------------------------------------------------------------
bool IsReducedDamage(const WarheadTypeClass* pWarhead, int32 armorType) noexcept
{
    if (!pWarhead)
        return false;
    float v = pWarhead->GetVersus(armorType);
    return (v > 0.0f && v < 1.0f);
}

// --------------------------------------------------------------------------
// IsEnhancedDamage - returns true if the versus multiplier is greater than
// 1.0 (bonus damage).
// --------------------------------------------------------------------------
bool IsEnhancedDamage(const WarheadTypeClass* pWarhead, int32 armorType) noexcept
{
    if (!pWarhead)
        return false;
    return pWarhead->GetVersus(armorType) > 1.0f;
}

// --------------------------------------------------------------------------
// GetMinVersus - returns the lowest versus multiplier across all armor types.
// --------------------------------------------------------------------------
float GetMinVersus(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return 1.0f;
    float minVal = pWarhead->Verses[0];
    for (int32 i = 1; i < 11; ++i)
    {
        if (pWarhead->Verses[i] < minVal)
            minVal = pWarhead->Verses[i];
    }
    return minVal;
}

// --------------------------------------------------------------------------
// GetMaxVersus - returns the highest versus multiplier across all armor types.
// --------------------------------------------------------------------------
float GetMaxVersus(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return 1.0f;
    float maxVal = pWarhead->Verses[0];
    for (int32 i = 1; i < 11; ++i)
    {
        if (pWarhead->Verses[i] > maxVal)
            maxVal = pWarhead->Verses[i];
    }
    return maxVal;
}

// --------------------------------------------------------------------------
// GetAverageVersus - returns the arithmetic mean of all versus multipliers.
// --------------------------------------------------------------------------
float GetAverageVersus(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return 1.0f;
    float sum = 0.0f;
    for (int32 i = 0; i < 11; ++i)
        sum += pWarhead->Verses[i];
    return sum / 11.0f;
}

// --------------------------------------------------------------------------
// CountImmuneArmors - returns how many armor types are completely immune
// (versus == 0) to this warhead.
// --------------------------------------------------------------------------
int32 CountImmuneArmors(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return 0;
    int32 count = 0;
    for (int32 i = 0; i < 11; ++i)
    {
        if (pWarhead->Verses[i] <= 0.0f)
            ++count;
    }
    return count;
}

// --------------------------------------------------------------------------
// CountSpecialEffectFlags - returns how many special-effect boolean flags
// are set on the warhead.  Used by the editor to show a summary.
// --------------------------------------------------------------------------
int32 CountSpecialEffectFlags(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return 0;
    int32 count = 0;
    if (pWarhead->IsSparky)           ++count;
    if (pWarhead->IsFire)             ++count;
    if (pWarhead->IsSmoke)            ++count;
    if (pWarhead->IsGas)              ++count;
    if (pWarhead->IsLocomotor)        ++count;
    if (pWarhead->IsSonic)            ++count;
    if (pWarhead->IsRadiation)        ++count;
    if (pWarhead->IsPsychic)          ++count;
    if (pWarhead->IsMechanical)       ++count;
    if (pWarhead->Bullets)            ++count;
    if (pWarhead->Temporal)           ++count;
    if (pWarhead->Parasite)           ++count;
    if (pWarhead->Bright)             ++count;
    if (pWarhead->PenetratesBunker)   ++count;
    if (pWarhead->IsAttachedParticle) ++count;
    return count;
}

// --------------------------------------------------------------------------
// HasSpecialEffect - returns true if the warhead has any special-effect
// flag set.
// --------------------------------------------------------------------------
bool HasSpecialEffect(const WarheadTypeClass* pWarhead) noexcept
{
    return CountSpecialEffectFlags(pWarhead) > 0;
}

// --------------------------------------------------------------------------
// HasDebris - returns true if the warhead has any debris types configured.
// --------------------------------------------------------------------------
bool HasDebris(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return false;
    return pWarhead->DebrisCount > 0;
}

// --------------------------------------------------------------------------
// HasSplashList - returns true if the warhead has any splash animations.
// --------------------------------------------------------------------------
bool HasSplashList(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return false;
    return pWarhead->AnimListCount > 0;
}

// --------------------------------------------------------------------------
// HasAnim - returns true if the warhead has a primary impact animation.
// --------------------------------------------------------------------------
bool HasAnim(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return false;
    return pWarhead->Anim != nullptr;
}

// --------------------------------------------------------------------------
// HasParticle - returns true if the warhead has an attached particle type.
// --------------------------------------------------------------------------
bool HasParticle(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return false;
    return pWarhead->Particle != nullptr;
}

// --------------------------------------------------------------------------
// IsDestroyerWeapon - returns true if the warhead can destroy any kind of
// terrain feature (walls, wood, tiberium, ore).
// --------------------------------------------------------------------------
bool IsDestroyerWeapon(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return false;
    return pWarhead->IsWallDestroyer ||
           pWarhead->IsWallAbsoluteDestroyer ||
           pWarhead->IsWoodDestroyer ||
           pWarhead->IsTiberiumDestroyer ||
           pWarhead->IsOreDestroyer;
}

// --------------------------------------------------------------------------
// GetDestroyerTypes - returns a bitmask of which terrain types the warhead
// can destroy.  Bit 0 = walls, bit 1 = wood, bit 2 = tiberium, bit 3 = ore.
// --------------------------------------------------------------------------
int32 GetDestroyerTypes(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return 0;
    int32 mask = 0;
    if (pWarhead->IsWallDestroyer || pWarhead->IsWallAbsoluteDestroyer)
        mask |= 0x01;
    if (pWarhead->IsWoodDestroyer)
        mask |= 0x02;
    if (pWarhead->IsTiberiumDestroyer)
        mask |= 0x04;
    if (pWarhead->IsOreDestroyer)
        mask |= 0x08;
    return mask;
}

// --------------------------------------------------------------------------
// GetEffectSummary - writes a compact human-readable summary of the
// warhead's properties into the supplied buffer.
// --------------------------------------------------------------------------
void GetEffectSummary(const WarheadTypeClass* pWarhead, char* pBuffer,
                      int32 nBufferSize) noexcept
{
    if (!pWarhead || !pBuffer || nBufferSize <= 0)
        return;

    int32 pos = 0;
    auto Append = [&](const char* pStr) {
        if (!pStr) return;
        while (*pStr && pos < nBufferSize - 2)
        {
            pBuffer[pos] = *pStr;
            ++pos; ++pStr;
        }
        if (pos < nBufferSize - 2)
        {
            pBuffer[pos] = ',';
            ++pos;
            pBuffer[pos] = ' ';
            ++pos;
        }
    };

    pBuffer[0] = '\0';

    if (pWarhead->IsFire)        Append("Fire");
    if (pWarhead->IsSmoke)       Append("Smoke");
    if (pWarhead->IsGas)         Append("Gas");
    if (pWarhead->IsRadiation)   Append("Radiation");
    if (pWarhead->IsSonic)       Append("Sonic");
    if (pWarhead->IsPsychic)     Append("Psychic");
    if (pWarhead->IsMechanical)  Append("Mechanical");
    if (pWarhead->Temporal)      Append("Temporal");
    if (pWarhead->Parasite)      Append("Parasite");
    if (pWarhead->IsSparky)      Append("Sparky");
    if (pWarhead->Bright)        Append("Bright");
    if (pWarhead->IsLocomotor)   Append("Locomotor");
    if (pWarhead->PenetratesBunker) Append("PenetratesBunker");

    // Trim trailing ", " if present.
    if (pos >= 2 && pBuffer[pos - 2] == ',' && pBuffer[pos - 1] == ' ')
    {
        pos -= 2;
        pBuffer[pos] = '\0';
    }

    if (pos == 0)
    {
        const char* none = "None";
        int32 i = 0;
        while (none[i] && pos < nBufferSize - 1)
        {
            pBuffer[pos] = none[i];
            ++pos; ++i;
        }
        pBuffer[pos] = '\0';
    }
}

// --------------------------------------------------------------------------
// CompareWarheads - returns true if two warheads have identical versus
// multipliers and special-effect flags.  Used for deduplication.
// --------------------------------------------------------------------------
bool CompareWarheads(const WarheadTypeClass* pA,
                     const WarheadTypeClass* pB) noexcept
{
    if (!pA || !pB)
        return false;
    if (pA == pB)
        return true;

    // Compare versus multipliers.
    for (int32 i = 0; i < 11; ++i)
    {
        if (pA->Verses[i] != pB->Verses[i])
            return false;
    }

    // Compare special-effect flags.
    if (pA->IsSparky != pB->IsSparky) return false;
    if (pA->IsFire != pB->IsFire) return false;
    if (pA->IsSmoke != pB->IsSmoke) return false;
    if (pA->IsGas != pB->IsGas) return false;
    if (pA->IsSonic != pB->IsSonic) return false;
    if (pA->IsRadiation != pB->IsRadiation) return false;
    if (pA->IsPsychic != pB->IsPsychic) return false;
    if (pA->Temporal != pB->Temporal) return false;
    if (pA->Parasite != pB->Parasite) return false;
    if (pA->CellSpread != pB->CellSpread) return false;
    if (pA->PercentAtMax != pB->PercentAtMax) return false;

    return true;
}

// --------------------------------------------------------------------------
// FindDuplicateWarhead - searches the array for a warhead with identical
// properties.  Returns the duplicate or nullptr.
// --------------------------------------------------------------------------
WarheadTypeClass* FindDuplicateWarhead(const WarheadTypeClass* pWarhead) noexcept
{
    if (!WarheadTypeClass::Array || !pWarhead)
        return nullptr;
    for (int32 i = 0; i < WarheadTypeClass::Array->Count; ++i)
    {
        WarheadTypeClass* pOther = WarheadTypeClass::Array->GetItem(i);
        if (pOther == pWarhead)
            continue;
        if (CompareWarheads(pWarhead, pOther))
            return pOther;
    }
    return nullptr;
}

// --------------------------------------------------------------------------
// CountWarheadsByEffect - counts how many loaded warheads have the given
// special-effect flag set.
// --------------------------------------------------------------------------
int32 CountWarheadsByEffect(bool WarheadTypeClass::* pFlag) noexcept
{
    if (!WarheadTypeClass::Array || !pFlag)
        return 0;
    int32 count = 0;
    for (int32 i = 0; i < WarheadTypeClass::Array->Count; ++i)
    {
        WarheadTypeClass* pWH = WarheadTypeClass::Array->GetItem(i);
        if (pWH && (pWH->*pFlag))
            ++count;
    }
    return count;
}

// --------------------------------------------------------------------------
// CountFireWarheads - counts how many warheads have the IsFire flag set.
// --------------------------------------------------------------------------
int32 CountFireWarheads() noexcept
{
    return CountWarheadsByEffect(&WarheadTypeClass::IsFire);
}

// --------------------------------------------------------------------------
// CountRadiationWarheads - counts how many warheads have the IsRadiation
// flag set.
// --------------------------------------------------------------------------
int32 CountRadiationWarheads() noexcept
{
    return CountWarheadsByEffect(&WarheadTypeClass::IsRadiation);
}

// --------------------------------------------------------------------------
// CountPsychicWarheads - counts how many warheads have the IsPsychic flag.
// --------------------------------------------------------------------------
int32 CountPsychicWarheads() noexcept
{
    return CountWarheadsByEffect(&WarheadTypeClass::IsPsychic);
}

// --------------------------------------------------------------------------
// CountTemporalWarheads - counts how many warheads have the Temporal flag.
// --------------------------------------------------------------------------
int32 CountTemporalWarheads() noexcept
{
    return CountWarheadsByEffect(&WarheadTypeClass::Temporal);
}

// --------------------------------------------------------------------------
// CountAreaEffectWarheads - counts how many warheads have CellSpread > 0.
// --------------------------------------------------------------------------
int32 CountAreaEffectWarheads() noexcept
{
    if (!WarheadTypeClass::Array)
        return 0;
    int32 count = 0;
    for (int32 i = 0; i < WarheadTypeClass::Array->Count; ++i)
    {
        WarheadTypeClass* pWH = WarheadTypeClass::Array->GetItem(i);
        if (pWH && pWH->CellSpread > 0.0f)
            ++count;
    }
    return count;
}

// --------------------------------------------------------------------------
// FindBestWarheadForArmor - searches the array for the warhead with the
// highest versus multiplier against the given armor type.  Returns nullptr
// if no warheads are loaded.
// --------------------------------------------------------------------------
WarheadTypeClass* FindBestWarheadForArmor(int32 armorType) noexcept
{
    if (!WarheadTypeClass::Array || armorType < 0 || armorType > 10)
        return nullptr;
    WarheadTypeClass* pBest = nullptr;
    float bestVersus = -1.0f;
    for (int32 i = 0; i < WarheadTypeClass::Array->Count; ++i)
    {
        WarheadTypeClass* pWH = WarheadTypeClass::Array->GetItem(i);
        if (!pWH)
            continue;
        float v = pWH->Verses[armorType];
        if (v > bestVersus)
        {
            bestVersus = v;
            pBest = pWH;
        }
    }
    return pBest;
}

// --------------------------------------------------------------------------
// ParseVersesString - parses a comma-separated list of 11 float values
// into the warhead's Verses array.  Returns true on success.
// --------------------------------------------------------------------------
bool ParseVersesString(WarheadTypeClass* pWarhead, const char* pStr) noexcept
{
    if (!pWarhead || !pStr)
        return false;

    char temp[256];
    int32 i = 0;
    while (pStr[i] && i < 255)
    {
        temp[i] = pStr[i];
        ++i;
    }
    temp[i] = '\0';

    int32 idx = 0;
    char* token = strtok(temp, ",");
    while (token && idx < 11)
    {
        // Skip leading whitespace.
        while (*token == ' ' || *token == '\t')
            ++token;
        if (*token)
        {
            double val = atof(token);
            pWarhead->Verses[idx] = static_cast<float>(val);
            ++idx;
        }
        token = strtok(nullptr, ",");
    }

    // Fill remaining entries with 1.0 if fewer than 11 were supplied.
    while (idx < 11)
    {
        pWarhead->Verses[idx] = 1.0f;
        ++idx;
    }
    return true;
}

// --------------------------------------------------------------------------
// FormatVersesString - writes the warhead's 11 versus multipliers as a
// comma-separated string into the supplied buffer.
// --------------------------------------------------------------------------
void FormatVersesString(const WarheadTypeClass* pWarhead, char* pBuffer,
                        int32 nBufferSize) noexcept
{
    if (!pWarhead || !pBuffer || nBufferSize <= 0)
        return;

    pBuffer[0] = '\0';
    int32 pos = 0;
    for (int32 i = 0; i < 11; ++i)
    {
        char numBuf[32];
        snprintf(numBuf, sizeof(numBuf), "%.2f", pWarhead->Verses[i]);
        int32 j = 0;
        while (numBuf[j] && pos < nBufferSize - 2)
        {
            pBuffer[pos] = numBuf[j];
            ++pos; ++j;
        }
        if (i < 10 && pos < nBufferSize - 2)
        {
            pBuffer[pos] = ',';
            ++pos;
            pBuffer[pos] = ' ';
            ++pos;
        }
    }
    pBuffer[pos] = '\0';
}

// --------------------------------------------------------------------------
// GetDebrisTypeList - writes the names of all debris types into the
// supplied buffer as a comma-separated string.
// --------------------------------------------------------------------------
void GetDebrisTypeList(const WarheadTypeClass* pWarhead, char* pBuffer,
                       int32 nBufferSize) noexcept
{
    if (!pWarhead || !pBuffer || nBufferSize <= 0)
        return;
    pBuffer[0] = '\0';
    if (!pWarhead->DebrisTypes || pWarhead->DebrisCount <= 0)
        return;

    int32 pos = 0;
    for (int32 i = 0; i < pWarhead->DebrisCount && pos < nBufferSize - 2; ++i)
    {
        TechnoTypeClass* pType = pWarhead->DebrisTypes[i];
        if (!pType)
            continue;
        const char* pID = pType->get_ID();
        if (!pID)
            continue;
        int32 j = 0;
        while (pID[j] && pos < nBufferSize - 2)
        {
            pBuffer[pos] = pID[j];
            ++pos; ++j;
        }
        if (i < pWarhead->DebrisCount - 1 && pos < nBufferSize - 2)
        {
            pBuffer[pos] = ',';
            ++pos;
            pBuffer[pos] = ' ';
            ++pos;
        }
    }
    pBuffer[pos] = '\0';
}

// --------------------------------------------------------------------------
// GetSplashAnimList - writes the names of all splash animations into the
// supplied buffer as a comma-separated string.
// --------------------------------------------------------------------------
void GetSplashAnimList(const WarheadTypeClass* pWarhead, char* pBuffer,
                       int32 nBufferSize) noexcept
{
    if (!pWarhead || !pBuffer || nBufferSize <= 0)
        return;
    pBuffer[0] = '\0';
    if (pWarhead->AnimListCount <= 0)
        return;

    int32 pos = 0;
    for (int32 i = 0; i < pWarhead->AnimListCount && pos < nBufferSize - 2; ++i)
    {
        AnimTypeClass* pAnim = pWarhead->AnimList[i];
        if (!pAnim)
            continue;
        const char* pID = pAnim->get_ID();
        if (!pID)
            continue;
        int32 j = 0;
        while (pID[j] && pos < nBufferSize - 2)
        {
            pBuffer[pos] = pID[j];
            ++pos; ++j;
        }
        if (i < pWarhead->AnimListCount - 1 && pos < nBufferSize - 2)
        {
            pBuffer[pos] = ',';
            ++pos;
            pBuffer[pos] = ' ';
            ++pos;
        }
    }
    pBuffer[pos] = '\0';
}

// --------------------------------------------------------------------------
// ValidateWarhead - checks that the warhead's configuration is internally
// consistent.  Returns true if valid.
// --------------------------------------------------------------------------
bool ValidateWarhead(const WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return false;

    // Verses must be in [0, very large].
    for (int32 i = 0; i < 11; ++i)
    {
        if (pWarhead->Verses[i] < 0.0f)
            return false;
    }

    // CellSpread must be non-negative.
    if (pWarhead->CellSpread < 0.0f)
        return false;

    // PercentAtMax must be in [0, 1].
    if (pWarhead->PercentAtMax < 0.0f || pWarhead->PercentAtMax > 1.0f)
        return false;

    // ProneDamage must be non-negative.
    if (pWarhead->ProneDamage < 0.0f)
        return false;

    // DebrisCount must match the allocated array.
    if (pWarhead->DebrisCount < 0)
        return false;

    // AnimListCount must not exceed the maximum.
    if (pWarhead->AnimListCount < 0 || pWarhead->AnimListCount > MAX_ANIM_LIST)
        return false;

    return true;
}

// --------------------------------------------------------------------------
// CountInvalidWarheads - counts how many loaded warheads fail validation.
// --------------------------------------------------------------------------
int32 CountInvalidWarheads() noexcept
{
    if (!WarheadTypeClass::Array)
        return 0;
    int32 count = 0;
    for (int32 i = 0; i < WarheadTypeClass::Array->Count; ++i)
    {
        WarheadTypeClass* pWH = WarheadTypeClass::Array->GetItem(i);
        if (pWH && !ValidateWarhead(pWH))
            ++count;
    }
    return count;
}

// --------------------------------------------------------------------------
// ResetWarheadToDefaults - resets all warhead fields to their default
// values.  Used when creating a new warhead from scratch.
// --------------------------------------------------------------------------
void ResetWarheadToDefaults(WarheadTypeClass* pWarhead) noexcept
{
    if (!pWarhead)
        return;

    pWarhead->ArrayIndex = -1;
    pWarhead->IsWallDestroyer = false;
    pWarhead->IsWoodDestroyer = false;
    pWarhead->IsWallAbsoluteDestroyer = false;
    pWarhead->IsTiberiumDestroyer = false;
    pWarhead->IsOreDestroyer = false;
    pWarhead->ProneDamage = 1.0f;
    pWarhead->InfDeath = 0;
    pWarhead->CellSpread = 0.0f;
    pWarhead->PercentAtMax = 1.0f;
    pWarhead->IsSparky = false;
    pWarhead->IsFire = false;
    pWarhead->IsSmoke = false;
    pWarhead->IsGas = false;
    pWarhead->IsLocomotor = false;
    pWarhead->IsSonic = false;
    pWarhead->IsRadiation = false;
    pWarhead->IsPsychic = false;
    pWarhead->IsMechanical = false;
    pWarhead->Bullets = false;
    pWarhead->Particle = nullptr;
    pWarhead->Anim = nullptr;
    pWarhead->SplashList = nullptr;
    pWarhead->IsAttachedParticle = false;
    pWarhead->Temporal = false;
    pWarhead->Parasite = false;
    pWarhead->Bright = false;
    pWarhead->PenetratesBunker = false;
    pWarhead->AnimListCount = 0;
    pWarhead->DebrisCount = 0;
    pWarhead->DebrisMaximumsCount = 0;

    for (int32 i = 0; i < 11; ++i)
        pWarhead->Verses[i] = 1.0f;

    for (int32 i = 0; i < MAX_ANIM_LIST; ++i)
        pWarhead->AnimList[i] = nullptr;
}

} // end anonymous namespace