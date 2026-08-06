#include "AlphaShapeClass.h"
#include "../Rendering/Blitter.h"
#include "../Rendering/Surface.h"
#include "../Math/Facing.h"

#include <cmath>
#include <cstring>

// ============================================================
// AlphaShapeClass
// ============================================================

AlphaShapeClass::AlphaShapeClass()
    : ShapeWidth(0), ShapeHeight(0), ShapeData(nullptr), ShapeDataSize(0)
    , AlphaLevel(128), Translucency(0.5f), BlendMode(AlphaBlendMode::Normal)
    , PositionX(0), PositionY(0), PositionZ(0)
    , ScaleX(1.0f), ScaleY(1.0f), Rotation(0.0f)
    , CurrentFrame(0), FrameCount(1), FrameWidth(0), FrameHeight(0)
    , AnimationSpeed(4), AnimationTimer(0), AnimationLooping(true)
    , AnimationPlaying(false), AnimationPaused(false)
    , IsShadow(false), ShadowAlpha(64), ShadowOffsetX(2), ShadowOffsetY(2)
    , IsVisible(true), IsDirty(true), RenderFlags(0)
    , CenterX(0), CenterY(0), ZValue(0), ColorTint(0xFFFFFFFF)
    , FlashTimer(0), FlashDuration(0), FlashColor(0xFFFFFFFF)
    , FadeInTimer(0), FadeInDuration(0), FadeOutTimer(0), FadeOutDuration(0)
    , OriginalAlpha(128), TargetAlpha(128), CurrentAlpha(128) {
}

AlphaShapeClass::~AlphaShapeClass() {
    UnloadShape();
}

bool AlphaShapeClass::LoadShape(const uint8* data, int32 width, int32 height, int32 frames) {
    if (!data || width <= 0 || height <= 0) return false;

    UnloadShape();
    ShapeWidth = width;
    ShapeHeight = height;
    FrameCount = frames > 0 ? frames : 1;
    FrameWidth = width;
    FrameHeight = height;

    ShapeDataSize = width * height * FrameCount;
    ShapeData = static_cast<uint8*>(std::malloc(ShapeDataSize));
    if (!ShapeData) return false;

    std::memcpy(ShapeData, data, ShapeDataSize);
    CurrentFrame = 0;
    CenterX = width / 2;
    CenterY = height / 2;
    IsDirty = true;
    return true;
}

void AlphaShapeClass::UnloadShape() {
    if (ShapeData) {
        std::free(ShapeData);
        ShapeData = nullptr;
    }
    ShapeDataSize = 0;
    ShapeWidth = 0;
    ShapeHeight = 0;
    FrameWidth = 0;
    FrameHeight = 0;
    FrameCount = 1;
    CurrentFrame = 0;
}

bool AlphaShapeClass::LoadShapeFromFile(const char* filename) {
    if (!filename || !filename[0]) return false;
    CCFileClass file(filename);
    if (!file.Exists()) return false;

    int32 fileSize = file.Size();
    if (fileSize <= 0) return false;

    uint8* fileData = static_cast<uint8*>(std::malloc(fileSize));
    if (!fileData) return false;

    if (!file.Read(fileData, fileSize)) {
        std::free(fileData);
        return false;
    }

    int32 width = static_cast<int32>(fileData[0]) | (static_cast<int32>(fileData[1]) << 8);
    int32 height = static_cast<int32>(fileData[2]) | (static_cast<int32>(fileData[3]) << 8);
    int32 frames = static_cast<int32>(fileData[4]) | (static_cast<int32>(fileData[5]) << 8);

    bool result = LoadShape(fileData + 6, width, height, frames);
    std::free(fileData);
    return result;
}

void AlphaShapeClass::Render(BlitterClass* blitter, DSurface* surface) {
    if (!IsVisible || !ShapeData || !blitter || !surface) return;
    if (AlphaLevel <= 0 && CurrentAlpha <= 0) return;

    int32 srcX = 0;
    int32 srcY = CurrentFrame * FrameHeight;
    int32 alpha = AlphaLevel;

    if (FlashTimer > 0) {
        alpha = ApplyFlashEffect(alpha);
    }

    if (FadeInTimer > 0) {
        float fadeProgress = static_cast<float>(FadeInDuration - FadeInTimer) / static_cast<float>(FadeInDuration);
        alpha = static_cast<int32>(alpha * fadeProgress);
    }

    if (FadeOutTimer > 0) {
        float fadeProgress = static_cast<float>(FadeOutTimer) / static_cast<float>(FadeOutDuration);
        alpha = static_cast<int32>(alpha * fadeProgress);
    }

    if (alpha <= 0) return;

    int32 destX = PositionX - static_cast<int32>(CenterX * ScaleX);
    int32 destY = PositionY - static_cast<int32>(CenterY * ScaleY);

    DSurface tempSurface(ShapeWidth, ShapeHeight);
    tempSurface.Allocate(ShapeWidth, ShapeHeight);

    // Copy the current frame's pixel data
    for (int32 y = 0; y < FrameHeight; ++y) {
        for (int32 x = 0; x < FrameWidth; ++x) {
            int32 pixelIdx = (srcY + y) * ShapeWidth + (srcX + x);
            if (pixelIdx < ShapeDataSize) {
                tempSurface.SetPixel(x, y, ShapeData[pixelIdx]);
            }
        }
    }

    // Apply color tint
    if (ColorTint != 0xFFFFFFFF) {
        ApplyColorTint(&tempSurface);
    }

    // Apply alpha blend - simplified: draw pixels directly to destination surface
    for (int32 y = 0; y < FrameHeight; ++y) {
        for (int32 x = 0; x < FrameWidth; ++x) {
            BYTE pixel = tempSurface.GetPixel(x, y);
            if (pixel != 0) {
                int32 dx = destX + x;
                int32 dy = destY + y;
                if (dx >= 0 && dx < surface->GetWidth() && dy >= 0 && dy < surface->GetHeight()) {
                    surface->SetPixelAlpha(dx, dy, 255, 255, 255, static_cast<uint8>(alpha));
                }
            }
        }
    }

    // Render shadow
    if (IsShadow) {
        int32 shadowX = destX + ShadowOffsetX;
        int32 shadowY = destY + ShadowOffsetY;
        for (int32 y = 0; y < FrameHeight; ++y) {
            for (int32 x = 0; x < FrameWidth; ++x) {
                BYTE pixel = tempSurface.GetPixel(x, y);
                if (pixel != 0) {
                    int32 dx = shadowX + x;
                    int32 dy = shadowY + y;
                    if (dx >= 0 && dx < surface->GetWidth() && dy >= 0 && dy < surface->GetHeight()) {
                        surface->SetPixelAlpha(dx, dy, 0, 0, 0, static_cast<uint8>(ShadowAlpha));
                    }
                }
            }
        }
    }

    tempSurface.Free();
    IsDirty = false;
}

void AlphaShapeClass::RenderWithRotation(BlitterClass* blitter, DSurface* surface, float angle) {
    if (!IsVisible || !ShapeData || !blitter || !surface) return;

    float savedRotation = Rotation;
    Rotation = angle;
    Render(blitter, surface);
    Rotation = savedRotation;
}

void AlphaShapeClass::RenderAtPosition(BlitterClass* blitter, DSurface* surface, int32 x, int32 y) {
    int32 savedX = PositionX;
    int32 savedY = PositionY;
    PositionX = x;
    PositionY = y;
    Render(blitter, surface);
    PositionX = savedX;
    PositionY = savedY;
}

void AlphaShapeClass::SetAlpha(int32 alpha) {
    AlphaLevel = alpha;
    if (AlphaLevel < 0) AlphaLevel = 0;
    if (AlphaLevel > 255) AlphaLevel = 255;
    OriginalAlpha = AlphaLevel;
    CurrentAlpha = AlphaLevel;
}

void AlphaShapeClass::SetTranslucency(float value) {
    Translucency = value;
    if (Translucency < 0.0f) Translucency = 0.0f;
    if (Translucency > 1.0f) Translucency = 1.0f;
    AlphaLevel = static_cast<int32>(255.0f * Translucency);
}

void AlphaShapeClass::SetBlendMode(AlphaBlendMode mode) {
    BlendMode = mode;
}

void AlphaShapeClass::SetPosition(int32 x, int32 y, int32 z) {
    PositionX = x;
    PositionY = y;
    PositionZ = z;
    IsDirty = true;
}

void AlphaShapeClass::SetScale(float scaleX, float scaleY) {
    ScaleX = scaleX;
    ScaleY = scaleY;
    IsDirty = true;
}

void AlphaShapeClass::SetRotation(float angle) {
    Rotation = angle;
    while (Rotation >= 360.0f) Rotation -= 360.0f;
    while (Rotation < 0.0f) Rotation += 360.0f;
    IsDirty = true;
}

void AlphaShapeClass::SetFrame(int32 frame) {
    if (frame < 0) frame = 0;
    if (frame >= FrameCount) frame = FrameCount - 1;
    CurrentFrame = frame;
    IsDirty = true;
}

void AlphaShapeClass::UpdateAnimation() {
    if (!AnimationPlaying || AnimationPaused) return;
    if (FrameCount <= 1) return;

    ++AnimationTimer;
    if (AnimationTimer >= AnimationSpeed) {
        AnimationTimer = 0;
        ++CurrentFrame;
        if (CurrentFrame >= FrameCount) {
            if (AnimationLooping) {
                CurrentFrame = 0;
            } else {
                CurrentFrame = FrameCount - 1;
                AnimationPlaying = false;
            }
        }
        IsDirty = true;
    }
}

void AlphaShapeClass::PlayAnimation(bool loop) {
    AnimationPlaying = true;
    AnimationPaused = false;
    AnimationLooping = loop;
    AnimationTimer = 0;
    CurrentFrame = 0;
}

void AlphaShapeClass::StopAnimation() {
    AnimationPlaying = false;
    AnimationPaused = false;
    AnimationTimer = 0;
}

void AlphaShapeClass::PauseAnimation() {
    AnimationPaused = true;
}

void AlphaShapeClass::ResumeAnimation() {
    AnimationPaused = false;
}

void AlphaShapeClass::SetAnimationSpeed(int32 speed) {
    AnimationSpeed = speed;
    if (AnimationSpeed < 1) AnimationSpeed = 1;
    if (AnimationSpeed > 60) AnimationSpeed = 60;
}

void AlphaShapeClass::SetShadow(bool enabled, int32 alpha, int32 offsetX, int32 offsetY) {
    IsShadow = enabled;
    ShadowAlpha = alpha;
    if (ShadowAlpha < 0) ShadowAlpha = 0;
    if (ShadowAlpha > 255) ShadowAlpha = 255;
    ShadowOffsetX = offsetX;
    ShadowOffsetY = offsetY;
}

void AlphaShapeClass::SetVisible(bool visible) {
    IsVisible = visible;
}

void AlphaShapeClass::SetColorTint(uint32 color) {
    ColorTint = color;
}

void AlphaShapeClass::ApplyColorTint(DSurface* surface) {
    if (!surface) return;
    uint32 r = (ColorTint >> 16) & 0xFF;
    uint32 g = (ColorTint >> 8) & 0xFF;
    uint32 b = ColorTint & 0xFF;

    for (int32 y = 0; y < surface->GetHeight(); ++y) {
        for (int32 x = 0; x < surface->GetWidth(); ++x) {
            // GetPixel returns BYTE in Surface.h
            BYTE pixel = surface->GetPixel(x, y);
            // Apply tint to the palette index (simplified)
            uint32 pr = (pixel >> 5) & 0x07;
            uint32 pg = (pixel >> 2) & 0x07;
            uint32 pb = pixel & 0x03;
            pr = (pr * r) / 255;
            pg = (pg * g) / 255;
            pb = (pb * b) / 255;
            surface->SetPixel(x, y, static_cast<BYTE>((pr << 5) | (pg << 2) | pb));
        }
    }
}

void AlphaShapeClass::SetFlash(int32 duration, uint32 color) {
    FlashTimer = duration;
    FlashDuration = duration;
    FlashColor = color;
}

int32 AlphaShapeClass::ApplyFlashEffect(int32 alpha) {
    if (FlashTimer > 0) {
        --FlashTimer;
        float flashFactor = static_cast<float>(FlashTimer) / static_cast<float>(FlashDuration);
        float intensity = 0.5f + 0.5f * std::sin(flashFactor * 3.14159f * 4.0f);
        return static_cast<int32>(alpha * (0.5f + 0.5f * intensity));
    }
    return alpha;
}

void AlphaShapeClass::StartFadeIn(int32 duration) {
    FadeInTimer = duration;
    FadeInDuration = duration;
    FadeOutTimer = 0;
    FadeOutDuration = 0;
}

void AlphaShapeClass::StartFadeOut(int32 duration) {
    FadeOutTimer = duration;
    FadeOutDuration = duration;
    FadeInTimer = 0;
    FadeInDuration = 0;
}

void AlphaShapeClass::UpdateFade() {
    if (FadeInTimer > 0) {
        --FadeInTimer;
    }
    if (FadeOutTimer > 0) {
        --FadeOutTimer;
    }
}

void AlphaShapeClass::Update() {
    UpdateAnimation();
    UpdateFade();
    if (FlashTimer > 0) {
        --FlashTimer;
    }
}

void AlphaShapeClass::GetPosition(int32& x, int32& y, int32& z) const {
    x = PositionX;
    y = PositionY;
    z = PositionZ;
}

int32 AlphaShapeClass::GetAlpha() const {
    return AlphaLevel;
}

float AlphaShapeClass::GetRotation() const {
    return Rotation;
}

int32 AlphaShapeClass::GetCurrentFrame() const {
    return CurrentFrame;
}

int32 AlphaShapeClass::GetFrameCount() const {
    return FrameCount;
}

int32 AlphaShapeClass::GetWidth() const {
    return ShapeWidth;
}

int32 AlphaShapeClass::GetHeight() const {
    return ShapeHeight;
}

bool AlphaShapeClass::IsPlaying() const {
    return AnimationPlaying && !AnimationPaused;
}

bool AlphaShapeClass::IsVisibleShape() const {
    return IsVisible && ShapeData != nullptr;
}

void AlphaShapeClass::SetCenter(int32 cx, int32 cy) {
    CenterX = cx;
    CenterY = cy;
}

void AlphaShapeClass::SetZValue(int32 z) {
    ZValue = z;
}

int32 AlphaShapeClass::GetZValue() const {
    return ZValue;
}