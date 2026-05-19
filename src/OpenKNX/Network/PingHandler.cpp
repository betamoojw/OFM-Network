#include "OpenKNX/Network/PingHandler.h"
#include "OpenKNX.h"

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_ESP32)
#include <lwip/dns.h>
#include <lwip/ip_addr.h>
#endif
#ifdef ARDUINO_ARCH_ESP32
#include <lwip/tcpip.h>
#endif

namespace OpenKNX
{
    namespace Network
    {

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_ESP32)
        struct DnsCallbackArg
        {
            PingHandler* queue;
            uint32_t timeoutMs;
            std::function<void(IPAddress, bool, uint32_t)> callback;
        };

        static void dnsFoundCallback(const char* name, const ip_addr_t* ipaddr, void* arg)
        {
            DnsCallbackArg* darg = static_cast<DnsCallbackArg*>(arg);
            if (ipaddr != nullptr)
            {
                const ip4_addr_t* ip4 = ip_2_ip4(ipaddr);
                IPAddress ip(ip4_addr1(ip4), ip4_addr2(ip4), ip4_addr3(ip4), ip4_addr4(ip4));
#ifdef ARDUINO_ARCH_ESP32
                // Callback runs inside TCPIP task which holds the core lock.
                // Calling sendto() here would deadlock — enqueue for loop() instead.
                darg->queue->enqueueDnsResult(ip, darg->timeoutMs, darg->callback);
#else
                // RP2040: NO_SYS=1, single-threaded — direct call is safe.
                darg->queue->ping(ip, darg->callback, darg->timeoutMs);
#endif
            }
            else if (darg->callback)
            {
                darg->callback(IPAddress(0, 0, 0, 0), false, 0);
            }
            delete darg;
        }
#endif

        PingHandler::PingHandler()
            : _activePingCount(0), _lastLoopTime(0), _nextEchoSeq(0)
        {
            for (int i = 0; i < OPENKNX_PING_PARALLEL; i++)
            {
                _activeSlots[i].state = PingRequest::IDLE;
                _activeSlots[i].echoId = i;
                _activeSlots[i].echoSeq = 0;
                _activeSlots[i].sock = -1;
            }

            platformInitialize();
        }

        PingHandler::~PingHandler()
        {
            platformCleanup();
        }

        void PingHandler::ping(IPAddress target, std::function<void(IPAddress, bool, uint32_t)> callback,
                               uint32_t timeoutMs)
        {
            if (target == IPAddress(0, 0, 0, 0))
            {
                if (callback)
                    callback(target, false, 0);
                return;
            }

            PingRequest request;
            request.target = target;
            request.timeoutMs = timeoutMs;
            request.startTimeMs = 0;
            request.rttMs = 0;
            request.state = PingRequest::IDLE;
            request.callback = callback;
            request.echoSeq = _nextEchoSeq++;

            int freeSlot = findFreeSlot();
            if (freeSlot >= 0)
            {
                _activeSlots[freeSlot] = request;
                _activeSlots[freeSlot].state = PingRequest::PINGING;
                _activeSlots[freeSlot].startTimeMs = millis();
                _activePingCount++;

                if (!platformSendPing(freeSlot, target))
                    dispatchCallback(freeSlot, false, 0);
            }
            else
            {
                _pendingQueue.push_back(request);
            }
        }

        void PingHandler::enqueueDnsResult(IPAddress target, uint32_t timeoutMs,
                                           std::function<void(IPAddress, bool, uint32_t)> callback)
        {
#ifdef ARDUINO_ARCH_ESP32
            taskENTER_CRITICAL(&_dnsMux);
#endif
            _resolvedDns.push_back({target, timeoutMs, callback});
#ifdef ARDUINO_ARCH_ESP32
            taskEXIT_CRITICAL(&_dnsMux);
#endif
        }

        void PingHandler::loop()
        {
            if (!delayCheckMillis(_lastLoopTime, 50))
                return;
            _lastLoopTime = millis();

            processDnsResults();
            processPendingSlots();
            startNextPending();
        }

        void PingHandler::processDnsResults()
        {
            while (!_resolvedDns.empty())
            {
#ifdef ARDUINO_ARCH_ESP32
                taskENTER_CRITICAL(&_dnsMux);
#endif
                DnsResult r = _resolvedDns.front();
                _resolvedDns.pop_front();
#ifdef ARDUINO_ARCH_ESP32
                taskEXIT_CRITICAL(&_dnsMux);
#endif
                ping(r.target, r.callback, r.timeoutMs);
            }
        }

        int PingHandler::findFreeSlot()
        {
            for (int i = 0; i < OPENKNX_PING_PARALLEL; i++)
            {
                if (_activeSlots[i].state == PingRequest::IDLE)
                    return i;
            }
            return -1;
        }

        void PingHandler::processPendingSlots()
        {
            uint32_t replyTimeMs = 0;
            for (int i = 0; i < OPENKNX_PING_PARALLEL; i++)
            {
                if (_activeSlots[i].state == PingRequest::PINGING)
                {
                    if (platformCheckReply(i, replyTimeMs))
                    {
                        uint32_t rttMs = replyTimeMs - _activeSlots[i].startTimeMs;
                        dispatchCallback(i, true, rttMs);
                    }
                    else
                    {
                        uint32_t elapsed = millis() - _activeSlots[i].startTimeMs;
                        if (elapsed >= _activeSlots[i].timeoutMs)
                        {
                            platformClosePing(i);
                            dispatchCallback(i, false, 0);
                        }
                    }
                }
                else if (_activeSlots[i].state == PingRequest::SUCCESS ||
                         _activeSlots[i].state == PingRequest::TIMEOUT)
                {
                    _activeSlots[i].state = PingRequest::IDLE;
                    _activePingCount--;
                }
            }
        }

        void PingHandler::startNextPending()
        {
            while (!_pendingQueue.empty() && findFreeSlot() >= 0)
            {
                PingRequest request = _pendingQueue.front();
                _pendingQueue.pop_front();

                int freeSlot = findFreeSlot();
                if (freeSlot >= 0)
                {
                    _activeSlots[freeSlot] = request;
                    _activeSlots[freeSlot].state = PingRequest::PINGING;
                    _activeSlots[freeSlot].startTimeMs = millis();
                    _activePingCount++;

                    if (!platformSendPing(freeSlot, request.target))
                        dispatchCallback(freeSlot, false, 0);
                }
            }
        }

        void PingHandler::dispatchCallback(int slotIndex, bool reachable, uint32_t rttMs)
        {
            if (slotIndex < 0 || slotIndex >= OPENKNX_PING_PARALLEL)
                return;

            PingRequest& req = _activeSlots[slotIndex];
            if (req.callback)
                req.callback(req.target, reachable, rttMs);

            req.state = reachable ? PingRequest::SUCCESS : PingRequest::TIMEOUT;
            req.callback = nullptr;
        }

        void PingHandler::ping(const std::string& host, std::function<void(IPAddress, bool, uint32_t)> callback,
                               uint32_t timeoutMs)
        {
            IPAddress ip;
            if (ip.fromString(host.c_str()))
            {
                ping(ip, callback, timeoutMs);
                return;
            }

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_ESP32)
            DnsCallbackArg* arg = new DnsCallbackArg{this, timeoutMs, callback};
            ip_addr_t resolved;
#ifdef ARDUINO_ARCH_ESP32
            LOCK_TCPIP_CORE();
#endif
            err_t err = dns_gethostbyname(host.c_str(), &resolved, dnsFoundCallback, arg);
#ifdef ARDUINO_ARCH_ESP32
            UNLOCK_TCPIP_CORE();
#endif
            if (err == ERR_OK)
            {
                // Already cached — enqueue so ping() runs from loop(), not from here
                dnsFoundCallback(host.c_str(), &resolved, arg);
            }
            else if (err != ERR_INPROGRESS)
            {
                if (callback)
                    callback(IPAddress(0, 0, 0, 0), false, 0);
                delete arg;
            }
            // ERR_INPROGRESS: dnsFoundCallback will fire when resolved
#else
            if (callback)
                callback(IPAddress(0, 0, 0, 0), false, 0);
#endif
        }

#if !defined(ARDUINO_ARCH_ESP32) && !defined(ARDUINO_ARCH_RP2040)

        bool PingHandler::platformSendPing(int slotIndex, IPAddress target)
        {
            return false;
        }

        bool PingHandler::platformCheckReply(int slotIndex, uint32_t& replyTimeMs)
        {
            return false;
        }

        void PingHandler::platformClosePing(int slotIndex) {}

        void PingHandler::platformInitialize() {}

        void PingHandler::platformCleanup() {}

#endif

    } // namespace Network
} // namespace OpenKNX
