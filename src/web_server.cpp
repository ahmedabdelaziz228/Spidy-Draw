/* web_server.cpp - Web Server Implementation */
#include "web_server.h"
#include "motor_control.h"
#include "kinematics.h"
#include "servo_control.h"
#include "config.h"
#include <math.h>

WebServerManager::WebServerManager(MotorController* motors,
                                   Kinematics* kin,
                                   GCodeParser* parser,
                                   ServoController* servo,
                                   GCodeExecutor* executor,
                                   RobotState* state)
    : server(80),
      motorController(motors),
      kinematics(kin),
      gcodeParser(parser),
      servoController(servo),
      gcodeExecutor(executor),
      robotState(state) {
}

bool WebServerManager::setupWiFi(const char* ssid, const char* password) {
    if (WiFi.softAP(ssid, password)) {
        Serial.println("WiFi hotspot started");
        Serial.println(WiFi.softAPIP());
        return true;
    }
    return false;
}

void WebServerManager::begin() {
    server.on("/", [this]() { handleRoot(); });
    server.on("/move", [this]() { handleMove(); });
    server.on("/upload", HTTP_POST,
              [this]() { handleUpload(); },
              [this]() { handleFileUpload(); });
    server.on("/upload-text", HTTP_POST, [this]() { handleUploadText(); });
    server.on("/execute", [this]() { handleExecute(); });
    server.on("/clear", [this]() { handleClear(); });
    server.on("/home", [this]() { handleHome(); });
    server.on("/servo", [this]() { handleServo(); });
    server.on("/stop", [this]() { handleStop(); });
    server.on("/status", [this]() { handleStatus(); });
    server.on("/progress", [this]() { handleProgress(); });
    server.on("/gcode-status", [this]() { handleGCodeStatus(); });

    server.begin();
}

void WebServerManager::handleClient() {
    server.handleClient();
}

void WebServerManager::handleRoot() {
    server.send(200, "text/html", getHTMLPage());
}

void WebServerManager::handleMove() {
    if (!server.hasArg("angle")) {
        server.send(400, "text/plain", "Missing angle parameter");
        return;
    }

    if (robotState->isExecuting || motorController->isBusy()) {
        server.send(400, "text/plain", "Robot is busy");
        return;
    }

    float angle = server.arg("angle").toFloat();
    int repeats = server.hasArg("repeats") ? server.arg("repeats").toInt() : 1;

    if (angle < 0 || angle > 360) {
        server.send(400, "text/plain", "Angle must be 0..360");
        return;
    }

    if (repeats <= 0 || repeats > 20) {
        server.send(400, "text/plain", "Repeats must be 1..20");
        return;
    }

    static constexpr float DEG_PER_RAD = 57.29577951308232f;
    const float rad = angle / DEG_PER_RAD;
    const float distanceMm = repeats * MANUAL_MOVE_MM;
    const float deltaX = cos(rad) * distanceMm;
    const float deltaY = sin(rad) * distanceMm;

    if (!motorController->startManualMove(deltaX, deltaY, *kinematics,
                                          robotState->currentX, robotState->currentY, false)) {
        server.send(400, "text/plain", "Manual move rejected by inverse kinematics/workspace limits");
        return;
    }

    robotState->isMoving = true;
    server.send(200, "text/plain", "Manual movement started");
}

void WebServerManager::handleFileUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        uploadedCommands.clear();
        uploadBuffer = "";
        uploadBytes = 0;
        robotState->currentCommand = 0;
        robotState->totalCommands = 0;
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        uploadBytes += upload.currentSize;
        if (uploadBytes > MAX_UPLOAD_BYTES) {
            return;
        }

        for (size_t i = 0; i < upload.currentSize; i++) {
            uploadBuffer += static_cast<char>(upload.buf[i]);
        }

        int lineStart = 0;
        for (int i = 0; i < uploadBuffer.length(); i++) {
            if (uploadBuffer.charAt(i) == '\n' || uploadBuffer.charAt(i) == '\r') {
                if (i > lineStart) {
                    String line = uploadBuffer.substring(lineStart, i);
                    line.trim();

                    if (!line.isEmpty()) {
                        GCodeCommand cmd = gcodeParser->parseLine(line);
                        if (cmd.valid && uploadedCommands.size() < MAX_GCODE_COMMANDS) {
                            uploadedCommands.push_back(cmd);
                        }
                    }
                }
                lineStart = i + 1;
            }
        }

        uploadBuffer = uploadBuffer.substring(lineStart);
    }
    else if (upload.status == UPLOAD_FILE_END) {
        uploadBuffer.trim();
        if (!uploadBuffer.isEmpty()) {
            GCodeCommand cmd = gcodeParser->parseLine(uploadBuffer);
            if (cmd.valid && uploadedCommands.size() < MAX_GCODE_COMMANDS) {
                uploadedCommands.push_back(cmd);
            }
        }

        robotState->totalCommands = uploadedCommands.size();
    }
}


void WebServerManager::handleUploadText() {
    String body = server.arg("plain");
    body.trim();

    if (body.isEmpty()) {
        server.send(400, "text/plain", "Empty G-code body");
        return;
    }

    uploadedCommands.clear();
    uploadBuffer = "";
    uploadBytes = body.length();
    robotState->currentCommand = 0;
    robotState->totalCommands = 0;
    robotState->stopRequested = false;

    if (uploadBytes > MAX_UPLOAD_BYTES) {
        server.send(400, "text/plain", "G-code body exceeds upload limit");
        return;
    }

    int lineStart = 0;
    for (int i = 0; i <= body.length(); i++) {
        if (i == body.length() || body.charAt(i) == '\n' || body.charAt(i) == '\r') {
            if (i > lineStart) {
                String line = body.substring(lineStart, i);
                line.trim();
                if (!line.isEmpty()) {
                    GCodeCommand cmd = gcodeParser->parseLine(line);
                    if (cmd.valid && uploadedCommands.size() < MAX_GCODE_COMMANDS) {
                        uploadedCommands.push_back(cmd);
                    }
                }
            }
            lineStart = i + 1;
        }
    }

    if (uploadedCommands.empty()) {
        server.send(400, "text/plain", "No valid G-code commands found");
        return;
    }

    robotState->totalCommands = uploadedCommands.size();
    gcodeExecutor->setQueue(uploadedCommands);
    server.send(200, "text/plain", "G-code text uploaded successfully. Loaded " + String(uploadedCommands.size()) + " commands.");
}

void WebServerManager::handleUpload() {
    gcodeExecutor->setQueue(uploadedCommands);
    String response = "G-code uploaded successfully. Loaded " + String(uploadedCommands.size()) + " commands.";
    server.send(200, "text/plain", response);
}

void WebServerManager::handleExecute() {
    robotState->stopRequested = false;
    if (!gcodeExecutor->hasQueue()) {
        server.send(400, "text/plain", "No G-code loaded");
        return;
    }

    if (!gcodeExecutor->start()) {
        server.send(400, "text/plain", "Execution already in progress or queue empty");
        return;
    }

    server.send(200, "text/plain", "Execution started");
}

void WebServerManager::handleClear() {
    gcodeExecutor->stop();
    gcodeExecutor->clearQueue();
    uploadedCommands.clear();

    robotState->currentCommand = 0;
    robotState->totalCommands = 0;
    robotState->isMoving = false;
    robotState->isExecuting = false;
    robotState->stopRequested = false;

    motorController->stopAllMotors();

    server.send(200, "text/plain", "Queue cleared");
}

void WebServerManager::handleHome() {
    if (robotState->isExecuting || motorController->isBusy()) {
        server.send(400, "text/plain", "Robot is busy");
        return;
    }

    robotState->currentX = START_X_MM;
    robotState->currentY = START_Y_MM;
    server.send(200, "text/plain", "Software position reset to safe center home");
}

void WebServerManager::handleServo() {
    if (!server.hasArg("pos")) {
        server.send(400, "text/plain", "Missing pos parameter");
        return;
    }

    int pos = server.arg("pos").toInt();
    if (pos == 0) {
        servoController->penUp();
        robotState->penDown = false;
        server.send(200, "text/plain", "Pen UP");
    } else if (pos == 1) {
        servoController->penDown();
        robotState->penDown = true;
        server.send(200, "text/plain", "Pen DOWN");
    } else {
        server.send(400, "text/plain", "Position must be 0 or 1");
    }
}

void WebServerManager::handleStop() {
    gcodeExecutor->stop();
    motorController->requestStop();
    robotState->stopRequested = true;
    server.send(200, "text/plain", "Stop requested");
}

void WebServerManager::handleStatus() {
    String status = "{";
    status += "\"state\":\"";
    if (robotState->isExecuting) status += "executing";
    else if (motorController->isBusy()) status += "moving";
    else status += "ready";
    status += "\",";
    status += "\"pen\":\"";
    status += (robotState->penDown ? "down" : "up");
    status += "\",";
    status += "\"x\":" + String(robotState->currentX, 2) + ",";
    status += "\"y\":" + String(robotState->currentY, 2) + ",";
    status += "\"current\":" + String(robotState->currentCommand) + ",";
    status += "\"total\":" + String(robotState->totalCommands) + ",";
    status += "\"stopRequested\":" + String(robotState->stopRequested ? "true" : "false");
    status += "}";

    server.send(200, "application/json", status);
}

void WebServerManager::handleProgress() {
    String progress = "{";
    progress += "\"current\":" + String(gcodeExecutor->getCurrentIndex()) + ",";
    progress += "\"total\":" + String(gcodeExecutor->getTotalCommands());
    progress += "}";

    server.send(200, "application/json", progress);
}

void WebServerManager::handleGCodeStatus() {
    server.send(200, "text/plain", String(gcodeExecutor->getTotalCommands()));
}

/* keep your existing getHTMLPage() here */
// ====== HTML Page (Next part) ======

/* web_html.cpp - HTML Page for Web Interface */

const char* WebServerManager::getHTMLPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>لوحة تحكم روبوت الرسم بالكابلات</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      padding: 20px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      margin: 0;
      min-height: 100vh;
    }

    .container {
      max-width: 850px;
      margin: 0 auto;
      background: rgba(255,255,255,0.96);
      padding: 30px;
      border-radius: 15px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.12);
    }

    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 2em;
    }

    .subtitle {
      color: #666;
      font-size: 1.05em;
      margin-bottom: 25px;
    }

    .control-group {
      margin: 22px 0;
      padding: 20px;
      border: 2px solid #e0e0e0;
      border-radius: 12px;
      background: rgba(249,249,249,0.85);
    }

    .control-group h3 {
      color: #444;
      margin-bottom: 15px;
      font-size: 1.25em;
    }

    input, button {
      padding: 12px 16px;
      margin: 8px;
      font-size: 16px;
      border: 2px solid #ddd;
      border-radius: 8px;
      transition: all 0.25s ease;
    }

    input {
      width: 110px;
      text-align: center;
    }

    input[type="file"] {
      width: auto;
      background: #f9f9f9;
      max-width: 100%;
    }

    button {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      cursor: pointer;
      min-width: 140px;
      font-weight: bold;
    }

    button:hover {
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(0,0,0,0.18);
    }

    .stop-btn {
      background: linear-gradient(135deg, #ff6b6b 0%, #ee5a24 100%);
    }

    .servo-btn {
      background: linear-gradient(135deg, #feca57 0%, #ff9ff3 100%);
      color: #222;
    }

    .gcode-btn {
      background: linear-gradient(135deg, #26de81 0%, #20bf6b 100%);
    }

    .upload-area {
      border: 2px dashed #bbb;
      border-radius: 10px;
      padding: 20px;
      margin: 15px 0;
      background: rgba(255,255,255,0.6);
    }

    .status {
      margin: 20px 0;
      padding: 15px;
      background: linear-gradient(135deg, #a8edea 0%, #fed6e3 100%);
      border-radius: 10px;
      border: 2px solid #e0e0e0;
      line-height: 1.9;
      font-size: 1.05em;
    }

    .gcode-info {
      background: rgba(38, 222, 129, 0.1);
      border: 2px solid #26de81;
      border-radius: 10px;
      padding: 15px;
      margin: 15px 0;
      line-height: 1.8;
    }

    .progress-bar {
      width: 100%;
      height: 20px;
      background: #ddd;
      border-radius: 10px;
      overflow: hidden;
      margin: 10px 0;
    }

    .progress-fill {
      height: 100%;
      background: linear-gradient(135deg, #26de81 0%, #20bf6b 100%);
      width: 0%;
      transition: width 0.3s ease;
    }

    .direction-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      max-width: 320px;
      margin: 0 auto;
    }

    .note {
      color: #666;
      font-size: 0.95em;
      margin-top: 8px;
    }

    #response {
      margin-top: 20px;
      font-weight: bold;
    }

    .inline-label {
      display: inline-block;
      min-width: 80px;
    }

    @media (max-width: 600px) {
      .container {
        padding: 18px;
      }

      button {
        min-width: 120px;
        font-size: 15px;
      }

      input {
        width: 90px;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🤖 لوحة تحكم روبوت الرسم بالكابلات</h1>
    <div class="subtitle">روبوت رسم 4 نقاط يدعم ملفات G-code</div>

    <div class="control-group">
      <h3>📁 رفع ملف الرسم</h3>
      <div class="upload-area">
        <form method="POST" action="/upload" enctype="multipart/form-data" id="uploadForm">
          <input type="file" name="gcode" accept=".gcode,.nc,.txt" id="fileInput">
          <br><br>
          <button type="submit" class="gcode-btn">رفع ملف G-code</button>
        </form>
        <div class="note">اختر ملف الرسم ثم اضغط رفع</div>
      </div>

      <div id="gcodeInfo" class="gcode-info" style="display: none;">
        <div><strong>عدد الأوامر المحمّلة:</strong> <span id="commandCount">0</span></div>
        <div><strong>معامل التكبير:</strong> 2.0x</div>
        <div class="progress-bar">
          <div class="progress-fill" id="progressFill"></div>
        </div>
        <div id="progressText">0 / 0 أمر</div>
      </div>

      <div style="margin: 15px 0;">
        <button class="gcode-btn" onclick="executeGCode()">▶ بدء التنفيذ</button>
        <button class="stop-btn" onclick="stopRobot()">⏹ إيقاف</button>
        <button onclick="clearGCode()">🗑 مسح الملف</button>
      </div>
    </div>

    <div class="control-group">
      <h3>🎮 التحكم اليدوي</h3>

      <div style="margin: 15px 0;">
        <span class="inline-label">الزاوية:</span>
        <input type="number" id="angle" value="90" min="0" max="360" step="1">
        <span>درجة</span>
      </div>

      <div style="margin: 15px 0;">
        <span class="inline-label">عدد الخطوات:</span>
        <input type="number" id="repeats" value="1" min="1" max="20">
        <span>مرة</span>
      </div>

      <button onclick="moveRobot()">تحريك الروبوت</button>
      <div class="note">مثال: 0 يمين - 90 أعلى - 180 يسار - 270 أسفل</div>
    </div>

    <div class="control-group">
      <h3>🧭 اتجاهات سريعة</h3>
      <div class="direction-grid">
        <button onclick="quickMove(135)">↖ أعلى يسار</button>
        <button onclick="quickMove(90)">⬆ أعلى</button>
        <button onclick="quickMove(45)">↗ أعلى يمين</button>
        <button onclick="quickMove(180)">⬅ يسار</button>
        <button onclick="homeRobot()">🏠 تصفير الموقع</button>
        <button onclick="quickMove(0)">➡ يمين</button>
        <button onclick="quickMove(225)">↙ أسفل يسار</button>
        <button onclick="quickMove(270)">⬇ أسفل</button>
        <button onclick="quickMove(315)">↘ أسفل يمين</button>
      </div>
    </div>

    <div class="control-group">
      <h3>✏️ التحكم في القلم</h3>
      <button class="servo-btn" onclick="setServo(1)">إنزال القلم ⬇</button>
      <button class="servo-btn" onclick="setServo(0)">رفع القلم ⬆</button>
    </div>

    <div class="status">
      <div><strong>حالة الروبوت:</strong> <span id="robotStatus">جاهز</span></div>
      <div><strong>حالة القلم:</strong> <span id="servoStatus">مرفوع</span></div>
      <div><strong>الموضع الحالي:</strong> X=<span id="posX">0</span> مم ، Y=<span id="posY">0</span> مم</div>
      <div><strong>ملف الرسم:</strong> <span id="totalCommands">0</span> أمر محمّل</div>
    </div>

    <div id="response"></div>
  </div>

  <script>
    document.getElementById('uploadForm').addEventListener('submit', function(e) {
      e.preventDefault();
      const formData = new FormData();
      const fileInput = document.getElementById('fileInput');

      if (fileInput.files.length === 0) {
        alert('من فضلك اختر ملف G-code أولًا');
        return;
      }

      formData.append('gcode', fileInput.files[0]);
      showMessage('جاري رفع الملف...', 'info');

      fetch('/upload', {
        method: 'POST',
        body: formData
      })
      .then(response => response.text())
      .then(data => {
        showMessage(data, 'success');
        updateStatus();
        setTimeout(updateGCodeInfo, 500);
      })
      .catch(error => showMessage('خطأ أثناء الرفع: ' + error, 'error'));
    });

    function executeGCode() {
      fetch('/execute')
        .then(response => response.text())
        .then(data => {
          showMessage(data, 'success');
          startProgressMonitoring();
        })
        .catch(error => showMessage('حدث خطأ: ' + error, 'error'));
    }

    function clearGCode() {
      fetch('/clear')
        .then(response => response.text())
        .then(data => {
          showMessage(data, 'info');
          document.getElementById('gcodeInfo').style.display = 'none';
          document.getElementById('progressFill').style.width = '0%';
          document.getElementById('progressText').innerText = '0 / 0 أمر';
          updateStatus();
        });
    }

    function moveRobot() {
      const angle = document.getElementById('angle').value;
      const repeats = document.getElementById('repeats').value;

      if (!angle || !repeats) {
        alert('من فضلك أدخل الزاوية وعدد مرات الحركة');
        return;
      }

      fetch(`/move?angle=${angle}&repeats=${repeats}`)
        .then(response => response.text())
        .then(data => {
          showMessage(data, 'info');
          updateStatus();
        })
        .catch(error => showMessage('حدث خطأ: ' + error, 'error'));
    }

    function quickMove(angle) {
      document.getElementById('angle').value = angle;
      document.getElementById('repeats').value = 1;
      moveRobot();
    }

    function homeRobot() {
      fetch('/home')
        .then(response => response.text())
        .then(data => {
          showMessage(data, 'info');
          updateStatus();
        })
        .catch(error => showMessage('حدث خطأ: ' + error, 'error'));
    }

    function setServo(pos) {
      fetch(`/servo?pos=${pos}`)
        .then(response => response.text())
        .then(data => {
          showMessage(data, 'info');
          updateStatus();
        })
        .catch(error => showMessage('حدث خطأ: ' + error, 'error'));
    }

    function stopRobot() {
      fetch('/stop')
        .then(response => response.text())
        .then(data => {
          showMessage(data, 'error');
          updateStatus();
        })
        .catch(error => showMessage('حدث خطأ: ' + error, 'error'));
    }

  function updateStatus() 
{
  fetch('/status')
    .then(response => response.json())
    .then(data => {
      document.getElementById('robotStatus').innerText = data.state;
      document.getElementById('servoStatus').innerText = data.pen.toUpperCase();
      document.getElementById('posX').innerText = Number(data.x).toFixed(1);
      document.getElementById('posY').innerText = Number(data.y).toFixed(1);
      document.getElementById('totalCommands').innerText = data.total;
    });
}

    function updateGCodeInfo() {
      fetch('/gcode-status')
        .then(response => response.text())
        .then(data => {
          const count = parseInt(data);
          if (count > 0) {
            document.getElementById('commandCount').innerText = count;
            document.getElementById('totalCommands').innerText = count;
            document.getElementById('gcodeInfo').style.display = 'block';
          } else {
            document.getElementById('commandCount').innerText = 0;
            document.getElementById('totalCommands').innerText = 0;
            document.getElementById('gcodeInfo').style.display = 'none';
          }
        });
    }

function startProgressMonitoring() {
  const progressInterval = setInterval(() => {
    fetch('/progress')
      .then(response => response.json())
      .then(data => {
        const current = parseInt(data.current);
        const total = parseInt(data.total);
        const percentage = total > 0 ? (current / total) * 100 : 0;

        document.getElementById('progressFill').style.width = percentage + '%';
        document.getElementById('progressText').innerText = current + ' / ' + total + ' commands';

        if (current >= total && total > 0) {
          clearInterval(progressInterval);
          updateStatus();
        }
      })
      .catch(() => clearInterval(progressInterval));
  }, 500);
}

    function showMessage(msg, type) {
      const colors = {
        success: 'rgba(38,222,129,0.2)',
        error: 'rgba(255,107,107,0.2)',
        info: 'rgba(102,126,234,0.2)'
      };

      document.getElementById('response').innerHTML =
        `<div style="color:#333; padding:12px; border-radius:8px; background:${colors[type] || colors.info}">
          ${msg}
        </div>`;
    }

    setInterval(updateStatus, 3000);
    updateStatus();
    updateGCodeInfo();
  </script>
</body>
</html>
)rawliteral";
}