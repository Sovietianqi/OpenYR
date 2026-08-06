#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Abstract/AbstractClass.h"

enum class AlphaBlendMode : int32 {
    Normal = 0,
    Additive = 1,
    Subtractive = 2,
    Multiply = 3,
    Screen = 4
};

class BlitterClass;
class DSurface;

class AlphaShapeClass {
public:
    AlphaShapeClass();
    ~AlphaShapeClass();

    bool LoadShape(const uint8* data, int32 width, int32 height, int32 frames);
    void UnloadShape();
    bool LoadShapeFromFile(const char* filename);

    void Render(BlitterClass* blitter, DSurface* surface);
    void RenderWithRotation(BlitterClass* blitter, DSurface* surface, float angle);
    void RenderAtPosition(BlitterClass* blitter, DSurface* surface, int32 x, int32 y);

    void SetAlpha(int32 alpha);
    void SetTranslucency(float value);
    void SetBlendMode(AlphaBlendMode mode);
    void SetPosition(int32 x, int32 y, int32 z);
    void SetScale(float scaleX, float scaleY);
    void SetRotation(float angle);
    void SetFrame(int32 frame);

    void UpdateAnimation();
    void PlayAnimation(bool loop);
    void StopAnimation();
    void PauseAnimation();
    void ResumeAnimation();
    void SetAnimationSpeed(int32 speed);

    void SetShadow(bool enabled, int32 alpha, int32 offsetX, int32 offsetY);
    void SetVisible(bool visible);
    void SetColorTint(uint32 color);
    void ApplyColorTint(DSurface* surface);
    void SetFlash(int32 duration, uint32 color);
    int32 ApplyFlashEffect(int32 alpha);
    void StartFadeIn(int32 duration);
    void StartFadeOut(int32 duration);
    void UpdateFade();
    void Update();

    void GetPosition(int32& x, int32& y, int32& z) const;
    int32 GetAlpha() const;
    float GetRotation() const;
    int32 GetCurrentFrame() const;
    int32 GetFrameCount() const;
    int32 GetWidth() const;
    int32 GetHeight() const;
    bool IsPlaying() const;
    bool IsVisibleShape() const;
    void SetCenter(int32 cx, int32 cy);
    void SetZValue(int32 z);
    int32 GetZValue() const;

    int32 ShapeWidth;
    int32 ShapeHeight;
    uint8* ShapeData;
    int32 ShapeDataSize;
    int32 AlphaLevel;
    float Translucency;
    AlphaBlendMode BlendMode;
    int32 PositionX;
    int32 PositionY;
    int32 PositionZ;
    float ScaleX;
    float ScaleY;
    float Rotation;
    int32 CurrentFrame;
    int32 FrameCount;
    int32 FrameWidth;
    int32 FrameHeight;
    int32 AnimationSpeed;
    int32 AnimationTimer;
    bool AnimationLooping;
    bool AnimationPlaying;
    bool AnimationPaused;
    bool IsShadow;
    int32 ShadowAlpha;
    int32 ShadowOffsetX;
    int32 ShadowOffsetY;
    bool IsVisible;
    bool IsDirty;
    int32 RenderFlags;
    int32 CenterX;
    int32 CenterY;
    int32 ZValue;
    uint32 ColorTint;
    int32 FlashTimer;
    int32 FlashDuration;
    uint32 FlashColor;
    int32 FadeInTimer;
    int32 FadeInDuration;
    int32 FadeOutTimer;
    int32 FadeOutDuration;
    int32 OriginalAlpha;
    int32 TargetAlpha;
    int32 CurrentAlpha;
};