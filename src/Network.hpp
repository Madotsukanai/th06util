#pragma once
#include "ZunResult.hpp"
#include "inttypes.hpp"

namespace th06
{
#pragma pack(push, 1)
struct HandshakeData
{
    u32 magic;
    i32 character;
    i32 shotType;
    i32 difficulty;
    u16 rngSeed;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DelayNegotiationPacket
{
    u32 magic;
    i32 type;
    u32 timestamp;
    i32 delayValue;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct GameFramePacket
{
    u32 frame;
    u16 input;
    f32 posX;
    f32 posY;
    u8 playerState;
    u8 respawnTimer;
    i16 invulnFrames;
    u8 livesRemaining;
    u8 bombsRemaining;
    u8 isInGameMenu;
    u8 isInRetryMenu;
    u8 nextSupervisorState;
    u16 rngSeed;
    u32 rngGenCount;
    u32 score;
    u32 highScore;
    u32 guiScore;
    u16 currentPower;
    i32 grazeInStage;
    i32 rank;
    i32 subRank;
    u16 pointItemsCollectedInStage;
};
#pragma pack(pop)

class NetworkManager
{
  public:
    NetworkManager();
    ~NetworkManager();
    ZunResult Initialize();
    void Cleanup(bool closePortMapping = true);

    ZunResult BeginConnect(const char *ip, int port);
    int CheckConnect();
    ZunResult StartServer(int port);
    ZunResult ExchangeHandshake(HandshakeData *local, HandshakeData *remote);
    int NegotiateDelay(int configuredDelay);
    ZunResult ExchangeInput(u16 *localInput, u16 *remoteInput);
    ZunResult ExchangeData(void *localData, void *remoteData, int size);
    ZunResult GetLocalIP(char *buffer, int bufferSize);
    ZunResult GetPublicIP(char *buffer, int bufferSize);
    ZunResult SendPacket(const void *data, int size);
    ZunResult RecvPacketNonBlocking(void *data, int size, bool *received);
    ZunResult RecvPacketBlocking(void *data, int size, int timeoutMs);
    bool CheckForConnection();
    bool IsHost() const
    {
        return m_isHost;
    }
    bool IsConnected() const
    {
        return m_isConnected;
    }
    bool IsPortMapped() const
    {
        return m_portMapped;
    }
    i32 GetRemoteCharacter() const
    {
        return m_remoteCharacter;
    }
    i32 GetRemoteShotType() const
    {
        return m_remoteShotType;
    }

  private:
    ZunResult OpenPortMapping(int port);
    void ClosePortMapping();
    void *m_socket;
    i32 m_remoteCharacter;
    i32 m_remoteShotType;
    int m_mappedPort;
    bool m_isHost;
    bool m_initialized;
    bool m_portMapped;
    bool m_isConnected;
};
extern NetworkManager g_NetworkManager;
} // namespace th06
