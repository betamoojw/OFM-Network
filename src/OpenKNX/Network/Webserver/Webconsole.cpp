#if defined(OPENKNX_WEBCONSOLE) && (defined(KNX_IP_WIFI) || defined(KNX_IP_LAN))

#include "OpenKNX/Network/Webserver/Webconsole.h"
#include "OpenKNX.h"
#include "OpenKNX/Network/Module.h"
#include "OpenKNX/Network/Webserver/Webserver.h"

#include <algorithm>

namespace OpenKNX
{
    namespace Network
    {

        void Webconsole::setup()
        {
            openknxNetwork.webserver.addMenuItem("Gerätekonsole", "/console", 100);

            openknxNetwork.webserver.addRoute(WEB_GET, "/console",
                                              [](WebRequest&, WebResponse& res) {
                                                  static const char content[] =
                                                      "<style>"
                                                      "html,body{height:100%;overflow:hidden}"
                                                      "main{display:flex;flex-direction:column;height:100vh;padding:1em;gap:.5em}"
                                                      "#console-wrap{flex:1;display:flex;flex-direction:column;min-height:0;gap:.5em}"
                                                      "#console-out{flex:1;background:#0d0d0d;color:#fff;font-family:monospace;"
                                                      "font-size:1em;padding:1em;border-radius:4px 4px 0 0;overflow-y:auto;"
                                                      "white-space:pre-wrap;border:1px solid #333}"
                                                      "#console-inp-row{display:flex;gap:.5em;flex:none}"
                                                      "#console-inp{flex:1;background:#0d0d0d;color:#fff;font-family:monospace;"
                                                      "font-size:1em;padding:.5em .75em;border:1px solid #333;border-top:none;"
                                                      "border-radius:0 0 0 4px;outline:none}"
                                                      "#console-inp::placeholder{color:#555}"
                                                      "#console-send{padding:.5em 1.2em;background:#449841;color:#fff;border:none;"
                                                      "border-radius:0 0 4px 0;cursor:pointer;font-size:1em}"
                                                      "#console-send:hover{background:#357a31}"
                                                      "</style>"
                                                      "<div id='console-wrap'>"
                                                      "<div id='console-out'>Verbinde...</div>"
                                                      "<div id='console-inp-row'>"
                                                      "<input id='console-inp' type='text' placeholder='Befehl eingeben...' autocomplete='off'>"
                                                      "<button id='console-send' type='button'>Senden</button>"
                                                      "</div>"
                                                      "</div>"
                                                      "<script>"
                                                      "const out=document.getElementById('console-out');"
                                                      "const inp=document.getElementById('console-inp');"
                                                      "const btn=document.getElementById('console-send');"
                                                      "const MAX_LINES=500;"
                                                      // Log-Zeilen kommen roh inkl. ANSI-Sequenzen an. Ausgabe nur
                                                      // über textContent, nie als HTML.
                                                      "const ANSI={31:'red',32:'green',33:'yellow',90:'gray'};"
                                                      "function trim(){"
                                                      "while(out.childNodes.length>MAX_LINES)out.removeChild(out.firstChild);"
                                                      "out.scrollTop=out.scrollHeight;"
                                                      "}"
                                                      "function emit(frag,text,cls){"
                                                      "if(!text)return;"
                                                      "if(cls){const s=document.createElement('span');s.className=cls;"
                                                      "s.textContent=text;frag.appendChild(s);}"
                                                      "else frag.appendChild(document.createTextNode(text));"
                                                      "}"
                                                      "function appendOut(text){"
                                                      "const frag=document.createDocumentFragment();"
                                                      "const re=/\\x1b\\[([0-9;]*)m/g;"
                                                      "let cls=null,last=0,m;"
                                                      "while((m=re.exec(text))!==null){"
                                                      "emit(frag,text.slice(last,m.index),cls);"
                                                      // "1;32": letzter bekannter Code gewinnt, "0" setzt zurück
                                                      "cls=null;"
                                                      "for(const c of m[1].split(';')){const n=ANSI[parseInt(c,10)];if(n)cls=n;}"
                                                      "last=re.lastIndex;"
                                                      "}"
                                                      "emit(frag,text.slice(last),cls);"
                                                      "out.appendChild(frag);trim();"
                                                      "}"
                                                      "function appendStatus(text,cls){"
                                                      "const s=document.createElement('span');"
                                                      "s.className=cls;s.textContent=text+'\\n';"
                                                      "out.appendChild(s);trim();"
                                                      "}"
                                                      "let ws;"
                                                      "function connect(){"
                                                      "ws=new WebSocket('ws://'+location.host+'/console');"
                                                      "ws.onopen=()=>{appendStatus('[verbunden]','green');};"
                                                      "ws.onmessage=e=>{appendOut(e.data);};"
                                                      "ws.onclose=()=>{appendStatus('[Verbindung getrennt \\u2014 Seite neu laden]','red');};"
                                                      "}"
                                                      "connect();"
                                                      "function send(){"
                                                      "const v=inp.value.trim();inp.value='';"
                                                      "if(ws&&ws.readyState===1)ws.send(v);"
                                                      "}"
                                                      "btn.onclick=send;"
                                                      "inp.addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();send();}});"
                                                      "</script>";

                                                  res.setContentType("text/html");
                                                  res.setLayout(true);
                                                  res.sendStatic(content);
                                              });

            openknxNetwork.webserver.addSocket("/console", [this](int clientId, WebSocketFrame* f) {
                    // Queue command for loop() — avoids running processCommand() inside
                    // a network callback (lwIP on RP2040, httpd task on ESP32).
                    // Ein leeres Frame (length == 0) = blankes Enter: wird ebenfalls
                    // übernommen, damit der Server eine Leerzeile ausgibt.
                    bool valid = true;
                    for (int i = 0; i < f->length; i++)
                    {
                        uint8_t c = f->data[i];
                        if ((c < 0x20 || c > 0x7E) && c != '\r' && c != '\n' && c != '\t')
                        {
                            valid = false;
                            break;
                        }
                    }
                    if (valid)
                    {
                        size_t len = (f->length < (int)sizeof(_pendingCmd) - 1)
                                         ? (size_t)f->length
                                         : sizeof(_pendingCmd) - 1;
                        memcpy(_pendingCmd, f->data, len);
                        _pendingCmd[len] = '\0';
                        _hasPendingCmd = true;
                    } },
                [this](int clientId, bool connected) {
                    // Sonst erbt ein neuer Client mit gleichem fd/Slot die alte
                    // Leseposition und bekommt einen Backlog-Dump.
                    if (!connected) _consoleReadPos.erase(clientId);
                });
        }

        void Webconsole::loop()
        {
            // Execute queued console command from the safe loop-task context.
            if (_hasPendingCmd)
            {
                char cmd[sizeof(_pendingCmd)];
                memcpy(cmd, _pendingCmd, sizeof(cmd));
                _pendingCmd[0] = '\0';
                _hasPendingCmd = false;
                // Echo der Eingabe via Logger → Serial + alle Webconsole-Clients.
                // Auch ein leerer Befehl wird geloggt und erzeugt so eine Leerzeile.
                openknx.logger.log(cmd);
                if (cmd[0] != '\0')
                    openknx.console.processCommand(cmd);
            }

            std::vector<int> clients = openknxNetwork.webserver.connectedClientFds("/console");
            if (clients.empty()) return;

            uint32_t writePos = openknx.logger.ringWritePos();

            // Eine Payload, die nicht in einen WS-Frame passt, wäre dauerhaft unsendbar
            // und würde die Retry-Schleife unten endlos blockieren.
            uint32_t maxLine = 512;
            const size_t maxPayload = openknxNetwork.webserver.maxWebsocketPayload();
            if (maxPayload < maxLine) maxLine = (uint32_t)maxPayload;

            // Remove stale entries for disconnected clients
            for (auto it = _consoleReadPos.begin(); it != _consoleReadPos.end();)
            {
                if (std::find(clients.begin(), clients.end(), it->first) == clients.end())
                    it = _consoleReadPos.erase(it);
                else
                    ++it;
            }

            for (int fd : clients)
            {
                // New client: start from current write position (no backlog)
                if (_consoleReadPos.find(fd) == _consoleReadPos.end())
                    _consoleReadPos[fd] = writePos;

                uint32_t& readPos = _consoleReadPos[fd];

                // Snap to oldest valid data if client fell too far behind
                if (writePos - readPos > Log::Logger::RING_SIZE)
                    readPos = writePos - Log::Logger::RING_SIZE;

                while (readPos < writePos)
                {
                    // Send only complete lines so the client-side ANSI parser never has
                    // to carry state across messages. Fall back to a fixed chunk if no
                    // newline within maxLine bytes (e.g. very long hex-dump lines).
                    uint32_t pos = readPos;
                    uint32_t limit = (writePos - readPos < maxLine) ? writePos : readPos + maxLine;
                    bool foundNewline = false;
                    while (pos < limit)
                    {
                        if (openknx.logger.ringBuf()[pos % Log::Logger::RING_SIZE] == '\n')
                        {
                            foundNewline = true;
                            pos++;
                            break;
                        }
                        pos++;
                    }
                    if (!foundNewline && writePos - readPos < maxLine) break;

                    std::string line;
                    line.reserve(pos - readPos);
                    for (uint32_t i = readPos; i < pos; i++)
                        line += openknx.logger.ringBuf()[i % Log::Logger::RING_SIZE];

                    if (!openknxNetwork.webserver.sendToClient("/console", fd, line.c_str(), line.size())) break;
                    readPos = pos;
                }
            }
        }

    } // namespace Network
} // namespace OpenKNX

#endif // defined(OPENKNX_WEBCONSOLE) && (defined(KNX_IP_WIFI) || defined(KNX_IP_LAN))
