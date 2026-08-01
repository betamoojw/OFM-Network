#include "OpenKNX/Network/Webserver/Webserver.h"

#if defined(OPENKNX_WEBSERVER) && (defined(KNX_IP_WIFI) || defined(KNX_IP_LAN))

#include "OpenKNX.h"
#include "OpenKNX/Network/Module.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace OpenKNX
{
    namespace Network
    {

        // ── Layout ─────────────────────────────────────────────────────────

        static const char baseCss[] =
            "*{box-sizing:border-box;margin:0;padding:0}"
            "button,input,select,textarea{font-family:inherit;font-size:inherit;padding:6px;margin:revert;border:revert;border-radius:0;background:revert;color:revert}"
            "body{display:flex;min-height:100vh;font-family:sans-serif;background:#fff;color:#111}"
            "nav{width:220px;min-width:220px;background:#000;display:flex;flex-direction:column;"
            "align-items:center;padding:6px 0;height:100vh;position:sticky;top:0;overflow-y:auto;"
            "box-shadow:3px 0 4px rgba(0,0,0,.18);z-index:1}"
            ".logo{width:180px;padding:0 24px;margin-top:12px;margin-bottom:12px}"
            ".logo img{width:100%;height:auto}"
            "nav a.menu{display:block;width:100%;padding:11px 24px;color:#aaa;text-decoration:none;"
            "font-size:.9em;border-left:3px solid transparent;transition:all .15s}"
            "nav a.menu:hover{background:#1a1a1a;color:#fff;border-left-color:#449841}"
            "a.menu.active{color:#fff;border-left-color:#449841;background:#111}"
            ".logo a{display:block;padding:0}"
            ".nav-wiki{margin-top:auto;width:100%;padding:8px 0 8px;text-align:center}"
            ".nav-wiki a,.nav-wiki a:hover{color:#555;font-size:.8em;padding:0;border:none;"
            "background:none;display:inline;width:auto;text-decoration:none}"
            ".nav-wiki a:hover{color:#888}"
            ".menu-container{width:100%;margin:12px 0;display:flex;flex-direction:column}"
            ".nav-footer{width:100%;padding:12px 24px 12px 27px;border-top:1px solid #222;"
            "display:flex;flex-direction:column;gap:10px}"
            ".nav-footer a{color:#666;text-decoration:none}"
            ".prog-status{display:flex;align-items:center;color:#666;font-size:.8em;gap:8px}"
            ".prog-dot{display:inline-block;width:8px;height:8px;border-radius:50%;flex-shrink:0}"
            ".dot-green{background:#449841}.dot-red{background:#ff5555}.dot-off{background:#555}"
            ".nav-firm{font-weight:bold;font-size:.8em;color:#aaa}"
            ".nav-info{display:flex;align-items:center;color:#666;font-size:.8em;gap:8px}"
            "main{flex:1;padding:2em;min-width:0;display:flex;flex-direction:column}"
            ".meta{color:#666;font-size:.8em;margin-bottom:1.5em}"
            ".red{color:#ff5555}.green{color:#449841}.yellow{color:#ffff55}.gray{color:#888}"
            "h1{font-size:1.25em;font-weight:600;margin-bottom:1.5em;color:#111}"
            "h2{font-size:.95em;font-weight:600;margin:1.5em 0 .75em;color:#449841;"
            "text-transform:uppercase;letter-spacing:.08em}"
            "table{width:100%;border-collapse:collapse;margin-bottom:.5em;font-size:.88em}"
            "td,th{text-align:left;padding:7px 14px}"
            "th{background:#eee;color:#000;border-bottom:1px solid #bbb}"
            "td{border-top:1px solid #eee}"
            "tr:last-child td{border-bottom:1px solid #eee}"
            ".attribute-table td:first-child{color:#888;white-space:nowrap;width:200px}"
            "main a{color:#449841;text-decoration:none;transition:color .15s}"
            "main a:hover{color:#5ab857;text-decoration:underline}"
            ".container{max-width:960px}";

        static const char baseJs[] = "/* OpenKNX */";

        static const char faviconSvg[] =
            "<svg width='32' height='32' viewBox='0 0 32 32' fill='none' xmlns='http://www.w3.org/2000/svg'>"
            "<line x1='7' y1='20' x2='7' y2='15' stroke='#449841' stroke-width='2'/>"
            "<line x1='25' y1='17' x2='25' y2='12' stroke='#449841' stroke-width='2'/>"
            "<line y1='16' x2='32' y2='16' stroke='#449841' stroke-width='2'/>"
            "<rect x='2' y='21' width='10' height='10' fill='#449841'/>"
            "<rect x='20' y='1' width='10' height='10' fill='#449841'/>"
            "<rect x='22' y='23' width='6' height='6' stroke='black' stroke-width='2'/>"
            "<rect x='4' y='3' width='6' height='6' stroke='black' stroke-width='2'/>"
            "</svg>";

        std::string Webserver::buildHeader(const std::string& activeUri)
        {
            std::string nav = "<div class='menu-container'>";
            for (auto& item : _menu)
            {
                bool active = (item.uri == activeUri);
                nav += "<a class='menu";
                if (active) nav += " active";
                nav += "' href='";
                nav += item.uri;
                nav += "'>";
                nav += item.label;
                nav += "</a>";
            }
            nav += "</div>";

            std::string html =
                "<!DOCTYPE html><html><head>"
                "<meta charset='utf-8'>"
                "<title>OpenKNX</title>";

            std::string buster = "?v=" + std::to_string(BUILD_TIMESTAMP);
            html += "<link rel='icon' type='image/svg+xml' href='/assets/favicon.svg" + buster + "'>";

            // Add registered stylesheets
            for (const auto& stylesheet : _stylesheets)
            {
                html += "<link rel='stylesheet' href='" + stylesheet + buster + "'>";
            }

            html += "</head><body>"
                    "<nav>"
                    "<div class='logo'>"
                    "<a href='https://www.openknx.de' target='_blank' rel='noopener noreferrer'>"
                    "<img src='/assets/logo/black.svg" + buster + "' alt='OpenKNX'>"
                    "</a>"
                    "</div>";

            html += nav;
            html += "<div class='nav-wiki'>"
                    "<a href='https://wiki.openknx.de' target='_blank' rel='noopener noreferrer'>"
                    "wiki</a> | "
                    "<a href='https://forum.openknx.de' target='_blank' rel='noopener noreferrer'>"
                    "forum</a>"
                    "</div>"
                    "<div class='nav-footer'>"
                    "<div class='nav-firm'>";
            html += openknx.info.firmwareName();
            html += "</div>"
                    "<div class='nav-info'>"
                    "<span class='prog-dot ";
            html += knx.configured() ? "dot-green" : "dot-off";
            html += "'></span>Adresse: ";
            html += openknx.info.humanIndividualAddress();
            html += "</div>"
                    "<a class='prog-status' href='/prog?mode=";
            html += knx.progMode() ? "0" : "1";
            html += "'>"
                    "<span class='prog-dot ";
            html += knx.progMode() ? "dot-red" : "dot-off";
            html += "'></span>";
            html += knx.progMode() ? "Prog-Modus aktiv" : "Prog-Modus inaktiv";
            html += "</a>"
                    "</div>";
            html += "</nav><main>";
            return html;
        }

        std::string Webserver::buildFooter()
        {
            std::string html = "</main>";

            // Add registered scripts before closing body
            std::string buster = "?v=" + std::to_string(BUILD_TIMESTAMP);
            for (const auto& script : _scripts)
            {
                html += "<script src='" + script + buster + "' defer></script>";
            }

            html += "</body></html>";
            return html;
        }

        void Webserver::addStylesheet(const char* uri)
        {
            _stylesheets.push_back(uri);
        }

        void Webserver::addJavaScript(const char* uri)
        {
            _scripts.push_back(uri);
        }

#ifdef OPENKNX_WEBSERVER
        void Webserver::buildOverviewPage(WebResponse& res)
        {
            auto row = [](const char* label, const std::string& val) -> std::string {
                return std::string("<tr><td>") + label + "</td><td>" + val + "</td></tr>";
            };

            char buf[64];
            std::string page = "<div class='container'><h1>Übersicht</h1>";

            // ── Gerät ──────────────────────────────────────────────────────────
            page += "<h2>Gerät</h2><table class='attribute-table'><tbody>";
#ifdef DEVICE_ID
            page += row("ID", DEVICE_ID);
#endif
#ifdef DEVICE_NAME
            page += row("Name", DEVICE_NAME);
#elif defined(HARDWARE_NAME)
            page += row("Name", HARDWARE_NAME);
#endif
            page += row("Seriennummer", openknx.info.humanSerialNumber());
            page += "</tbody></table>";

            // ── Firmware ───────────────────────────────────────────────────────
            page += "<h2>Firmware</h2><table class='attribute-table'><tbody>";
            page += row("Name", openknx.info.firmwareName());
            page += row("Version", openknx.info.humanFirmwareVersion(true));
            page += row("Nummer", openknx.info.humanFirmwareNumber());
#if MASK_VERSION == 0x07B0
            page += row("KNX-Typ", "TP (07B0)");
#elif MASK_VERSION == 0x57B0
            page += row("KNX-Typ", "IP (57B0)");
#elif MASK_VERSION == 0x091A
            page += row("KNX-Typ", "Router (091A)");
#else
            snprintf(buf, sizeof(buf), "%04X", MASK_VERSION);
            page += row("KNX-Typ", buf);
#endif
            {
#ifdef OPENKNX_DUALCORE
                const char* cpuMode = openknx.usesDualCore() ? "Dual-Core" : "Single-Core";
#else
                const char* cpuMode = "Single-Core";
#endif
                float temp = openknx.hardware.cpuTemperature();
                if (temp > 0.0f)
                {
                    snprintf(buf, sizeof(buf), "%s (%.1f °C)", cpuMode, temp);
                    page += row("CPU-Modus", buf);
                }
                else
                    page += row("CPU-Modus", cpuMode);
            }
            page += "</tbody></table>";

            // ── Applikation ────────────────────────────────────────────────────
            page += "<h2>Applikation</h2><table class='attribute-table'><tbody>";
            {
                std::string addr = openknx.info.humanIndividualAddress();
                addr += knx.configured()
                            ? " <span class='green'>(Konfiguriert)</span>"
                            : " <span class='gray'>(Nicht konfiguriert)</span>";
                page += row("KNX-Adresse", addr);
            }
            if (openknx.info.applicationNumber() > 0)
            {
                page += row("Version", openknx.info.humanApplicationVersion());
                page += row("Nummer", openknx.info.humanApplicationNumber());
            }
            page += "</tbody></table>";

            // ── Laufzeit ───────────────────────────────────────────────────────
            page += "<h2>Laufzeit</h2><table class='attribute-table'><tbody>";
            {
                uint32_t s = millis() / 1000;
                uint32_t d = s / 86400;
                s %= 86400;
                uint32_t h = s / 3600;
                s %= 3600;
                uint32_t m = s / 60;
                s %= 60;
                snprintf(buf, sizeof(buf), "%ud %02u:%02u:%02u", d, h, m, s);
                page += row("Uptime", buf);
            }
            {
                float fm = freeMemory() / 1024.0f;
                float fmMin = openknx.common.freeMemoryMin() / 1024.0f;
                snprintf(buf, sizeof(buf), "%.3f KiB (min. %.3f KiB)", fm, fmMin);
                page += row("Freier Speicher", buf);
            }
#ifdef OPENKNX_WATCHDOG
            if (openknx.watchdog.active())
            {
                snprintf(buf, sizeof(buf), "Running (%is)", (int)openknx.watchdog.maxPeriod());
                page += row("Watchdog", buf);
            }
            else
                page += row("Watchdog", "Disabled");
#else
            page += row("Watchdog", "Unsupported");
#endif
            page += "</tbody></table>";

            // ── Netzwerk ───────────────────────────────────────────────────────
            {
                uint8_t mac[6] = {};
                openknxNetwork.macAddress(mac);
                char macStr[18];
                snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                bool net = openknxNetwork.established();

                page += "<h2>Netzwerk</h2><table class='attribute-table'><tbody>";
                page += row("Hostname", openknxNetwork.hostName());
                page += row("MAC", macStr);
                page += row("IP-Adresse", net ? openknxNetwork.localIP().toString().c_str()
                                              : "<span class='gray'>–</span>");
                page += row("Subnetz", net ? openknxNetwork.subnetMask().toString().c_str()
                                           : "<span class='gray'>–</span>");
                page += row("Gateway", net ? openknxNetwork.gatewayIP().toString().c_str()
                                           : "<span class='gray'>–</span>");
                page += row("DNS", net ? openknxNetwork.nameServerIP().toString().c_str()
                                       : "<span class='gray'>–</span>");
                page += "</tbody></table>";
            }

            // ── Versionen ──────────────────────────────────────────────────────
            page += "<h2>Versionen</h2><table class='attribute-table'><tbody>";
            page += row("This Firmware", openknx.info.humanFirmwareVersion(true));
#ifdef KNX_Version
            page += row("KNX", KNX_Version);
#endif
#ifdef MODULE_Common_Version
            page += row(openknx.common.logPrefix().c_str(), MODULE_Common_Version);
#endif
            for (uint8_t i = 0; i < openknx.modules.count; i++)
            {
                if (openknx.modules.list[i]->version().empty()) continue;
                page += row(openknx.modules.list[i]->name().c_str(),
                            openknx.modules.list[i]->version());
            }
            page += "<tr><td colspan='2' style='padding-top:1em'></td></tr>";
            page += row("Buildtime", BUILD_DATETIME);
            page += "</tbody></table>";

            page += "</div>";
            res.setContentType("text/html");
            res.setLayout(true);
            res.send(page.c_str());
        }
#endif // OPENKNX_WEBSERVER

        // ── Routing ────────────────────────────────────────────────────────

        bool Webserver::hasSocket(const std::string& uri) const
        {
            for (auto& s : _sockets)
                if (s.uri == uri) return true;
            return false;
        }

        // ── Common setup ───────────────────────────────────────────────────

#ifdef OPENKNX_WEBSERVER
        void Webserver::setup()
        {
            addMenuItem("Übersicht", "/", -127);

            static const char logoSvg[] =
                "<svg viewBox='0 0 402 242' fill='none' xmlns='http://www.w3.org/2000/svg'>"
                "<path d='M1 50C1 34.7 5.1 22.7 13.3 14C21.6 5.3 32.2 1 45.2 1C53.7 1 61.4 3 68.3 7.1C75.1 11.2 80.3 16.9 83.9 24.2C87.5 31.4 89.3 39.7 89.3 48.9C89.3 58.3 87.4 66.7 83.6 74.1C79.9 81.5 74.5 87.1 67.6 90.9C60.6 94.7 53.2 96.6 45.2 96.6C36.5 96.6 28.7 94.5 21.8 90.3C15 86.1 9.8 80.3 6.3 73.1C2.8 65.8 1 58.1 1 50ZM13.6 50.2C13.6 61.3 16.6 70.1 22.5 76.5C28.5 82.9 36.1 86.1 45.1 86.1C54.3 86.1 61.8 82.9 67.8 76.4C73.7 69.9 76.7 60.8 76.7 48.9C76.7 41.4 75.4 34.8 72.9 29.2C70.4 23.6 66.6 19.2 61.7 16.2C56.8 13.1 51.4 11.5 45.3 11.5C36.6 11.5 29.2 14.5 22.9 20.5C16.7 26.4 13.6 36.3 13.6 50.2Z' fill='white'/>"
                "<path d='M103.7 121V28.1H114V36.8C116.4 33.4 119.2 30.8 122.3 29.2C125.3 27.4 129 26.6 133.4 26.6C139.1 26.6 144.2 28 148.5 31C152.9 33.9 156.2 38.1 158.4 43.5C160.6 48.8 161.7 54.6 161.7 61C161.7 67.9 160.5 74 158 79.5C155.6 85 152 89.2 147.3 92.1C142.7 95 137.7 96.5 132.6 96.5C128.8 96.5 125.4 95.7 122.4 94.1C119.4 92.5 116.9 90.5 115 88V121H103.7ZM113.9 61.9C113.9 70.5 115.7 76.9 119.2 81C122.7 85.1 126.9 87.2 131.8 87.2C136.9 87.2 141.2 85 144.7 80.8C148.4 76.5 150.2 69.9 150.2 61C150.2 52.4 148.4 46.1 144.9 41.8C141.4 37.6 137.2 35.5 132.3 35.5C127.5 35.5 123.2 37.7 119.5 42.3C115.8 46.8 113.9 53.3 113.9 61.9Z' fill='white'/>"
                "<path d='M221.3 73.4L233 74.9C231.1 81.7 227.7 87 222.7 90.8C217.7 94.6 211.3 96.5 203.6 96.5C193.8 96.5 186 93.5 180.3 87.5C174.6 81.4 171.7 73 171.7 62.1C171.7 50.8 174.6 42.1 180.4 35.9C186.2 29.7 193.7 26.6 202.9 26.6C211.9 26.6 219.2 29.6 224.9 35.7C230.5 41.8 233.4 50.4 233.4 61.4C233.4 62.1 233.3 63.1 233.3 64.4H183.4C183.8 71.8 185.9 77.4 189.7 81.3C193.4 85.2 198 87.2 203.6 87.2C207.8 87.2 211.3 86.1 214.3 83.9C217.2 81.7 219.5 78.2 221.3 73.4ZM184 55.1H221.4C220.9 49.5 219.5 45.3 217.1 42.4C213.5 38.1 208.8 35.9 203.1 35.9C197.9 35.9 193.5 37.6 189.9 41.1C186.4 44.6 184.4 49.3 184 55.1Z' fill='white'/>"
                "<path d='M247.3 95V28.1H257.5V37.6C262.4 30.2 269.5 26.6 278.8 26.6C282.8 26.6 286.5 27.3 289.9 28.8C293.3 30.2 295.8 32.1 297.5 34.4C299.2 36.8 300.3 39.6 301 42.8C301.4 44.9 301.6 48.6 301.6 53.8V95H290.3V54.3C290.3 49.7 289.9 46.2 289 44C288.1 41.6 286.5 39.8 284.3 38.5C282 37.1 279.4 36.4 276.4 36.4C271.6 36.4 267.4 37.9 263.9 41C260.4 44.1 258.6 49.9 258.6 58.4V95H247.3Z' fill='white'/>"
                "<path d='M103.7 242.5V138H124.8V184.4L167.4 138H195.8L156.4 178.7L197.9 242.5H170.6L141.9 193.4L124.8 210.9V242.5H103.7Z' fill='white'/>"
                "<path d='M209.1 242.5V138H229.6L272.4 207.8V138H292V242.5H270.8L228.7 174.3V242.5H209.1Z' fill='white'/>"
                "<path d='M303.8 242.5L339.5 187.9L307.1 138H331.8L352.8 171.5L373.3 138H397.7L365.2 188.7L400.9 242.5H375.5L352.3 206.3L329.1 242.5H303.8Z' fill='white'/>"
                "<line x1='0' y1='116' x2='97.8' y2='116' stroke='#449841' stroke-width='10'/>"
                "<line x1='120.8' y1='116' x2='403' y2='116' stroke='#449841' stroke-width='10'/>"
                "<line x1='352.8' y1='99' x2='352.8' y2='111' stroke='#449841' stroke-width='10'/>"
                "<rect x='318.8' y='28' width='67' height='67' fill='#449841'/>"
                "<line x1='45.8' y1='133' x2='45.8' y2='121' stroke='#449841' stroke-width='10'/>"
                "<rect x='79.8' y='205' width='67' height='67' transform='rotate(-180 79.8 205)' fill='#449841'/>"
                "</svg>";
            addRoute(WEB_GET, "/assets/base.css", Static("text/css", baseCss));
            addRoute(WEB_GET, "/assets/base.js", Static("text/javascript", baseJs));
            addRoute(WEB_GET, "/assets/logo/black.svg", Static("image/svg+xml", logoSvg));
            addRoute(WEB_GET, "/assets/favicon.svg", Static("image/svg+xml", faviconSvg));

            addStylesheet("/assets/base.css");
            addJavaScript("/assets/base.js");

            addRoute(WEB_GET, "/", [this](WebRequest&, WebResponse& res) {
                buildOverviewPage(res);
            });

            addRoute(WEB_GET, "/prog", [](WebRequest& req, WebResponse& res) {
                // Kein std::stoi — wirft bei nicht-numerischem Input und beendet
                // unter -fno-exceptions das Programm.
                std::string mode = req.getQueryParam("mode");
                if (!mode.empty())
                {
                    char* end = nullptr;
                    long value = strtol(mode.c_str(), &end, 10);
                    if (end != mode.c_str() && *end == '\0')
                        knx.progMode(value != 0);
                }
                // Redirect to home
                res.setStatus(303);
                res.setHeader("Location", "/");
                res.send("");
            });

#ifdef ARDUINO_ARCH_ESP32
            setupEsp32();
#elif defined(ARDUINO_ARCH_RP2040)
            setupRp2040();
#endif
        }
#endif // OPENKNX_WEBSERVER (setup)

        void Webserver::addRoute(uint8_t method, const std::string& uri, WebRouteHandler handler)
        {
            _routes.push_back({method, uri, handler});
        }

        void Webserver::addSocket(const std::string& uri, WebSocketHandler onMessage,
                                  WebSocketConnectHandler onConnect)
        {
            _sockets.push_back({uri, onMessage, onConnect});
        }

        void Webserver::addMenuItem(const std::string& label, const std::string& uri, int8_t priority)
        {
            _menu.push_back({label, uri, priority});
            std::stable_sort(_menu.begin(), _menu.end(),
                             [](const WebMenuItem& a, const WebMenuItem& b) { return a.priority < b.priority; });
        }

        std::vector<int> Webserver::connectedClientFds(const std::string& uri) const
        {
#ifdef ARDUINO_ARCH_ESP32
            // Snapshot under the WS state lock — the list is mutated from the httpd task
            // (upgrade) and the loop task (disconnect) while it is read here.
            wsStateLock();
            std::vector<int> result;
            auto it = _socketClients.find(uri);
            if (it != _socketClients.end()) result = it->second;
            wsStateUnlock();
            return result;
#else
            auto it = _socketClients.find(uri);
            if (it == _socketClients.end()) return {};
            return it->second;
#endif
        }

        bool Webserver::hasClients(const std::string& uri) const
        {
#ifdef ARDUINO_ARCH_ESP32
            wsStateLock();
            auto it = _socketClients.find(uri);
            bool has = it != _socketClients.end() && !it->second.empty();
            wsStateUnlock();
            return has;
#else
            auto it = _socketClients.find(uri);
            return it != _socketClients.end() && !it->second.empty();
#endif
        }

        bool Webserver::handleRequest(WebRequest& req, WebResponse& res)
        {
            // Extract path without query string for routing
            std::string path = req.uri;
            size_t qpos = path.find('?');
            if (qpos != std::string::npos)
                path = path.substr(0, qpos);

            bool handled = false;
            for (auto& route : _routes)
            {
                if (route.method != req.method) continue;

                const std::string& pattern = route.uri;
                bool matched = false;

                if (pattern.size() >= 2 && pattern.back() == '*' && pattern[pattern.size() - 2] == '/')
                {
                    std::string prefix = pattern.substr(0, pattern.size() - 1);
                    matched = (path.compare(0, prefix.size(), prefix) == 0) ||
                              (path + "/" == prefix);
                }
                else
                {
                    matched = (path == pattern);
                }

                if (matched)
                {
                    route.handler(req, res);
                    handled = true;
                    break;
                }
            }

            if (!handled)
            {
                res.setStatus(404);
                res.setContentType("text/html");
                res.setLayout(true);
                res.send("<h2>404 &ndash; Seite nicht gefunden</h2>"
                         "<p class='meta'>Die angeforderte Seite existiert nicht.</p>");
            }

            // Layout anwenden und die Antwort in ihre Sendeblöcke zerlegen. Bewusst hier
            // und nicht im Plattformcode: der kennt danach nur noch res.segments() und
            // muss weder useLayout() auswerten noch buildHeader()/buildFooter() kennen.
            if (!res.isStreaming())
            {
                if (res.useLayout())
                    res.setLayoutChrome(
                        buildHeader(res.activeMenuUri().empty() ? path : res.activeMenuUri()),
                        buildFooter());
                res.finalizeSegments();
            }

            return handled;
        }

        // ── Route-Helpers ──────────────────────────────────────────────────

        WebRouteHandler Webserver::Redirect(const std::string& target)
        {
            return [target](WebRequest& req, WebResponse& res) {
                res.setStatus(301);
                res.setHeader("Location", target.c_str());
                res.send("");
            };
        }

        WebRouteHandler Webserver::Static(const char* mimeType, const char* text)
        {
            std::string mime(mimeType);
            return [mime, text](WebRequest& req, WebResponse& res) {
                res.setContentType(mime.c_str());
                res.setHeader("Cache-Control", "public, max-age=86400");
                res.sendStatic(text);
            };
        }

        WebRouteHandler Webserver::Static(const char* mimeType, const uint8_t* data, int length)
        {
            std::string mime(mimeType);
            return [mime, data, length](WebRequest& req, WebResponse& res) {
                res.setContentType(mime.c_str());
                res.setHeader("Cache-Control", "public, max-age=86400");
                res.sendStatic(data, length);
            };
        }

        void Webserver::logRequest(const WebRequest& req, const WebResponse& res)
        {
            // Get remote address as string
            uint32_t remoteIp = req.getRemoteAddr();
            uint16_t remotePort = req.getRemotePort();

            uint8_t a = (remoteIp >> 0) & 0xFF;
            uint8_t b = (remoteIp >> 8) & 0xFF;
            uint8_t c = (remoteIp >> 16) & 0xFF;
            uint8_t d = (remoteIp >> 24) & 0xFF;
            char clientIp[16];
            snprintf(clientIp, sizeof(clientIp), "%u.%u.%u.%u", a, b, c, d);

            // Get method string
            const char* method = "?";
            switch (req.method)
            {
                case WEB_GET: method = "GET"; break;
                case WEB_POST: method = "POST"; break;
                case WEB_PUT: method = "PUT"; break;
                case WEB_DELETE: method = "DELETE"; break;
            }

            // Get status string (just the code)
            int statusCode = res.statusCode();

            logInfo("Webserver", "%s %s - %s:%u - %d", method, req.getUri().c_str(), clientIp, remotePort, statusCode);
        }

        void Webserver::logWebsocket(const WebRequest& req, int statusCode)
        {
            // Get remote address as string
            uint32_t remoteIp = req.getRemoteAddr();
            uint16_t remotePort = req.getRemotePort();

            uint8_t a = (remoteIp >> 0) & 0xFF;
            uint8_t b = (remoteIp >> 8) & 0xFF;
            uint8_t c = (remoteIp >> 16) & 0xFF;
            uint8_t d = (remoteIp >> 24) & 0xFF;
            char clientIp[16];
            snprintf(clientIp, sizeof(clientIp), "%u.%u.%u.%u", a, b, c, d);

            logInfo("Webserver", "WS %s - %s:%u - %d", req.getUri().c_str(), clientIp, remotePort, statusCode);
        }

    } // namespace Network
} // namespace OpenKNX

#endif // defined(OPENKNX_WEBSERVER) && (defined(KNX_IP_WIFI) || defined(KNX_IP_LAN))
