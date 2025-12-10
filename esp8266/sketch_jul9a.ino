#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define SD_CS D8
#define USER_FILE "/users.txt"
#define WIFI_FILE "/wifi.txt"

ESP8266WebServer server(80);
File uploadFile;

// Global variables for WiFi credentials
String ssid = "ESP8266_SD";    // Default SSID
String password = "12345678";  // Default password

// Global variables for current user
String currentUsername = "";
String currentRole = "";

// Common CSS
const String commonCSS = R"rawliteral(
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      margin: 0;
      padding: 20px;
      background: #f4f4f9;
      color: #333;
      transition: all 0.3s ease;
    }
    .dark { background: #1e1e2f; color: #e0e0e0; }
    .dark .card { background: #2c2c3e; border-color: #444; }
    .dark a { color: #60a5fa; }
    .dark a:hover { color: #93c5fd; }
    .dark .btn { background: #3b82f6; }
    .dark .btn:hover { background: #2563eb; }
    .dark input[type="file"], .dark input[type="text"], .dark input[type="password"], .dark textarea {
      background: #333344; border-color: #555; color: #e0e0e0;
    }
    .container { max-width: 900px; margin: 0 auto; }
    h1, h2 { margin-bottom: 1.2rem; font-weight: 600; }
    nav { margin-bottom: 1.5rem; }
    nav ol { display: flex; flex-wrap: wrap; padding: 0; list-style: none; font-size: 0.9rem; color: #666; }
    nav a { color: #007bff; text-decoration: none; margin-right: 0.5rem; }
    nav a:hover { text-decoration: underline; }
    .card { background: #fff; border: 1px solid #e0e0e0; border-radius: 10px; padding: 1.5rem; margin-bottom: 1.5rem; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
    ul { padding: 0; margin: 0; }
    li { display: flex; justify-content: space-between; align-items: center; padding: 0.75rem; border-radius: 6px; margin-bottom: 0.5rem; background: #fafafa; transition: background 0.2s; }
    li:hover { background: #e8ecef; }
    .dark li { background: #333344; }
    .dark li:hover { background: #3e3e5e; }
    .file-name {
      display: flex;
      align-items: center;
      flex: 1;
      min-width: 0;
      gap: 0.5rem;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .file-name span {
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .file-actions { display: flex; gap: 0.5rem; align-items: center; flex-wrap: wrap; justify-content: flex-end; }
    .file-actions a, .file-actions button { min-width: 80px; text-align: center; flex-shrink: 0; }
    a { color: #007bff; text-decoration: none; font-size: 0.9rem; }
    a:hover { color: #0056b3; }
    form { display: flex; flex-direction: column; gap: 0.75rem; }
    input[type="file"], input[type="text"], input[type="password"], textarea {
      padding: 0.75rem; border: 1px solid #ccc; border-radius: 6px; width: 100%; box-sizing: border-box; background: #fff; color: #333; font-size: 0.9rem;
    }
    .btn { padding: 0.75rem 1.5rem; background: #007bff; color: #fff; border: none; border-radius: 6px; cursor: pointer; font-size: 0.9rem; font-weight: 500; transition: background 0.2s; box-sizing: border-box; }
    .btn:hover { background: #0056b3; }
    .btn-danger { background: #dc3545; }
    .btn-danger:hover { background: #c82333; }
    .btn-success { background: #28a745; }
    .btn-success:hover { background: #218838; }
    .btn-warning { background: #ffc107; }
    .btn-warning:hover { background: #e0a800; }
    .flex { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 1rem; }
    .grid { display: grid; grid-template-columns: 1fr; gap: 1.5rem; }
    @media (min-width: 600px) { .grid { grid-template-columns: 1fr 1fr; } }
    .alert { padding: 1rem; border-radius: 6px; margin-bottom: 1rem; background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
    .alert-error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
    .full-width { width: 100%; }
  </style>
)rawliteral";

// Common JavaScript
const String commonJS = R"rawliteral(
  <script>
    const themeToggle = document.getElementById('themeToggle');
    const body = document.body;
    const currentTheme = localStorage.getItem('theme') || (window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');
    
    if (currentTheme === 'dark') {
      body.classList.add('dark');
      themeToggle.textContent = 'Light Theme';
    } else {
      themeToggle.textContent = 'Dark Theme';
    }

    themeToggle.addEventListener('click', () => {
      body.classList.toggle('dark');
      const newTheme = body.classList.contains('dark') ? 'dark' : 'light';
      localStorage.setItem('theme', newTheme);
      themeToggle.textContent = newTheme === 'dark' ? 'Light Theme' : 'Dark Theme';
    });
  </script>
)rawliteral";

// Prototypes
void handleFileUpload();
bool deleteDirectory(String path);
void handleUARTCommands();
void sendRedirectPage(String message, String redirectUrl, int delay = 2000);
String getStorageInfo();
unsigned long getDirectorySize(String path);
String getFilesInfo();
String getUptime();
String getMemoryInfo();
String getWiFiStatus();
void handleLogin();
void handleLoginPost();
void handleRegister();
void handleRegisterPost();
void handleLogout();
void handleAdminPage();
void handleUserPage();
void handleWiFiSettings();
void handleWiFiSettingsPost();
void showFileManager(String path, String username, String role, String routePrefix);
void loadWiFiCredentials();

// Setup
void setup() {
  Serial.begin(921600);
  delay(2000);
  Serial.println("\nStarting...");
  randomSeed(analogRead(A0));

  if (!SD.begin(SD_CS)) {
    Serial.println("Failed to initialize SD card!");
    return;
  }
  Serial.println("SD card ready!");

  // Load WiFi credentials
  loadWiFiCredentials();

  if (!SD.exists(USER_FILE)) {
    Serial.println("users.txt not found. Creating a new one with a default admin account.");
    File usersFile = SD.open(USER_FILE, FILE_WRITE);
    if (usersFile) {
      usersFile.println("admin,123456,admin");
      usersFile.close();
      Serial.println("Default admin account created: admin/123456");
    } else {
      Serial.println("Failed to create users.txt file!");
      return;
    }
  }

  WiFi.softAP(ssid.c_str(), password.c_str());
  Serial.print("WiFi AP: ");
  Serial.println(WiFi.softAPIP());

  // Public routes
  server.on("/login", HTTP_GET, handleLogin);
  server.on("/login", HTTP_POST, handleLoginPost);
  server.on("/register", HTTP_GET, handleRegister);
  server.on("/register", HTTP_POST, handleRegisterPost);
  server.on("/logout", HTTP_GET, handleLogout);

  // WiFi settings route (admin only)
  server.on("/wifi", HTTP_GET, handleWiFiSettings);
  server.on("/wifi", HTTP_POST, handleWiFiSettingsPost);
  server.on("/wifi/confirm", HTTP_POST, handleWiFiConfirm);  // New route
  // Landing route
  server.on("/", HTTP_GET, []() {
    if (currentUsername == "") {
      sendRedirectPage("You need to log in to access this page.", "/login", 1500);
      return;
    }
    server.sendHeader("Location", currentRole == "admin" ? "/admin" : "/user");
    server.send(302, "text/plain", "");
  });

  // Role-specific file manager pages
  server.on("/admin", HTTP_GET, handleAdminPage);
  server.on("/user", HTTP_GET, handleUserPage);

  // Protected actions
  server.on(
    "/upload", HTTP_POST, []() {
      if (currentUsername == "") {
        sendRedirectPage("You need to log in to access this page.", "/login", 2000);
        return;
      }
      sendRedirectPage("Upload completed.", currentRole == "admin" ? "/admin" : "/user", 1000);
    },
    handleFileUpload);

  server.on("/delete", HTTP_GET, []() {
    if (currentUsername == "") {
      sendRedirectPage("You need to log in to access this page.", "/login", 1500);
      return;
    }
    if (!server.hasArg("file")) {
      server.send(400, "text/plain", "Missing ?file=");
      return;
    }
    String filename = server.arg("file");
    if (currentRole != "admin" && !filename.startsWith("/" + currentUsername)) {
      sendRedirectPage("You do not have permission to delete this file.", currentRole == "admin" ? "/admin" : "/user", 2000);
      return;
    }
    if (!SD.exists(filename)) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    SD.remove(filename);
    sendRedirectPage("File deleted successfully!", currentRole == "admin" ? "/admin" : "/user", 1000);
  });

  server.on("/deleteDir", HTTP_GET, []() {
    if (currentUsername == "") {
      sendRedirectPage("You need to log in to access this page.", "/login", 1500);
      return;
    }
    if (!server.hasArg("dir")) {
      server.send(400, "text/plain", "Missing ?dir=");
      return;
    }
    String dirPath = server.arg("dir");
    if (currentRole != "admin" && !dirPath.startsWith("/" + currentUsername)) {
      sendRedirectPage("You do not have permission to delete this directory.", currentRole == "admin" ? "/admin" : "/user", 2000);
      return;
    }
    if (!SD.exists(dirPath)) {
      server.send(404, "text/plain", "Directory not found");
      return;
    }
    if (deleteDirectory(dirPath)) {
      sendRedirectPage("Directory deleted successfully!", currentRole == "admin" ? "/admin" : "/user", 1000);
    } else {
      sendRedirectPage("Failed to delete directory.", currentRole == "admin" ? "/admin" : "/user", 2000);
    }
  });

  server.on("/view", HTTP_GET, []() {
    if (currentUsername == "") {
      sendRedirectPage("You need to log in.", "/login", 1500);
      return;
    }
    if (!server.hasArg("file")) {
      server.send(400, "text/plain", "Missing ?file=");
      return;
    }
    String filename = server.arg("file");
    if (filename.indexOf("..") != -1) {
      server.send(403, "text/plain", "Invalid file path");
      return;
    }
    if (currentRole != "admin" && !filename.startsWith("/" + currentUsername)) {
      sendRedirectPage("You do not have permission to view this file.", currentRole == "admin" ? "/admin" : "/user", 2000);
      return;
    }
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    if (!SD.exists(filename)) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    File f = SD.open(filename, FILE_READ);
    if (!f || f.isDirectory()) {
      server.send(500, "text/plain", "Failed to open file");
      return;
    }
    const size_t MAX_FILE_SIZE_TO_VIEW = 8192;
    if (f.size() > MAX_FILE_SIZE_TO_VIEW) {
      String downloadLink = "/download?file=" + String(filename);
      String message = "File is too large to view. Please <a href=\"" + downloadLink + "\">download</a> it instead.";
      server.send(200, "text/html", message);
    } else {
      server.streamFile(f, "text/plain");
    }
    f.close();
  });

  server.on("/download", HTTP_GET, []() {
    if (currentUsername == "") {
      sendRedirectPage("You need to log in.", "/login", 1500);
      return;
    }
    if (!server.hasArg("file")) {
      server.send(400, "text/plain", "Missing ?file=");
      return;
    }
    String filename = server.arg("file");
    if (filename.indexOf("..") != -1) {
      server.send(403, "text/plain", "Invalid file path");
      return;
    }
    if (currentRole != "admin" && !filename.startsWith("/" + currentUsername)) {
      sendRedirectPage("You do not have permission to download this file.", currentRole == "admin" ? "/admin" : "/user", 2000);
      return;
    }
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    if (!SD.exists(filename)) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    File downloadFile = SD.open(filename, FILE_READ);
    if (!downloadFile) {
      server.send(500, "text/plain", "Could not open file.");
      return;
    }
    String contentDisposition = String("attachment; filename=\"") + downloadFile.name() + "\"";
    server.sendHeader("Content-Disposition", contentDisposition);
    server.streamFile(downloadFile, "application/octet-stream");
    downloadFile.close();
  });

  server.on("/new", HTTP_GET, []() {
    if (currentUsername == "") {
      sendRedirectPage("You need to log in to access this page.", "/login", 1500);
      return;
    }
    String form = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Create New File</title>
      )rawliteral" + commonCSS
                  + R"rawliteral(
      </head>
      <body>
        <div class="container">
          <div class="flex">
            <h1>Create New .txt File</h1>
            <button id="themeToggle" class="btn">Toggle Theme</button>
          </div>
          <div class="card">
            <form method="POST" action="/save">
              <div>
                <label for="filename">Filename (full path)</label>
                <input id="filename" name="filename" value="/)rawliteral"
                  + currentUsername + R"rawliteral(/newfile.txt">
              </div>
              <div>
                <label for="content">Content</label>
                <textarea id="content" name="content" rows="10">Enter content here...</textarea>
              </div>
              <button type="submit" class="btn btn-success">Create</button>
            </form>
          </div>
        </div>
      )rawliteral" + commonJS
                  + R"rawliteral(
      </body>
      </html>
    )rawliteral";
    server.send(200, "text/html", form);
  });

  server.on("/edit", HTTP_GET, []() {
    if (currentUsername == "") {
      sendRedirectPage("You need to log in to access this page.", "/login", 1500);
      return;
    }
    if (!server.hasArg("file")) {
      server.send(400, "text/plain", "Missing ?file=");
      return;
    }
    String filename = server.arg("file");
    if (filename.indexOf("..") != -1) {
      server.send(403, "text/plain", "Invalid file path");
      return;
    }
    if (currentRole != "admin" && !filename.startsWith("/" + currentUsername)) {
      sendRedirectPage("You do not have permission to edit this file.", currentRole == "admin" ? "/admin" : "/user", 2000);
      return;
    }
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    if (!SD.exists(filename)) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    File f = SD.open(filename, FILE_READ);
    if (!f || f.isDirectory()) {
      server.send(500, "text/plain", "Failed to open file");
      return;
    }
    const size_t MAX_FILE_SIZE_TO_VIEW = 8192;
    if (f.size() > MAX_FILE_SIZE_TO_VIEW) {
      String downloadLink = "/download?file=" + String(filename);
      String message = "File is too large to edit. Please <a href=\"" + downloadLink + "\">download</a> it instead.";
      server.send(200, "text/html", message);
      f.close();
      return;
    }
    String content = "";
    while (f.available()) content += (char)f.read();
    f.close();

    String form = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Edit )rawliteral"
                  + filename + R"rawliteral(</title>
    )rawliteral" + commonCSS
                  + R"rawliteral(
    </head>
    <body>
      <div class="container">
        <div class="flex">
          <h1>Edit )rawliteral"
                  + filename + R"rawliteral(</h1>
          <button id="themeToggle" class="btn">Toggle Theme</button>
        </div>
        <div class="card">
          <form method="POST" action="/save">
            <input type="hidden" name="filename" value=")rawliteral"
                  + filename + R"rawliteral(">
            <div>
              <label for="content">Content</label>
              <textarea id="content" name="content" rows="20">)rawliteral"
                  + content + R"rawliteral(</textarea>
            </div>
            <button type="submit" class="btn btn-success">Save</button>
          </form>
        </div>
      </div>
    )rawliteral" + commonJS
                  + R"rawliteral(
    </body>
    </html>
  )rawliteral";
    server.send(200, "text/html", form);
  });

  server.on("/save", HTTP_POST, []() {
    if (currentUsername == "") {
      sendRedirectPage("You need to log in to access this page.", "/login", 1500);
      return;
    }
    if (!server.hasArg("filename") || !server.hasArg("content")) {
      server.send(400, "text/plain", "Missing filename or content");
      return;
    }
    String filename = server.arg("filename");
    String content = server.arg("content");
    if (currentRole != "admin" && !filename.startsWith("/" + currentUsername)) {
      sendRedirectPage("You do not have permission to save this file.", currentRole == "admin" ? "/admin" : "/user", 2000);
      return;
    }
    if (SD.exists(filename)) SD.remove(filename);
    File f = SD.open(filename, FILE_WRITE);
    if (!f) {
      sendRedirectPage("Failed to save file.", currentRole == "admin" ? "/admin" : "/user", 2000);
      return;
    }
    f.print(content);
    f.close();
    sendRedirectPage("File saved successfully!", currentRole == "admin" ? "/admin" : "/user", 1000);
  });

  server.begin();
  Serial.println("Server started.");
}

void loop() {
  server.handleClient();
  handleUARTCommands();
}

// Load WiFi credentials from SD card
void loadWiFiCredentials() {
  File wifiFile = SD.open(WIFI_FILE, FILE_READ);
  if (wifiFile) {
    String line = wifiFile.readStringUntil('\n');
    wifiFile.close();
    line.trim();
    int commaIndex = line.indexOf(',');
    if (commaIndex != -1) {
      ssid = line.substring(0, commaIndex);
      password = line.substring(commaIndex + 1);
      ssid.trim();
      password.trim();
      Serial.println("Loaded WiFi credentials: SSID=" + ssid + ", Password=" + password);
    } else {
      Serial.println("Invalid format in wifi.txt, using default credentials.");
    }
  } else {
    Serial.println("wifi.txt not found, creating with default credentials.");
    wifiFile = SD.open(WIFI_FILE, FILE_WRITE);
    if (wifiFile) {
      wifiFile.println(ssid + "," + password);
      wifiFile.close();
      Serial.println("Created wifi.txt with default credentials.");
    } else {
      Serial.println("Failed to create wifi.txt, using default credentials.");
    }
  }
}

// Page renderers
void showFileManager(String path, String username, String role, String routePrefix) {
  if (!path.startsWith("/")) path = "/" + path;
  File root = SD.open(path);
  if (!root || !root.isDirectory()) {
    server.send(500, "text/plain", "Invalid directory");
    return;
  }

  String uploadDir = path;
  if (role != "admin" && !path.startsWith("/" + username)) {
    uploadDir = "/" + username;
  }

  // Start streaming response
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  // Send static HTML header
  server.sendContent(F(R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>ESP8266 SD File Manager</title>
  )rawliteral"));
  server.sendContent(commonCSS);
  server.sendContent(F("</head><body><div class='container'>"));

  // Send dynamic content
  server.sendContent("<div class='flex'><h1>SD Card File Manager</h1><div>");
  server.sendContent("<a href='/logout' class='btn btn-danger'>Logout</a>");
  server.sendContent("<button id='themeToggle' class='btn'>Toggle Theme</button></div></div>");
  server.sendContent("<p>Hello, " + username + " (" + role + ")!</p>");

  // Breadcrumbs
  server.sendContent("<nav><ol><li><a href='" + routePrefix + "?dir=/'>Home</a></li>");
  if (path != "/") {
    String breadcrumbPath = "";
    String segments[16];
    int segmentCount = 0;
    int lastIndex = 1;
    while (lastIndex < (int)path.length()) {
      int nextSlash = path.indexOf('/', lastIndex);
      if (nextSlash == -1) nextSlash = path.length();
      segments[segmentCount++] = path.substring(lastIndex, nextSlash);
      lastIndex = nextSlash + 1;
    }
    for (int i = 0; i < segmentCount; i++) {
      breadcrumbPath += "/" + segments[i];
      server.sendContent("<li><span>/</span><a href='" + routePrefix + "?dir=" + breadcrumbPath + "'>" + segments[i] + "</a></li>");
    }
  }
  server.sendContent("</ol></nav>");

  // File list
  server.sendContent("<div class='card'><h2>Files in " + path + "</h2><ul>");
  if (path != "/") {
    int lastSlash = path.lastIndexOf('/');
    String parent = path.substring(0, lastSlash);
    if (parent == "") parent = "/";
    server.sendContent("<li><div class='file-name'><span></span> <a href='" + routePrefix + "?dir=" + parent + "'>Back to " + parent + "</a></div></li>");
  }

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    String name = entry.name();
    String fullPath = name;
    if (!fullPath.startsWith("/")) {
      if (path == "/") fullPath = "/" + name;
      else fullPath = path + "/" + name;
    }

    server.sendContent("<li>");
    if (entry.isDirectory()) {
      server.sendContent("<div class='file-name'><span></span> <a href='" + routePrefix + "?dir=" + fullPath + "'>" + name + "</a></div>");
      server.sendContent("<div class='file-actions'><a href='/deleteDir?dir=" + fullPath + "' onclick=\"return confirm('Delete directory " + fullPath + " and all its contents?');\" class='btn btn-danger'>Delete</a></div>");
    } else {
      server.sendContent("<div class='file-name'><span></span> " + name + " (" + String(entry.size()) + " bytes)</div>");
      server.sendContent("<div class='file-actions'>");
      server.sendContent("<a href='/view?file=" + fullPath + "' class='btn'>View</a>");
      server.sendContent("<a href='/download?file=" + fullPath + "' class='btn btn-success'>Download</a>");
      if (name.endsWith(".txt")) {
        server.sendContent("<a href='/edit?file=" + fullPath + "' class='btn btn-warning'>Edit</a>");
      }
      server.sendContent("<a href='/delete?file=" + fullPath + "' onclick=\"return confirm('Delete this file?');\" class='btn btn-danger'>Delete</a>");
      server.sendContent("</div>");
    }
    server.sendContent("</li>");
    entry.close();
  }
  server.sendContent("</ul></div>");

  // WiFi settings for admin
  if (role == "admin") {
    server.sendContent("<div class='card'><h2>WiFi Settings</h2><a href='/wifi' class='btn btn-warning'>Configure WiFi</a></div>");
  }

  // Upload and create file forms
  server.sendContent(R"rawliteral(
    <div class='grid'>
      <div class='card'>
        <h2>Upload File to )rawliteral"
                     + uploadDir + R"rawliteral(</h2>
        <form method='POST' action='/upload' enctype='multipart/form-data' onsubmit='return validateUpload()'>
          <input type='file' name='upload' id='uploadInput'>
          <input type='hidden' name='dir' value=')rawliteral"
                     + uploadDir + R"rawliteral('>
          <button type='submit' class='btn'>Upload</button>
        </form>
      </div>
      <div class='card'>
        <h2>Create New File</h2>
        <a href='/new' class='btn btn-success'>Create .txt File</a>
      </div>
    </div>
  )rawliteral");

  // JavaScript
  server.sendContent(commonJS);
  server.sendContent(R"rawliteral(
    <script>
      function validateUpload() {
        const fileInput = document.getElementById('uploadInput');
        if (!fileInput.files.length) {
          alert('Please select a file to upload.');
          return false;
        }
        return true;
      }
    </script>
  </div></body></html>
  )rawliteral");

  server.sendContent("");  // Finalize streaming
  root.close();
}

void handleAdminPage() {
  if (currentUsername == "") {
    Serial.println("Admin page access denied: No user logged in.");
    sendRedirectPage("You need to log in to access the admin page.", "/login", 2000);
    return;
  }
  if (currentRole != "admin") {
    Serial.println("Admin page access denied: User " + currentUsername + " is not an admin.");
    sendRedirectPage("You do not have permission to access the admin page.", "/login", 2000);
    return;
  }
  String dir = "/";
  if (server.hasArg("dir")) {
    dir = server.arg("dir");
    if (!dir.startsWith("/")) dir = "/" + dir;
  }
  Serial.println("Accessing admin page for user: " + currentUsername + ", dir: " + dir);
  showFileManager(dir, currentUsername, currentRole, "/admin");
}

void handleUserPage() {
  if (currentUsername == "") {
    Serial.println("User page access denied: No user logged in.");
    sendRedirectPage("You need to log in to access this page.", "/login", 2000);
    return;
  }
  String baseDir = "/" + currentUsername;
  if (!SD.exists(baseDir)) {
    if (!SD.mkdir(baseDir)) {
      Serial.println("Failed to create user directory: " + baseDir);
      sendRedirectPage("Server error: Could not create user directory.", "/login", 2000);
      return;
    }
    Serial.println("Created user directory: " + baseDir);
  }

  String dir = baseDir;
  if (server.hasArg("dir")) {
    dir = server.arg("dir");
    if (!dir.startsWith(baseDir)) {
      Serial.println("User " + currentUsername + " attempted to access restricted directory: " + dir);
      dir = baseDir;  // Restrict to user's directory
    }
  }
  Serial.println("Accessing user page for user: " + currentUsername + ", dir: " + dir);
  showFileManager(dir, currentUsername, currentRole, "/user");
}

// WiFi settings page (admin only)
void handleWiFiSettings() {
  if (currentUsername == "") {
    Serial.println("WiFi settings access denied: No user logged in.");
    sendRedirectPage("You need to log in to access this page.", "/login", 2000);
    return;
  }
  if (currentRole != "admin") {
    Serial.println("WiFi settings access denied: User " + currentUsername + " is not an admin.");
    sendRedirectPage("You do not have permission to access this page.", "/login", 2000);
    return;
  }

  String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>WiFi Settings</title>
      )rawliteral"
                + commonCSS + R"rawliteral(
        <style>
          .modal {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0,0,0,0.5);
            justify-content: center;
            align-items: center;
            z-index: 1000;
          }
          .modal-content {
            background: #fff;
            padding: 2rem;
            border-radius: 10px;
            max-width: 500px;
            width: 90%;
            text-align: center;
            box-shadow: 0 4px 10px rgba(0,0,0,0.2);
          }
          .dark .modal-content {
            background: #2c2c3e;
            color: #e0e0e0;
          }
          .modal-content h2 {
            margin-top: 0;
          }
          .modal-content .btn {
            margin-top: 1rem;
          }
        </style>
      </head>
      <body>
        <div class="container">
          <div class="flex">
            <h1>WiFi Settings</h1>
            <div>
              <a href="/admin" class="btn">Back to Admin</a>
              <button id="themeToggle" class="btn">Toggle Theme</button>
            </div>
          </div>
          <div class="card">
            <h2>Update WiFi Credentials</h2>
            <form id="wifiForm" onsubmit="showConfirmModal(event)">
              <div>
                <label for="ssid">SSID</label>
                <input type="text" name="ssid" id="ssid" value=")rawliteral"
                + ssid + R"rawliteral(" required>
              </div>
              <div>
                <label for="password">Password</label>
                <input type="text" name="password" id="password" value=")rawliteral"
                + password + R"rawliteral(" required>
              </div>
              <button type="submit" class="btn btn-success full-width">Save Changes</button>
            </form>
            <p><strong>Note:</strong> Changing WiFi credentials will disconnect all clients. You will need to reconnect to the new SSID and password.</p>
          </div>
        </div>
        <div class="modal" id="confirmModal">
          <div class="modal-content">
            <h2>Confirm WiFi Changes</h2>
            <p>Please confirm the new WiFi information:</p>
            <p>SSID: <strong id="confirmSsid"></strong></p>
            <p>Password: <strong id="confirmPassword"></strong></p>
            <p><strong>Warning:</strong> This change will disconnect all devices. Please note the information to reconnect.</p>
            <button onclick="applyWiFiChanges()" class="btn btn-success">Confirm and Apply</button>
            <button onclick="closeModal()" class="btn btn-danger">Cancel</button>
          </div>
        </div>
      )rawliteral"
                + commonJS + R"rawliteral(
        <script>
          function showConfirmModal(event) {
            event.preventDefault();
            const ssid = document.getElementById('ssid').value.trim();
            const password = document.getElementById('password').value.trim();
            if (ssid.length < 1 || ssid.length > 32) {
              alert('SSID must be between 1 and 32 characters.');
              return;
            }
            if (password.length < 8 || password.length > 63) {
              alert('Password must be between 8 and 63 characters.');
              return;
            }
            document.getElementById('confirmSsid').textContent = ssid;
            document.getElementById('confirmPassword').textContent = password;
            document.getElementById('confirmModal').style.display = 'flex';
          }

          function closeModal() {
            document.getElementById('confirmModal').style.display = 'none';
          }

          function applyWiFiChanges() {
            const ssid = document.getElementById('ssid').value.trim();
            const password = document.getElementById('password').value.trim();
            const formData = new FormData();
            formData.append('ssid', ssid);
            formData.append('password', password);
            fetch('/wifi/confirm', {
              method: 'POST',
              body: formData
            }).then(response => {
              if (response.ok) {
                alert('WiFi change request sent. Connection will be disconnected. Please reconnect with SSID: ' + ssid);
              } else {
                alert('Error applying WiFi changes.');
              }
            }).catch(() => {
              alert('Connection error. Please try again.');
            });
            document.getElementById('confirmModal').style.display = 'none';
          }
        </script>
      </body>
      </html>
    )rawliteral";
  server.send(200, "text/html", html);
}

// Handle WiFi settings form submission
void handleWiFiSettingsPost() {
  if (currentUsername == "") {
    Serial.println("WiFi settings update denied: No user logged in.");
    sendRedirectPage("You need to log in to access this page.", "/login", 2000);
    return;
  }
  if (currentRole != "admin") {
    Serial.println("WiFi settings update denied: User " + currentUsername + " is not an admin.");
    sendRedirectPage("You do not have permission to access this page.", "/login", 2000);
    return;
  }

  if (!server.hasArg("ssid") || !server.hasArg("password")) {
    Serial.println("WiFi settings update failed: Missing SSID or password.");
    sendRedirectPage("Missing SSID or password.", "/wifi", 2000);
    return;
  }

  String newSsid = server.arg("ssid");
  String newPassword = server.arg("password");
  newSsid.trim();
  newPassword.trim();

  // Validate inputs
  if (newSsid.length() < 1 || newSsid.length() > 32) {
    Serial.println("WiFi settings update failed: Invalid SSID length.");
    sendRedirectPage("SSID must be between 1 and 32 characters.", "/wifi", 2000);
    return;
  }
  if (newPassword.length() < 8 || newPassword.length() > 63) {
    Serial.println("WiFi settings update failed: Invalid password length.");
    sendRedirectPage("Password must be between 8 and 63 characters.", "/wifi", 2000);
    return;
  }

  // Save new credentials to wifi.txt
  File wifiFile = SD.open(WIFI_FILE, FILE_WRITE);
  if (!wifiFile) {
    Serial.println("Failed to open wifi.txt for writing.");
    sendRedirectPage("Server error: Could not save WiFi settings.", "/wifi", 2000);
    return;
  }
  wifiFile.println(newSsid + "," + newPassword);
  wifiFile.close();
  Serial.println("WiFi credentials updated: SSID=" + newSsid + ", Password=" + newPassword);

  // Update global variables
  ssid = newSsid;
  password = newPassword;

  // Restart WiFi AP with new credentials
  WiFi.softAPdisconnect(true);
  WiFi.softAP(ssid.c_str(), password.c_str());
  Serial.print("WiFi AP restarted with new credentials. IP: ");
  Serial.println(WiFi.softAPIP());

  sendRedirectPage("WiFi settings updated. Please reconnect with the new SSID.", "/admin", 3000);
}

// Auth pages
void handleLogin() {
  String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Login</title>
      )rawliteral"
                + commonCSS + R"rawliteral(
      </head>
      <body>
        <div class="container">
          <div class="card">
            <h2>Login</h2>
            <form method="POST" action="/login">
              <input type="text" name="username" placeholder="Username" required>
              <input type="password" name="password" placeholder="Password" required>
              <button type="submit" class="btn full-width">Login</button>
            </form>
            <p>Don't have an account? <a href="/register">Register now</a></p>
          </div>
        </div>
      )rawliteral"
                + commonJS + R"rawliteral(
      </body>
      </html>
    )rawliteral";
  server.send(200, "text/html", html);
}

void handleLoginPost() {
  if (!server.hasArg("username") || !server.hasArg("password")) {
    Serial.println("Login failed: Missing username or password.");
    sendRedirectPage("Missing username or password.", "/login", 3000);
    return;
  }

  String username = server.arg("username");
  String password = server.arg("password");
  username.trim();
  password.trim();
  Serial.print("Attempting login for user: ");
  Serial.println(username);

  File usersFile = SD.open(USER_FILE, FILE_READ);
  if (!usersFile) {
    Serial.println("Failed to open users.txt for reading.");
    sendRedirectPage("Server error: Could not open user file.", "/login", 3000);
    return;
  }

  bool loginSuccess = false;
  String role;
  char buffer[256];  // Buffer for reading file lines
  while (usersFile.available()) {
    size_t len = usersFile.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';  // Null-terminate the string
    String line = String(buffer);
    line.trim();
    if (line.length() == 0) continue;

    int c1 = line.indexOf(',');
    int c2 = line.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) {
      Serial.println("Invalid line format in users.txt: " + line);
      continue;
    }

    String u = line.substring(0, c1);
    String p = line.substring(c1 + 1, c2);
    String r = line.substring(c2 + 1);
    u.trim();
    p.trim();
    r.trim();
    Serial.print("Checking user: ");
    Serial.println(u);

    if (username == u && password == p) {
      loginSuccess = true;
      role = r;
      Serial.println("Login successful for user: " + username + ", role: " + role);
      break;
    }
  }
  usersFile.close();

  if (loginSuccess) {
    currentUsername = username;
    currentRole = role;
    server.sendHeader("Location", role == "admin" ? "/admin" : "/user");
    server.send(302, "text/plain", "");
    Serial.println("Redirecting to: " + String(role == "admin" ? "/admin" : "/user"));
  } else {
    Serial.println("Login failed: Invalid credentials for user: " + username);
    sendRedirectPage("Incorrect username or password.", "/login", 2000);
  }
}

void handleRegister() {
  String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Register</title>
      )rawliteral"
                + commonCSS + R"rawliteral(
      </head>
      <body>
        <div class="container">
          <div class="card">
            <h2>Register</h2>
            <form method="POST" action="/register">
              <input type="text" name="username" placeholder="Username" required>
              <input type="password" name="password" placeholder="Password" required>
              <button type="submit" class="btn full-width">Register</button>
            </form>
            <p>Already have an account? <a href="/login">Login</a></p>
          </div>
        </div>
      )rawliteral"
                + commonJS + R"rawliteral(
      </body>
      </html>
    )rawliteral";
  server.send(200, "text/html", html);
}

void handleRegisterPost() {
  if (!server.hasArg("username") || !server.hasArg("password")) {
    Serial.println("Register failed: Missing username or password.");
    sendRedirectPage("Missing username or password.", "/register", 3000);
    return;
  }
  String username = server.arg("username");
  String password = server.arg("password");
  username.trim();
  password.trim();
  Serial.print("Attempting to register user: ");
  Serial.println(username);

  // Check if username already exists
  File usersFile = SD.open(USER_FILE, FILE_READ);
  if (usersFile) {
    char buffer[256];
    while (usersFile.available()) {
      size_t len = usersFile.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
      buffer[len] = '\0';
      String line = String(buffer);
      line.trim();
      int c1 = line.indexOf(',');
      if (c1 < 0) continue;
      String u = line.substring(0, c1);
      if (u == username) {
        Serial.println("Register failed: Username already exists: " + username);
        sendRedirectPage("Username already exists.", "/register", 3000);
        usersFile.close();
        return;
      }
    }
    usersFile.close();
  } else {
    Serial.println("Failed to open users.txt for reading during registration check.");
    sendRedirectPage("Server error: Could not open user file.", "/register", 3000);
    return;
  }

  // Append new user to file
  File newUsersFile = SD.open(USER_FILE, FILE_WRITE);
  if (!newUsersFile) {
    Serial.println("Failed to open users.txt for appending.");
    sendRedirectPage("Server error: Could not open user file.", "/register", 3000);
    return;
  }

  // Seek to end of file to append
  newUsersFile.seek(newUsersFile.size());
  String userLine = username + "," + password + ",user\n";
  newUsersFile.print(userLine);
  newUsersFile.close();

  String userDir = "/" + username;
  SD.mkdir(userDir);
  Serial.println("Registered new user: " + username);
  sendRedirectPage("Registration successful! Please log in.", "/login", 2000);
}

void handleLogout() {
  Serial.println("Logging out user: " + currentUsername);
  currentUsername = "";
  currentRole = "";
  sendRedirectPage("You have logged out.", "/login", 1500);
}

// File upload
void handleFileUpload() {
  if (currentUsername == "") {
    sendRedirectPage("You need to log in to access this page.", "/login", 2000);
    return;
  }

  HTTPUpload &upload = server.upload();
  static String uploadDir = "/";
  if (upload.status == UPLOAD_FILE_START) {
    if (server.hasArg("dir")) {
      uploadDir = server.arg("dir");
      if (!uploadDir.startsWith("/")) uploadDir = "/" + uploadDir;
    } else if (currentRole != "admin") {
      uploadDir = "/" + currentUsername;  // Default to user's directory for non-admins
    }

    // Ensure the upload directory exists
    if (!SD.exists(uploadDir)) {
      if (!SD.mkdir(uploadDir)) {
        Serial.println("Failed to create upload directory: " + uploadDir);
        server.send(500, "text/plain", "Failed to create directory");
        return;
      }
    }

    // Check if the full path is allowed for non-admin users
    String filename = uploadDir + "/" + upload.filename;
    if (currentRole != "admin" && !filename.startsWith("/" + currentUsername)) {
      Serial.println("Permission denied for upload to: " + filename);
      server.send(403, "text/plain", "Permission denied");
      return;
    }

    Serial.print("Uploading: ");
    Serial.println(filename);
    uploadFile = SD.open(filename, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("Failed to open file for writing: " + filename);
      server.send(500, "text/plain", "Failed to open file");
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.println("Upload finished.");
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    Serial.println("Upload aborted.");
  }
}

// Utilities
bool deleteDirectory(String path) {
  File root = SD.open(path);
  if (!root || !root.isDirectory()) return false;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    String entryName = entry.name();
    String fullPath = path == "/" ? "/" + entryName : path + "/" + entryName;
    if (entry.isDirectory()) deleteDirectory(fullPath);
    else SD.remove(fullPath);
    entry.close();
  }
  root.close();
  return SD.rmdir(path);
}

void sendRedirectPage(String message, String redirectUrl, int delay) {
  String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <meta http-equiv="refresh" content=")rawliteral"
                + String(delay / 1000) + R"rawliteral(;url=)rawliteral" + redirectUrl + R"rawliteral(">
        <title>Action Completed</title>
      )rawliteral"
                + commonCSS + R"rawliteral(
      </head>
      <body>
        <div class="container">
          <div class="card">
            <div class="alert )rawliteral"
                + (message.startsWith("Failed") || message.startsWith("Missing") || message.startsWith("Incorrect") || message.startsWith("You do not") ? "alert-error" : "") + R"rawliteral(")> )rawliteral" + message + R"rawliteral(</div>
            <p>Redirecting in )rawliteral"
                + String(delay / 1000) + R"rawliteral( seconds... <a href=")rawliteral" + redirectUrl + R"rawliteral(">Click to redirect now</a></p>
          </div>
        </div>
      )rawliteral"
                + commonJS + R"rawliteral(
      </body>
      </html>
    )rawliteral";
  server.send(200, "text/html", html);
}

String getStorageInfo() {
  unsigned long usedBytes = 0;
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    return String("Storage: Error accessing SD card");
  }
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) usedBytes += entry.size();
    else usedBytes += getDirectorySize(String("/") + entry.name());
    entry.close();
  }
  root.close();
  float usedMB = (float)usedBytes / (1024.0 * 1024.0);
  unsigned long long totalBytes = 8ULL * 1024 * 1024 * 1024;
  float totalMB = (float)totalBytes / (1024.0 * 1024.0);
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "Storage: %.2fMB / %.2fMB", usedMB, totalMB);
  return String(buffer);
}

unsigned long getDirectorySize(String path) {
  unsigned long size = 0;
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return 0;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) size += entry.size();
    else size += getDirectorySize(path + "/" + entry.name());
    entry.close();
  }
  dir.close();
  return size;
}

String getFilesInfo() {
  File root = SD.open("/");
  int count = 0;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) count++;
    entry.close();
  }
  root.close();
  return String("Files: ") + String(count);
}

String getUptime() {
  unsigned long seconds = millis() / 1000;
  unsigned long hours = seconds / 3600;
  seconds %= 3600;
  unsigned long minutes = seconds / 60;
  return String("Uptime: ") + String(hours) + "h " + String(minutes) + "m";
}

String getMemoryInfo() {
  float freeHeap = (float)ESP.getFreeHeap();
  const float totalHeap = 80000.0;
  int percentage = (freeHeap / totalHeap) * 100;
  return String("Memory: ") + String(percentage) + "%";
}

String getWiFiStatus() {
  int clients = WiFi.softAPgetStationNum();
  String ip = WiFi.softAPIP().toString();
  if (clients > 0) return "Connected: " + String(clients) + " clients";
  else return "Disconnected: 0 clients";
}

void handleUARTCommands() {
  static String command = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      command.trim();
      String response = "";
      if (command == "GET_STORAGE") response = getStorageInfo();
      else if (command == "GET_FILES") response = getFilesInfo();
      else if (command == "GET_UPTIME") response = getUptime();
      else if (command == "GET_MEMORY") response = getMemoryInfo();
      else if (command == "GET_WIFI") response = getWiFiStatus();
      else if (command == "GET_WIFI_QR") response = String("WIFI:S:") + ssid + ";P:" + password + ";T:WPA;;";
      else response = "ERROR:UNKNOWN_CMD";
      Serial.print(response);
      Serial.println('\n');
      command = "";
    } else {
      command += c;
    }
  }
}

void handleWiFiConfirm() {
  if (currentUsername == "") {
    Serial.println("WiFi settings confirm denied: No user logged in.");
    server.send(403, "text/plain", "You need to log in to access this page.");
    return;
  }
  if (currentRole != "admin") {
    Serial.println("WiFi settings confirm denied: User " + currentUsername + " is not an admin.");
    server.send(403, "text/plain", "You do not have permission to access this page.");
    return;
  }

  if (!server.hasArg("ssid") || !server.hasArg("password")) {
    Serial.println("WiFi settings confirm failed: Missing SSID or password.");
    server.send(400, "text/plain", "Missing SSID or password.");
    return;
  }

  String newSsid = server.arg("ssid");
  String newPassword = server.arg("password");
  newSsid.trim();
  newPassword.trim();

  // Validate inputs
  if (newSsid.length() < 1 || newSsid.length() > 32) {
    Serial.println("WiFi settings confirm failed: Invalid SSID length.");
    server.send(400, "text/plain", "SSID must be between 1 and 32 characters.");
    return;
  }
  if (newPassword.length() < 8 || newPassword.length() > 63) {
    Serial.println("WiFi settings confirm failed: Invalid password length.");
    server.send(400, "text/plain", "Password must be between 8 and 63 characters.");
    return;
  }

  // Save new credentials to wifi.txt
  File wifiFile = SD.open(WIFI_FILE, FILE_WRITE);
  if (!wifiFile) {
    Serial.println("Failed to open wifi.txt for writing.");
    server.send(500, "text/plain", "Server error: Could not save WiFi settings.");
    return;
  }
  wifiFile.println(newSsid + "," + newPassword);
  wifiFile.close();
  Serial.println("WiFi credentials updated: SSID=" + newSsid + ", Password=" + newPassword);

  // Update global variables
  ssid = newSsid;
  password = newPassword;

  // Restart WiFi AP with new credentials
  WiFi.softAPdisconnect(true);
  WiFi.softAP(ssid.c_str(), password.c_str());
  Serial.print("WiFi AP restarted with new credentials. IP: ");
  Serial.println(WiFi.softAPIP());

  server.send(200, "text/plain", "WiFi credentials updated successfully.");
}

String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".gif")) return "image/gif";
  if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
  if (filename.endsWith(".ico")) return "image/x-icon";
  if (filename.endsWith(".xml")) return "text/xml";
  if (filename.endsWith(".pdf")) return "application/pdf";
  if (filename.endsWith(".txt")) return "text/plain";
  return "application/octet-stream";
}