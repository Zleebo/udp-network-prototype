#include "stdafx.h"

#include "GameWorld.h"

#include <array>

namespace
{
constexpr float kPlayerStep = 10.0f;

void DrawReplicatedObjects(const std::vector<Object>& objects, const Tga::Color& color)
{
    auto& engine = *Tga::Engine::GetInstance();
    auto& spriteDrawer = engine.GetGraphicsEngine().GetSpriteDrawer();

    const Tga::Vector2ui renderSize = engine.GetRenderSize();
    const Tga::Vector2f resolution = {static_cast<float>(renderSize.x), static_cast<float>(renderSize.y)};

    Tga::SpriteSharedData spriteData;
    spriteData.myTexture = engine.GetTextureManager().GetTexture(L"Sprites/tge_logo_w.dds");

    for (const Object& object : objects)
    {
        Tga::Sprite2DInstanceData spriteInstance;
        spriteInstance.myPivot = {0.5f, 0.5f};
        spriteInstance.mySize = Tga::Vector2f(0.25f, 0.25f) * resolution.y;
        spriteInstance.myColor = color;
        spriteInstance.myPosition = object.position;
        spriteDrawer.Draw(spriteData, spriteInstance);
    }
}

void DrawReplicatedEnemies(const std::vector<Enemy>& enemies)
{
    auto& engine = *Tga::Engine::GetInstance();
    auto& spriteDrawer = engine.GetGraphicsEngine().GetSpriteDrawer();

    const Tga::Vector2ui renderSize = engine.GetRenderSize();
    const Tga::Vector2f resolution = {static_cast<float>(renderSize.x), static_cast<float>(renderSize.y)};

    Tga::SpriteSharedData spriteData;
    spriteData.myTexture = engine.GetTextureManager().GetTexture(L"Sprites/tge_logo_w.dds");

    for (const Enemy& enemy : enemies)
    {
        Tga::Sprite2DInstanceData spriteInstance;
        spriteInstance.myPivot = {0.5f, 0.5f};
        spriteInstance.mySize = Tga::Vector2f(0.25f, 0.25f) * resolution.y;
        spriteInstance.myColor = Tga::Color(0.4f, 1.0f, 1.0f, 1.0f);
        spriteInstance.myPosition = enemy.position;
        spriteDrawer.Draw(spriteData, spriteInstance);
    }
}
} // namespace

void GameWorld::Init()
{
    auto& engine = *Tga::Engine::GetInstance();
    const Tga::Vector2ui renderSize = engine.GetRenderSize();
    const Tga::Vector2f resolution = {static_cast<float>(renderSize.x), static_cast<float>(renderSize.y)};

    sharedSpriteData.myTexture = engine.GetTextureManager().GetTexture(L"Sprites/tge_logo_w.dds");

    playerSprite.myPivot = {0.5f, 0.5f};
    playerSprite.myPosition = Tga::Vector2f(0.25f, 0.25f) * resolution;
    playerSprite.mySize = Tga::Vector2f(0.25f, 0.25f) * resolution.y;
    playerSprite.myColor = Tga::Color(0.4f, 1.0f, 1.0f, 1.0f);
}

void GameWorld::Update(const float deltaTime)
{
    UNREFERENCED_PARAMETER(deltaTime);

    ClientHandler& client = ClientHandler::GetInstance();
    if (client.GetWindowHandle() == GetForegroundWindow())
    {
        if (GetAsyncKeyState('A'))
        {
            playerSprite.myPosition += Tga::Vector2f(-kPlayerStep, 0.0f);
        }

        if (GetAsyncKeyState('W'))
        {
            playerSprite.myPosition += Tga::Vector2f(0.0f, kPlayerStep);
        }

        if (GetAsyncKeyState('D'))
        {
            playerSprite.myPosition += Tga::Vector2f(kPlayerStep, 0.0f);
        }

        if (GetAsyncKeyState('S'))
        {
            playerSprite.myPosition += Tga::Vector2f(0.0f, -kPlayerStep);
        }

        if ((GetAsyncKeyState('B') & 0x8001) == 0x8001)
        {
            client.CreateObject(playerSprite.myPosition);
        }

        if ((GetAsyncKeyState('C') & 0x8001) == 0x8001)
        {
            client.RemoveObject(playerSprite.myPosition);
        }

        if ((GetAsyncKeyState('V') & 0x8001) == 0x8001)
        {
            client.CreateMovingObject(playerSprite.myPosition);
        }
    }

    Message positionMessage;
    positionMessage.type = MessageType::Position;
    positionMessage.objectType = MessageObjectType::Enemy;
    positionMessage.position = playerSprite.myPosition;
    client.QueueMessage(positionMessage);
}

void GameWorld::Render()
{
    auto& engine = *Tga::Engine::GetInstance();
    auto& spriteDrawer = engine.GetGraphicsEngine().GetSpriteDrawer();
    spriteDrawer.Draw(sharedSpriteData, playerSprite);

    ClientHandler& client = ClientHandler::GetInstance();
    DrawReplicatedEnemies(client.GetEnemies());
    DrawReplicatedObjects(client.GetObjects(), Tga::Color(0.4f, 0.3f, 0.7f, 1.0f));
    DrawReplicatedObjects(client.GetMovingObjects(), Tga::Color(1.0f, 0.0f, 0.0f, 1.0f));

#ifndef _RETAIL
    Tga::DebugDrawer& debugDrawer = engine.GetDebugDrawer();
    const Tga::Color markerColor =
        (playerSprite.myColor.myR + playerSprite.myColor.myG + playerSprite.myColor.myB) / 3.0f > 0.3f
            ? Tga::Color(0, 0, 0, 1)
            : Tga::Color(1, 1, 1, 1);
    debugDrawer.DrawCircle(playerSprite.myPosition, 5.0f, markerColor);
#endif
}
