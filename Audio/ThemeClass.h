#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

enum class ThemeType : int32 {
    Peace = 0,
    Battle = 1,
    Both = 2
};

enum class ThemeState : int32 {
    Peace = 0,
    Battle = 1,
    Transition = 2
};

static constexpr int32 MAX_THEMES = 128;

class ThemeClass {
public:
    ThemeClass();
    ~ThemeClass();

    static ThemeClass* GetInstance();

    bool AddTheme(const char* name, int32 duration, ThemeType type);
    bool RemoveTheme(int32 index);
    void ClearPlaylist();

    void Play();
    void PlayTrack(int32 index);
    void Stop();
    void Pause();
    void Resume();
    void Next();
    void Previous();
    void Update();

    void UpdateCrossfade();
    void UpdateFadeIn();
    void UpdateFadeOut();
    int32 SelectTrack();
    int32 GetNextTrack();
    int32 GetPreviousTrack();
    void LoadTrack(int32 index);
    bool IsCDTrack(const char* filename);
    int32 ParseCDTrack(const char* filename);

    void StartCrossfade(int32 duration);
    void StopCrossfade();
    void StartFadeIn(int32 duration);
    void StartFadeOut(int32 duration);
    void SwitchToBattleTheme();
    void SwitchToPeaceTheme();
    void StartBattleTransition();
    void EndBattleTransition();

    void SetVolume(int32 volume);
    void SetCrossfadeDuration(int32 duration);
    void SetBattleTransitionDuration(int32 duration);
    void SetShuffle(bool enabled);
    void SetRepeat(bool enabled);
    void Mute();
    void Unmute();
    void ToggleMute();
    void SetCDAudio(bool enabled);
    void SetMP3BufferSize(int32 size);

    int32 GetVolume() const;
    int32 GetCurrentTrack() const;
    int32 GetTrackPosition() const;
    int32 GetTrackDuration() const;
    int32 GetPlaylistCount() const;
    const char* GetThemeName(int32 index) const;
    ThemeType GetThemeType(int32 index) const;
    bool IsPlayingTheme() const;
    bool IsPausedTheme() const;
    bool IsMutedTheme() const;
    bool IsBattleMode() const;
    bool IsShuffleEnabled() const;
    bool IsRepeatEnabled() const;
    bool IsCDAudioEnabled() const;
    ThemeState GetThemeState() const;
    int32 GetCDTrackIndex() const;

    void LoadThemeList(const char* listFilename);

    bool IsPlaying;
    bool IsPaused;
    int32 Volume;
    int32 CurrentTrack;
    char* ThemeNames[MAX_THEMES];
    int32 ThemeDurations[MAX_THEMES];
    ThemeType ThemeTypes[MAX_THEMES];
    int32 PlaylistCount;
    bool ShuffleEnabled;
    bool RepeatEnabled;
    bool BattleMode;
    int32 BattleTransitionDuration;
    int32 TransitionTimer;
    bool IsCrossfading;
    int32 CrossfadeTimer;
    int32 CrossfadeDuration;
    int32 OldTrackVolume;
    int32 NewTrackVolume;
    int32 FadeInTimer;
    int32 FadeInDuration;
    int32 FadeOutTimer;
    int32 FadeOutDuration;
    bool IsMuted;
    int32 SavedVolume;
    int32 PeaceTrackIndex;
    int32 BattleTrackIndex;
    ThemeState CurrentThemeState;
    int32 TrackPosition;
    int32 TrackDuration;
    int32 CDTrackIndex;
    bool IsCDAudio;
    int32 MP3Latency;
    int32 MP3BufferSize;
    uint8* AudioBuffer;
    int32 AudioBufferSize;
};