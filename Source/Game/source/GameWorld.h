#pragma once

#include "ClientObject.h"

#include <tge/drawers/DebugDrawer.h>
#include <tge/drawers/SpriteDrawer.h>
#include <tge/graphics/GraphicsEngine.h>
#include <tge/texture/TextureManager.h>

class GameWorld
{
  public:
    GameWorld() = default;
    ~GameWorld() = default;

    void Init();
    void Update(float deltaTime);
    void Render();

  private:
    Tga::Sprite2DInstanceData playerSprite = {};
    Tga::SpriteSharedData sharedSpriteData = {};
};
