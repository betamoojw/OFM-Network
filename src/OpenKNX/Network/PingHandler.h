#pragma once

#include <IPAddress.h>
#include <array>
#include <deque>
#include <functional>
#include <string>

#ifdef ARDUINO_ARCH_ESP32
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
#endif

#ifndef OPENKNX_PING_TIMEOUT
#define OPENKNX_PING_TIMEOUT 1000
#endif

#ifndef OPENKNX_PING_PARALLEL
#define OPENKNX_PING_PARALLEL 5
#endif

namespace OpenKNX
{
    namespace Network
    {

        struct PingRequest
        {
            IPAddress target;
            uint32_t timeoutMs;
            uint32_t startTimeMs;
            uint32_t rttMs;
            int sock;

            enum State
            {
                IDLE = 0,
                PINGING = 1,
                TIMEOUT = 2,
                SUCCESS = 3
            } state;

            std::function<void(IPAddress, bool, uint32_t)> callback;
            uint8_t echoId;
            uint8_t echoSeq;
        };

        struct DnsResult
        {
            IPAddress target;
            uint32_t timeoutMs;
            std::function<void(IPAddress, bool, uint32_t)> callback;
        };

        class PingHandler
        {
          public:
            PingHandler();
            ~PingHandler();

            std::string logPrefix() { return "Ping"; }

            void ping(IPAddress target, std::function<void(IPAddress, bool, uint32_t)> callback = nullptr,
                      uint32_t timeoutMs = OPENKNX_PING_TIMEOUT);
            void ping(const std::string& host, std::function<void(IPAddress, bool, uint32_t)> callback = nullptr,
                      uint32_t timeoutMs = OPENKNX_PING_TIMEOUT);
            void loop();

            void enqueueDnsResult(IPAddress target, uint32_t timeoutMs,
                                  std::function<void(IPAddress, bool, uint32_t)> callback);

            uint32_t getPendingCount() const { return _pendingQueue.size(); }
            uint32_t getActivePingCount() const { return _activePingCount; }

          private:
            std::deque<PingRequest> _pendingQueue;
            std::deque<DnsResult> _resolvedDns;
            std::array<PingRequest, OPENKNX_PING_PARALLEL> _activeSlots;
            uint32_t _activePingCount;
            uint32_t _lastLoopTime;
            uint8_t _nextEchoSeq;

#ifdef ARDUINO_ARCH_ESP32
            portMUX_TYPE _dnsMux = portMUX_INITIALIZER_UNLOCKED;
#endif

            void startNextPending();
            void processPendingSlots();
            void processDnsResults();
            int findFreeSlot();
            void dispatchCallback(int slotIndex, bool reachable, uint32_t rttMs);

            bool platformSendPing(int slotIndex, IPAddress target);
            bool platformCheckReply(int slotIndex, uint32_t& replyTimeMs);
            void platformClosePing(int slotIndex);
            void platformInitialize();
            void platformCleanup();
        };

    } // namespace Network
} // namespace OpenKNX
