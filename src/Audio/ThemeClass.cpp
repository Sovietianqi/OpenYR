#include "ThemeClass.h"
#include "../IO/CCFileClass.h"
#include "../IO/FileSystem.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>

// ============================================================
// ThemeClass
// ============================================================

static ThemeClass* g_ThemeInstance = nullptr;

ThemeClass::ThemeClass()
    : IsPlaying(false), IsPaused(false), Volume(200), CurrentTrack(-1)
    , PlaylistCount(0), ShuffleEnabled(false), RepeatEnabled(true)
    , BattleMode(false), BattleTransitionDuration(120), TransitionTimer(0)
    , IsCrossfading(false), CrossfadeTimer(0), CrossfadeDuration(60)
    , OldTrackVolume(0), NewTrackVolume(0), FadeInTimer(0), FadeInDuration(0)
    , FadeOutTimer(0), FadeOutDuration(0), IsMuted(false), SavedVolume(200)
    , PeaceTrackIndex(-1), BattleTrackIndex(-1), CurrentThemeState(ThemeState::Peace)
    , TrackPosition(0), TrackDuration(0), CDTrackIndex(-1), IsCDAudio(false)
    , MP3Latency(0), MP3BufferSize(16384), AudioBuffer(nullptr), AudioBufferSize(0) {
    for (int32 i = 0; i < MAX_THEMES; ++i) {
        ThemeNames[i] = nullptr;
        ThemeDurations[i] = 0;
        ThemeTypes[i] = ThemeType::Peace;
    }
}

ThemeClass::~ThemeClass() {
    Stop();
    ClearPlaylist();
}

ThemeClass* ThemeClass::GetInstance() {
    if (!g_ThemeInstance) {
        g_ThemeInstance = new ThemeClass();
    }
    return g_ThemeInstance;
}

bool ThemeClass::AddTheme(const char* name, int32 duration, ThemeType type) {
    if (!name || !name[0]) return false;
    if (PlaylistCount >= MAX_THEMES) return false;

    int32 nameLen = 0;
    while (name[nameLen] && nameLen < 63) ++nameLen;

    ThemeNames[PlaylistCount] = static_cast<char*>(std::malloc(nameLen + 1));
    if (!ThemeNames[PlaylistCount]) return false;

    std::memcpy(ThemeNames[PlaylistCount], name, nameLen);
    ThemeNames[PlaylistCount][nameLen] = '\0';
    ThemeDurations[PlaylistCount] = duration;
    ThemeTypes[PlaylistCount] = type;
    ++PlaylistCount;
    return true;
}

bool ThemeClass::RemoveTheme(int32 index) {
    if (index < 0 || index >= PlaylistCount) return false;

    if (ThemeNames[index]) {
        std::free(ThemeNames[index]);
        ThemeNames[index] = nullptr;
    }

    for (int32 i = index; i < PlaylistCount - 1; ++i) {
        ThemeNames[i] = ThemeNames[i + 1];
        ThemeDurations[i] = ThemeDurations[i + 1];
        ThemeTypes[i] = ThemeTypes[i + 1];
    }
    ThemeNames[PlaylistCount - 1] = nullptr;
    ThemeDurations[PlaylistCount - 1] = 0;
    --PlaylistCount;
    return true;
}

void ThemeClass::ClearPlaylist() {
    for (int32 i = 0; i < PlaylistCount; ++i) {
        if (ThemeNames[i]) {
            std::free(ThemeNames[i]);
            ThemeNames[i] = nullptr;
        }
    }
    PlaylistCount = 0;
}

void ThemeClass::Play() {
    if (PlaylistCount <= 0) return;

    if (IsPlaying) {
        Stop();
    }

    int32 selectedTrack = SelectTrack();
    if (selectedTrack < 0) return;

    if (IsCrossfading) {
        StopCrossfade();
    }

    CurrentTrack = selectedTrack;
    TrackPosition = 0;
    TrackDuration = ThemeDurations[selectedTrack];
    IsPlaying = true;
    IsPaused = false;
    BattleMode = false;
    CurrentThemeState = ThemeState::Peace;

    if (FadeInDuration > 0) {
        FadeInTimer = FadeInDuration;
        Volume = 0;
    } else {
        Volume = IsMuted ? 0 : SavedVolume;
    }

    LoadTrack(CurrentTrack);
}

void ThemeClass::PlayTrack(int32 index) {
    if (index < 0 || index >= PlaylistCount) return;
    if (IsPlaying) Stop();

    CurrentTrack = index;
    TrackPosition = 0;
    TrackDuration = ThemeDurations[index];
    IsPlaying = true;
    IsPaused = false;
    Volume = IsMuted ? 0 : SavedVolume;
    LoadTrack(index);
}

void ThemeClass::Stop() {
    IsPlaying = false;
    IsPaused = false;
    IsCrossfading = false;
    CurrentTrack = -1;
    TrackPosition = 0;
    BattleMode = false;
    CurrentThemeState = ThemeState::Peace;
    TransitionTimer = 0;
}

void ThemeClass::Pause() {
    if (IsPlaying) {
        IsPaused = true;
        IsPlaying = false;
    }
}

void ThemeClass::Resume() {
    if (IsPaused) {
        IsPaused = false;
        IsPlaying = true;
    }
}

void ThemeClass::Next() {
    if (PlaylistCount <= 0) return;

    int32 nextTrack = GetNextTrack();
    PlayTrack(nextTrack);
}

void ThemeClass::Previous() {
    if (PlaylistCount <= 0) return;

    int32 prevTrack = GetPreviousTrack();
    PlayTrack(prevTrack);
}

void ThemeClass::Update() {
    if (!IsPlaying || IsPaused) return;

    ++TrackPosition;

    if (TrackDuration > 0 && TrackPosition >= TrackDuration) {
        if (RepeatEnabled) {
            Next();
        } else {
            Stop();
        }
        return;
    }

    if (IsCrossfading) {
        UpdateCrossfade();
    }

    if (FadeInTimer > 0) {
        UpdateFadeIn();
    }

    if (FadeOutTimer > 0) {
        UpdateFadeOut();
    }

    if (BattleMode && TransitionTimer > 0) {
        --TransitionTimer;
        if (TransitionTimer <= 0) {
            SwitchToBattleTheme();
        }
    }
}

void ThemeClass::UpdateCrossfade() {
    ++CrossfadeTimer;
    float t = static_cast<float>(CrossfadeTimer) / static_cast<float>(CrossfadeDuration);
    if (t > 1.0f) t = 1.0f;

    OldTrackVolume = static_cast<int32>(SavedVolume * (1.0f - t));
    NewTrackVolume = static_cast<int32>(SavedVolume * t);

    if (CrossfadeTimer >= CrossfadeDuration) {
        StopCrossfade();
    }
}

void ThemeClass::UpdateFadeIn() {
    --FadeInTimer;
    float t = static_cast<float>(FadeInDuration - FadeInTimer) / static_cast<float>(FadeInDuration);
    if (t > 1.0f) t = 1.0f;
    Volume = static_cast<int32>(SavedVolume * t);
    if (Volume < 0) Volume = 0;
    if (FadeInTimer <= 0) {
        FadeInTimer = 0;
        Volume = SavedVolume;
    }
}

void ThemeClass::UpdateFadeOut() {
    --FadeOutTimer;
    float t = static_cast<float>(FadeOutTimer) / static_cast<float>(FadeOutDuration);
    if (t < 0.0f) t = 0.0f;
    Volume = static_cast<int32>(SavedVolume * t);
    if (Volume < 0) Volume = 0;
    if (FadeOutTimer <= 0) {
        Stop();
    }
}

int32 ThemeClass::SelectTrack() {
    int32 availableCount = 0;
    int32 availableIndices[MAX_THEMES];

    for (int32 i = 0; i < PlaylistCount; ++i) {
        if (ThemeTypes[i] == ThemeType::Peace || ThemeTypes[i] == ThemeType::Both) {
            availableIndices[availableCount++] = i;
        }
    }

    if (availableCount <= 0) return -1;

    if (ShuffleEnabled) {
        int32 randIdx = std::rand() % availableCount;
        return availableIndices[randIdx];
    }

    if (CurrentTrack >= 0) {
        return (CurrentTrack + 1) % PlaylistCount;
    }

    return availableIndices[0];
}

int32 ThemeClass::GetNextTrack() {
    if (PlaylistCount <= 0) return -1;

    int32 next = CurrentTrack + 1;
    if (next >= PlaylistCount) {
        if (RepeatEnabled) {
            next = 0;
        } else {
            return -1;
        }
    }
    return next;
}

int32 ThemeClass::GetPreviousTrack() {
    if (PlaylistCount <= 0) return -1;

    int32 prev = CurrentTrack - 1;
    if (prev < 0) {
        if (RepeatEnabled) {
            prev = PlaylistCount - 1;
        } else {
            prev = 0;
        }
    }
    return prev;
}

void ThemeClass::LoadTrack(int32 index) {
    if (index < 0 || index >= PlaylistCount) return;
    if (!ThemeNames[index]) return;

    const char* filename = ThemeNames[index];

    if (IsCDTrack(filename)) {
        CDTrackIndex = ParseCDTrack(filename);
        IsCDAudio = true;
    } else {
        IsCDAudio = false;
    }
}

bool ThemeClass::IsCDTrack(const char* filename) {
    if (!filename) return false;
    return filename[0] == 'C' && filename[1] == 'D' && filename[2] == ':';
}

int32 ThemeClass::ParseCDTrack(const char* filename) {
    if (!filename) return -1;
    int32 track = 0;
    int32 i = 3;
    while (filename[i] >= '0' && filename[i] <= '9') {
        track = track * 10 + (filename[i] - '0');
        ++i;
    }
    return track;
}

void ThemeClass::StartCrossfade(int32 duration) {
    if (IsCrossfading) return;

    CrossfadeDuration = duration;
    CrossfadeTimer = 0;
    OldTrackVolume = Volume;
    NewTrackVolume = 0;
    IsCrossfading = true;
}

void ThemeClass::StopCrossfade() {
    IsCrossfading = false;
    CrossfadeTimer = 0;
    OldTrackVolume = 0;
    NewTrackVolume = 0;
}

void ThemeClass::StartFadeIn(int32 duration) {
    FadeInDuration = duration;
    FadeInTimer = duration;
    Volume = 0;
}

void ThemeClass::StartFadeOut(int32 duration) {
    FadeOutDuration = duration;
    FadeOutTimer = duration;
}

void ThemeClass::SwitchToBattleTheme() {
    if (BattleMode) return;
    BattleMode = true;
    CurrentThemeState = ThemeState::Battle;

    int32 availableCount = 0;
    int32 availableIndices[MAX_THEMES];

    for (int32 i = 0; i < PlaylistCount; ++i) {
        if (ThemeTypes[i] == ThemeType::Battle || ThemeTypes[i] == ThemeType::Both) {
            availableIndices[availableCount++] = i;
        }
    }

    if (availableCount > 0) {
        int32 battleTrack = availableIndices[std::rand() % availableCount];
        if (battleTrack != CurrentTrack) {
            if (CrossfadeDuration > 0) {
                StartCrossfade(CrossfadeDuration);
            }
            BattleTrackIndex = battleTrack;
            PlayTrack(battleTrack);
        }
    }
}

void ThemeClass::SwitchToPeaceTheme() {
    if (!BattleMode) return;
    BattleMode = false;
    CurrentThemeState = ThemeState::Peace;

    int32 availableCount = 0;
    int32 availableIndices[MAX_THEMES];

    for (int32 i = 0; i < PlaylistCount; ++i) {
        if (ThemeTypes[i] == ThemeType::Peace || ThemeTypes[i] == ThemeType::Both) {
            availableIndices[availableCount++] = i;
        }
    }

    if (availableCount > 0) {
        int32 peaceTrack = availableIndices[std::rand() % availableCount];
        if (peaceTrack != CurrentTrack) {
            if (CrossfadeDuration > 0) {
                StartCrossfade(CrossfadeDuration);
            }
            PeaceTrackIndex = peaceTrack;
            PlayTrack(peaceTrack);
        }
    }
}

void ThemeClass::StartBattleTransition() {
    if (BattleMode) return;
    TransitionTimer = BattleTransitionDuration;
}

void ThemeClass::EndBattleTransition() {
    TransitionTimer = 0;
    SwitchToPeaceTheme();
}

void ThemeClass::SetVolume(int32 volume) {
    SavedVolume = volume;
    if (SavedVolume < 0) SavedVolume = 0;
    if (SavedVolume > 255) SavedVolume = 255;
    if (!IsMuted) {
        Volume = SavedVolume;
    }
}

void ThemeClass::SetCrossfadeDuration(int32 duration) {
    CrossfadeDuration = duration;
    if (CrossfadeDuration < 0) CrossfadeDuration = 0;
    if (CrossfadeDuration > 300) CrossfadeDuration = 300;
}

void ThemeClass::SetBattleTransitionDuration(int32 duration) {
    BattleTransitionDuration = duration;
    if (BattleTransitionDuration < 0) BattleTransitionDuration = 0;
    if (BattleTransitionDuration > 600) BattleTransitionDuration = 600;
}

void ThemeClass::SetShuffle(bool enabled) {
    ShuffleEnabled = enabled;
}

void ThemeClass::SetRepeat(bool enabled) {
    RepeatEnabled = enabled;
}

void ThemeClass::Mute() {
    if (!IsMuted) {
        SavedVolume = Volume;
        Volume = 0;
        IsMuted = true;
    }
}

void ThemeClass::Unmute() {
    if (IsMuted) {
        Volume = SavedVolume;
        IsMuted = false;
    }
}

void ThemeClass::ToggleMute() {
    if (IsMuted) {
        Unmute();
    } else {
        Mute();
    }
}

void ThemeClass::SetCDAudio(bool enabled) {
    IsCDAudio = enabled;
}

void ThemeClass::SetMP3BufferSize(int32 size) {
    MP3BufferSize = size;
    if (MP3BufferSize < 4096) MP3BufferSize = 4096;
    if (MP3BufferSize > 65536) MP3BufferSize = 65536;
}

int32 ThemeClass::GetVolume() const {
    return Volume;
}

int32 ThemeClass::GetCurrentTrack() const {
    return CurrentTrack;
}

int32 ThemeClass::GetTrackPosition() const {
    return TrackPosition;
}

int32 ThemeClass::GetTrackDuration() const {
    return TrackDuration;
}

int32 ThemeClass::GetPlaylistCount() const {
    return PlaylistCount;
}

const char* ThemeClass::GetThemeName(int32 index) const {
    if (index < 0 || index >= PlaylistCount) return nullptr;
    return ThemeNames[index];
}

ThemeType ThemeClass::GetThemeType(int32 index) const {
    if (index < 0 || index >= PlaylistCount) return ThemeType::Peace;
    return ThemeTypes[index];
}

bool ThemeClass::IsPlayingTheme() const {
    return IsPlaying;
}

bool ThemeClass::IsPausedTheme() const {
    return IsPaused;
}

bool ThemeClass::IsMutedTheme() const {
    return IsMuted;
}

bool ThemeClass::IsBattleMode() const {
    return BattleMode;
}

bool ThemeClass::IsShuffleEnabled() const {
    return ShuffleEnabled;
}

bool ThemeClass::IsRepeatEnabled() const {
    return RepeatEnabled;
}

bool ThemeClass::IsCDAudioEnabled() const {
    return IsCDAudio;
}

ThemeState ThemeClass::GetThemeState() const {
    return CurrentThemeState;
}

int32 ThemeClass::GetCDTrackIndex() const {
    return CDTrackIndex;
}

void ThemeClass::LoadThemeList(const char* listFilename) {
    if (!listFilename || !listFilename[0]) return;

    CCFileClass file(listFilename);
    if (!file.Exists()) return;

    int32 fileSize = file.Size();
    if (fileSize <= 0 || fileSize > 65536) return;

    char* buffer = static_cast<char*>(std::malloc(fileSize + 1));
    if (!buffer) return;

    if (!file.Read(buffer, fileSize)) {
        std::free(buffer);
        return;
    }
    buffer[fileSize] = '\0';

    ClearPlaylist();

    char lineBuffer[256];
    int32 linePos = 0;

    for (int32 i = 0; i < fileSize; ++i) {
        if (buffer[i] == '\n' || buffer[i] == '\r' || buffer[i] == '\0') {
            if (linePos > 0) {
                lineBuffer[linePos] = '\0';
                char name[128];
                int32 duration = 0;
                int32 themeType = 0;

                if (sscanf(lineBuffer, "%127[^,],%d,%d", name, &duration, &themeType) >= 2) {
                    ThemeType type = ThemeType::Peace;
                    if (themeType == 1) type = ThemeType::Battle;
                    else if (themeType == 2) type = ThemeType::Both;
                    AddTheme(name, duration, type);
                }
                linePos = 0;
            }
        } else {
            if (linePos < 255) {
                lineBuffer[linePos++] = buffer[i];
            }
        }
    }

    std::free(buffer);
}