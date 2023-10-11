/////////////////////////////////////////////
//
// Venkovni teplomer v1.0
//
//
/////////////////////////////////////////////
#include <TimeLib.h>
#include <U8g2lib.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <TaskScheduler.h>
#include <EEPROM.h>

#define SCK D3 
#define SDA D4

U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0,SCK,SDA);
unsigned int width = 0;
unsigned int height = 0;

OneWire oneWire(D5);

DallasTemperature dallas(&oneWire);
char tempC[5] = {0};
float Temp = 0.0f;

const char *ssid = "TempoAP";
//const char *password = "22222222";

int Year = 0;
int Month = 0;
int Day = 0;
int Hour = 0;
int Minute = 0;
int Second = 0;

ESP8266WebServer server(80);

Scheduler scheduler;


struct DataEEPROM
{
  //int16_t Year;
  int8_t Month;
  int8_t Day;
  int8_t Hour;
  int8_t Minute;
  float_t Temp;
};

struct DataEEPROM edata;
int16_t savingCount = 0;

/////////////////////////////////////////////////////////////

void TimerForEEPROM() 
{
  time_t aNow = now();
  Year = year(aNow);
  Month = month(aNow);
  Day = day(aNow);
  Hour = hour(aNow);
  Minute = minute(aNow);
  Second = second(aNow);
  DataEEPROM countEE = {0};
  DataEEPROM dataEE = {0};
  savingCount++;
  if (savingCount > 100)
    savingCount = 1;
  // ADDRESS
  // 0123456789012345678901234567890
  // yymdhmtttt
  countEE.Month = savingCount;
  EEPROM.put(0,countEE);
  EEPROM.commit();
  //
  dataEE.Month = Month;
  dataEE.Day = Day;
  dataEE.Hour = Hour;
  dataEE.Minute = Minute;
  dataEE.Temp = Temp;
  EEPROM.put(savingCount*sizeof(DataEEPROM),dataEE);
  EEPROM.commit();
}

Task taskTimer(900000, TASK_FOREVER, &TimerForEEPROM);


/////////////////////////////////////////////////////////////

void measureTemperature() 
{
  strcpy(tempC,"");
  Temp = 0.0f;
  dallas.requestTemperatures();
  Temp = dallas.getTempCByIndex(0);
  sprintf(tempC, "%.1f", Temp);
}

Task taskTemp(5000, TASK_FOREVER, &measureTemperature);

/////////////////////////////////////////////////////////////

void showTime() 
{
  time_t aNow = now();
  Year = year(aNow);
  Month = month(aNow);
  Day = day(aNow);
  Hour = hour(aNow);
  Minute = minute(aNow);
  Second = second(aNow);
  //char totalDateTime[256] = "";
  //sprintf(totalDateTime, "%04d-%02d-%02d %02d:%02d:%02d", Year, Month, Day, Hour, Minute, Second);
  char dayC[4] = {0};
  char monthC[4] = {0};
  char hourC[4] = {0};
  char minC[4] = {0};
  sprintf(dayC, "%02d", Day);
  sprintf(monthC, "%02d", Month);
  sprintf(hourC, "%02d", Hour);
  sprintf(minC, "%02d", Minute);
  //Serial.print("Aktuální čas: ");
  //Serial.print(totalDateTime);
  //Serial.println();
  //u8g2.drawFrame(0, 0, width, height);
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, width, height);
  u8g2.setDrawColor(15);
  u8g2.setFont(u8g2_font_8bitclassic_tf);
  u8g2.setCursor(2,12);
  u8g2.print(dayC);
  u8g2.setCursor(2,27);
  u8g2.print(monthC);
  u8g2.setCursor(104,12);
  u8g2.print(hourC);
  u8g2.setCursor(104,27);
  u8g2.print(minC);
  u8g2.drawVLine(25, 0, 32);
  u8g2.drawVLine(102, 0, 32);
  u8g2.setFont(u8g2_font_logisoso24_tn);
  int lenTemp = u8g2.getStrWidth(tempC);
  int widthTemp = width - 50;
  u8g2.setCursor(25+((widthTemp/2)-(lenTemp/2)),31);
  u8g2.print(tempC);
  //u8g2.print("12.2");
  u8g2.sendBuffer();
}

Task taskTime(1000, TASK_FOREVER, &showTime);

/////////////////////////////////////////////////////////////

void handleRoot();


void setup() 
{
  setTime(0,0,0,1,1,2023);
  EEPROM.begin(1024);
  DataEEPROM cntEE = {0};
  EEPROM.get(0,cntEE);
  savingCount = cntEE.Month;
  //
  u8g2.begin();
  u8g2.clearDisplay();
  width = u8g2.getDisplayWidth();
  height = u8g2.getDisplayHeight();
  //
  IPAddress local_IP(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAP(ssid, "", 1, false, 8);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  //
  server.on("/", HTTP_GET, handleRoot);
  server.on("/history", HTTP_POST, handleHistory);
  server.on("/datetime", HTTP_POST, handleDateTime);
  server.on("/data", HTTP_POST, handleGetDateTime);
  server.on("/clear", HTTP_POST, handleClear);
  server.begin();
  //
  scheduler.init();
  scheduler.addTask(taskTemp);
  scheduler.addTask(taskTime);
  scheduler.addTask(taskTimer);
  taskTemp.enable();
  taskTime.enable();
  taskTimer.enable();
  scheduler.execute();
}

/////////////////////////////////////////////////////////////

void loop() 
{
  server.handleClient();
  scheduler.execute();
}

/////////////////////////////////////////////////////////////

DataEEPROM getMin()
{
  float min = 100.0f;
  DataEEPROM save = {0};
  save.Temp = min;
  for (int i = 1; i < 101; i++)
  {
    DataEEPROM dataEE = {0};
    EEPROM.get(i*sizeof(DataEEPROM),dataEE);
    if (dataEE.Temp < min) {
      min = dataEE.Temp;
      save = dataEE;
    }
  }
  return save;
}

DataEEPROM getMax()
{
  float max = -100.0f;
  DataEEPROM save = {0};
  save.Temp = max;
  for (int i = 1; i < 101; i++)
  {
    DataEEPROM dataEE = {0};
    EEPROM.get(i*sizeof(DataEEPROM),dataEE);
    if (dataEE.Temp > max)
    {
      if (dataEE.Temp > 100) {
      }
      else {
        max = dataEE.Temp;
        save = dataEE;
      }
    }
  }
  return save;
}

/////////////////////////////////////////////////////////////

void handleRoot()
{
  char totalDateTime[256] = "";
  sprintf(totalDateTime, "%04d-%02d-%02d %02d:%02d:%02d", Year, Month, Day, Hour, Minute, Second);
  String html = "";
  html += "<!DOCTYPE HTML>";
  html += "<html LANG='cs'>";
  html += "<head>";
  html += "<meta HTTP-EQUIV='Refresh' CONTENT='5' charset='UTF-8'>";
  html += "<title>TEPLOMER</title>";
  html += "<style>";
  html += ".title-temp {";
  html += "text-align: center;";
  html += "font-family: Verdana, sans-serif;";
  html += "color: black;";
  html += "font-size: 50px;";
  html += "}";
  html += ".text-temp {";
  html += "text-align: center;";
  html += "font-family: Verdana, sans-serif;";
  html += "color: black;";
  html += "font-size: 70px;";
  html += "}";
  html += ".text-datetime {";
  html += "text-align: center;";
  html += "font-family: Helvetica, sans-serif;";
  html += "color: black;";
  html += "font-size: 25px;";
  html += "}";
  html += ".title-min {";
  html += "text-align: center;";
  html += "font-family: Helvetica, sans-serif;";
  html += "color: blue;";
  html += "font-size: 25px;";
  html += "}";
  html += ".title-max {";
  html += "text-align: center;";
  html += "font-family: Helvetica, sans-serif;";
  html += "color: red;";
  html += "font-size: 25px;";
  html += "}";
  html += ".input-one {";
  html += "background-color: lightblue;";
  html += "color: black;";
  html += "border-color: black;";
  html += "font-size: 40px;";
  html += "width: 300px;";
  html += "}";
  html += ".input-two {";
  html += "background-color: lightgreen;";
  html += "color: black;";
  html += "border-color: black;";
  html += "font-size: 40px;";
  html += "width: 300px;";
  html += "}";
  html += ".input-three {";
  html += "background-color: lightred;";
  html += "color: black;";
  html += "border-color: red;";
  html += "font-size: 25px;";
  html += "width: 300px;";
  html += "}";
  html += ".red {";
  html += "color: red;";
  html += "}";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<br>";
  html += "<br>";
  html += "<div class='title-temp'>";
  html += "<center>AKTUÁLNÍ TEPLOTA</center>";
  html += "</div>";
  html += "<br>";
  html += "<div class='text-temp'>";
  html += String(tempC);
  html += "\u00B0C</div>";
  html += "<br>";
  html += "<br>";
  html += "<div class='text-datetime'>[";
  html += totalDateTime;
  html += "]</div>";
  html += "<br>";
  html += "<br>";
  char DateCMin[32] = {0};
  char TimeCMin[32] = {0};
  char TempHistoryMin[8] = {0};
  char DateCMax[32] = {0};
  char TimeCMax[32] = {0};
  char TempHistoryMax[8] = {0};
  DataEEPROM min = getMin();
  DataEEPROM max = getMax();
  if (min.Temp == 100)
  {
    strcpy(DateCMin,"mm-dd");
    strcpy(TimeCMin,"hh:mm");
    strcpy(TempHistoryMin,"0.0");
  }
  else
  {
    sprintf(DateCMin, "%02d-%02d", min.Month, min.Day);
    sprintf(TimeCMin, "%02d:%02d", min.Hour, min.Minute);
    sprintf(TempHistoryMin, "%02.1f", min.Temp);
  }
  if (max.Temp == -100)
  {
    strcpy(DateCMax,"mm-dd");
    strcpy(TimeCMax,"hh:mm");
    strcpy(TempHistoryMax,"0.0");
  }
  else
  {
    sprintf(DateCMax, "%02d-%02d", max.Month, max.Day);
    sprintf(TimeCMax, "%02d:%02d", max.Hour, max.Minute);
    sprintf(TempHistoryMax, "%02.1f", max.Temp);
  }
  html += "<div class='title-min'>";
  html += "[MIN: ";
  html += TempHistoryMin;
  html += " - ";
  html += TimeCMin;
  html += " - ";
  html += DateCMin;
  html += "]</div>";
  html += "<br>";
  html += "<div class='title-max'>";
  html += "[MAX: ";
  html += TempHistoryMax;
  html += " - ";
  html += TimeCMax;
  html += " - ";
  html += DateCMax;
  html += "]</div>";
  html += "<br>";
  html += "<br>";
  html += "<div style='text-align: center;'>";
  html += "<button class='input-one' id='historyButton'>Historie</button>";
  html += "</div>";
  html += "<br>";
  html += "<div style='text-align: center;'>";
  html += "<button class='input-two' id='datetimeButton'>Datum a cas</button>";
  html += "</div>";
  html += "<br>";
  html += "<br>";
  html += "<br>";
  html += "<div style='text-align: center;'>";
  html += "<button class='input-three' id='clearButton'>!!! SMAZAT HISTORII !!!</button>";
  html += "</div>";
  html += "<br>";
  html += "<br>";
  html += "<script>";
  html += "document.getElementById('historyButton').addEventListener('click', function() {";
  html += "var form = document.createElement('form');";
  html += "form.method = 'POST';";
  html += "form.action = '/history';";
  html += "document.body.appendChild(form);";
  html += "form.submit();";
  html += "});";
  html += "document.getElementById('datetimeButton').addEventListener('click', function() {";
  html += "var form = document.createElement('form');";
  html += "form.method = 'POST';";
  html += "form.action = '/datetime';";
  html += "document.body.appendChild(form);";
  html += "form.submit();";
  html += "});";
  html += "document.getElementById('clearButton').addEventListener('click', function() {";
  html += "var confirmation = confirm('Opravdu chces pokračovat?');";
  html += "if (confirmation) {";
  html += "var form = document.createElement('form');";
  html += "form.method = 'POST';";
  html += "form.action = '/clear';";
  html += "document.body.appendChild(form);";
  html += "form.submit();";
  html += "}";
  html += "});";  
  html += "</script>";
  html += "</body>";
  html += "</html>";  //Serial.print(html);
  server.send(200, "text/html", html);
}

void handleHistory()
{
  String html = "";
  html += "<!DOCTYPE HTML>";
  html += "<html LANG='cs'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>TEPLOMER - HISTORIE</title>";
  html += "<style>";
  html += ".input-back {";
  html += "background-color: lightblue;";
  html += "color: black;";
  html += "border-color: black;";
  html += "font-size: 30px;";
  html += "width: 300px;";
  html += "}";
  html += ".refresh-button {";
  html += "background-color: lightcoral;";
  html += "color: black;";
  html += "border-color: black;";
  html += "font-size: 30px;";
  html += "width: 300px;";
  html += "}";
  html += ".title-history {";
  html += "text-align: center;";
  html += "font-family: Verdana, sans-serif;";
  html += "color: black;";
  html += "font-size: 50px;";
  html += "}";
  html += "table {";
  html += "border:1px solid #b3adad;";
  html += "border-collapse:collapse;";
  html += "padding:5px;";
  html += "width:50%;";
  html += "}";
  html += "table th {";
  html += "border:1px solid #b3adad;";
  html += "padding:5px;";
  html += "background: #f0f0f0;";
  html += "color: #313030;";
  html += "width:25.0%;";
  html += "font-size: 30px;";
  html += "}";
  html += "table td {";
  html += "border:1px solid #b3adad;";
  html += "text-align:center;";
  html += "padding:5px;";
  html += "background: #ffffff;";
  html += "color: #313030;";
  html += "font-size: 30px;";
  html += "}";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<br>";
  html += "<br>";
  html += "<div class='title-history'>";
  html += "<center>HISTORIE</center>";
  html += "</div>";
  html += "<br>";
  html += "<center>";
  html += "<button class='input-back' id='backButton'>Zpět</button>";
  html += "</center>";
  html += "<br>";
  html += "<center>";
  html += "<button class = 'refresh-button' onclick='refreshPage()'>Obnovit stránku</button>";
  html += "</center>";
  html += "<br>";
  html += "<br>";
  html += "<center>";
  html += "<table>";
  html += "<thead>";
  html += "<tr>";
  html += "<th></th>";
  html += "<th>Datum</th>";
  html += "<th>Cas</th>";
  html += "<th>Teplota</th>";
  html += "</tr>";
  html += "</thead>";
  html += "<tbody>";
  for (long i = 1; i < 101; i++)
  {
    html += "<tr>";
    DataEEPROM dataEE = {0};
    EEPROM.get(i*sizeof(DataEEPROM), dataEE);
    if ( i == savingCount )
    {
      html += "<td style='background-color: lightgreen;'>";
      html += String(i);
      html += "</td>";
    }
    else
    {
      html += "<td>";
      html += String(i);
      html += "</td>";
    }
    char DateC[32] = {0};
    char TimeC[32] = {0};
    char TempHistory[8] = {0};
    if (dataEE.Temp > 100.0f)
    {
      strcpy(DateC,"not");
      strcpy(TimeC,"not");
      strcpy(TempHistory,"not");
    }
    else
    {
      sprintf(DateC, "%02d-%02d", dataEE.Month, dataEE.Day);
      sprintf(TimeC, "%02d:%02d", dataEE.Hour, dataEE.Minute);
      sprintf(TempHistory, "%.1f", dataEE.Temp);
    }
    html += "<td>";
    html += DateC;
    html += "</td>";
    html += "<td>";
    html += TimeC;
    html += "</td>";
    html += "<td>";
    html += TempHistory;
    html += "</td>";
    html += "</tr>";
  }
  html += "</tbody>";
  html += "</table>";
  html += "</center>";
  html += "<br>";
  html += "<center>";
  html += "<button class='input-back' id='backButton2'>Zpět</button>";
  html += "</center>";
  html += "<br>";
  html += "<script>";
  html += "var backButton = document.getElementById('backButton');";
  html += "backButton.addEventListener('click', function() {";
  html += "window.location.href = '/';";
  html += "});";
  html += "var backButton2 = document.getElementById('backButton2');";
  html += "backButton2.addEventListener('click', function() {";
  html += "window.location.href = '/';";
  html += "});";
  html += "function refreshPage() {";
  html += "location.reload();}";
  html += "</script>";
  html += "</body>";
  html += "</html>";  
  server.send(200, "text/html", html);
}


void handleGetDateTime()
{
  String html = "";
  if (server.hasArg("rok") && server.hasArg("mesic") && server.hasArg("den") &&
      server.hasArg("hodiny") && server.hasArg("minuty")) {
    Hour = server.arg("hodiny").toInt();
    Minute = server.arg("minuty").toInt();
    Second = 0;
    Day = server.arg("den").toInt();
    Month = server.arg("mesic").toInt();
    Year = server.arg("rok").toInt();
    setTime(Hour,Minute,Second,Day,Month,Year);
    html += "<!DOCTYPE HTML>";
    html += "<html LANG='cs'>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<title>Nastaveno</title>";
    html += "</head>";
    html += "<body>";
    html += "<div>";
    html += "<h1>Datum a cas byl aktualizovan</h1>";
    html += "<h2>Datum byl nastaven na ";
    html += String(Year);
    html += "-";
    html += String(Month);
    html += "-";
    html += String(Day);
    html += "</h2>";
    html += "<h2>Cas byl nastaven na ";
    html += String(Hour);
    html += ":";
    html += String(Minute);
    html += ":";
    html += String(Second);
    html += "</h2>";
    html += "</div>";
    html += "<script>";
    html += "setTimeout(function() {";
    html += "  window.location.href = '/'";
    html += "}, 5000);";
    html += "</script>";
    html += "</body>";
    html += "</html>";    
    server.send(200, "text/html", html);
  } 
  else 
  {
    html += "<!DOCTYPE HTML>";
    html += "<html LANG='cs'>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<title>Chyba</title>";
    html += "</head>";
    html += "<body>";
    html += "<div>";
    html += "<h1>Chyba v nastavovani data a casu!</h1>";
    html += "<h2>Jsou zadane vsechny udaje ?</h2>";
    html += "</div>";
    html += "<script>";
    html += "setTimeout(function() {";
    html += "  window.location.href = '/'";
    html += "}, 5000);";
    html += "</script>";
    html += "</body>";
    html += "</html>";    
    server.send(400, "text/html", html);
  }
}


void handleDateTime()
{
  String html = "";
  html += "<!DOCTYPE HTML>";
  html += "<html LANG='cs'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>TEPLOMER - DATUM A CAS</title>";
  html += "<style>";
  html += ".input-back {";
  html += "background-color: lightblue;";
  html += "color: black;";
  html += "border-color: black;";
  html += "font-size: 30px;";
  html += "width: 300px;";
  html += "}";
  html += ".title-datetime {";
  html += "text-align: center;";
  html += "font-family: Verdana, sans-serif;";
  html += "color: black;";
  html += "font-size: 50px;";
  html += "}";
  html += "form {";
  html += "width: 50%;";
  html += "text-align: center;";
  html += "font-size: 30px;";
  html += "}";
  html += ".form-group {";
  html += "display: flex;";
  html += "justify-content: space-between;";
  html += "align-items: center;";
  html += "margin-bottom: 10px;";
  html += "}";
  html += "label {";
  html += "flex: 1;";
  html += "text-align: right;";
  html += "margin-right: 10px;";
  html += "font-size: 30px;";
  html += "}";
  html += "input[type=\"number\"] {";
  html += "flex: 1;";
  html += "width: 30px; /* Šířka pro pole pro číslo */";
  html += "text-align: center;";
  html += "font-size: 30px;";
  html += "}";
  html += "input[type=\"submit\"] {";
  html += "width: 50%;";
  html += "background-color: lightgreen;";
  html += "border: 2px;";
  html += "padding: 10px;";
  html += "cursor: pointer;";
  html += "font-size: 30px;";
  html += "}";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<br>";
  html += "<br>";
  html += "<div class='title-datetime'>";
  html += "<center>NASTAVENI DATA A CASU</center>";
  html += "</div>";
  html += "<br>";
  html += "<center>";
  html += "<button class='input-back' id='backButton'>Zpět</button>";
  html += "</center>";
  html += "<br>";
  html += "<br>";
  html += "<br>";
  html += "<br>";
  html += "<center>";
  html += "<form action='/data' method='post'>";
  html += "<div class='form-group'>";
  html += "<label>Rok:</label>";
  html += "<input type='number' style='text-align: center;' value='";
  html += String(Year);
  html += "' name='rok' min='1900' max='2099' required>";
  html += "<label>Mesic:</label>";
  html += "<input type='number' style='text-align: center;' value='";
  html += String(Month);
  html += "' name='mesic' min='1' max='12' required>";
  html += "<label>Den:</label>";
  html += "<input type='number' style='text-align: center;' value='";
  html += String(Day);
  html += "' name='den' min='1' max='31' required>";
  html += "</div>";
  html += "<div class='form-group'>";
  html += "<label>Hodiny:</label>";
  html += "<input type='number' style='text-align: center;' value='";
  html += String(Hour);
  html += "' name='hodiny' min='0' max='23' required>";
  html += "<label>Minuty:</label>";
  html += "<input type='number' style='text-align: center;' value='";
  html += String(Minute);
  html += "' name='minuty' min='0' max='59' required>";
  html += "</div>";
  html += "<input type='submit' value='Odeslat'>";
  html += "</form>";
  html += "</center>";
  html += "<br>";
  html += "<br>";
  html += "<br>";
  html += "<br>";
  html += "<br>";
  html += "<center>";
  html += "<button class='input-back' id='backButton2'>Zpět</button>";
  html += "</center>";
  html += "<br>";
  html += "<script>";
  html += "var backButton = document.getElementById('backButton');";
  html += "backButton.addEventListener('click', function() {";
  html += "window.location.href = '/';";
  html += "});";
  html += "var backButton2 = document.getElementById('backButton2');";
  html += "backButton2.addEventListener('click', function() {";
  html += "window.location.href = '/';";
  html += "});";
  html += "</script>";
  html += "</body>";
  html += "</html>";  
  server.send(200, "text/html", html);
}

void handleClear()
{
  for (int i = 0; i < 1024; i++) {
    EEPROM.write(i, 255);
  }
  EEPROM.commit();
  DataEEPROM dataEE = {0};
  dataEE.Month = 0;
  EEPROM.put(0,dataEE);
  savingCount = 0;
  EEPROM.commit();  
  String html = "";
  html += "<!DOCTYPE HTML>";
  html += "<html LANG='cs'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>EEPROM smazan</title>";
  html += "</head>";
  html += "<body>";
  html += "<div>";
  html += "<h1>EEPROM byl vynulovan!</h1>";
  html += "<h2>Vse ok.</h2>";
  html += "</div>";
  html += "<script>";
  html += "setTimeout(function() {";
  html += "  window.location.href = '/'";
  html += "}, 3000);";
  html += "</script>";
  html += "</body>";
  html += "</html>";    
  server.send(200, "text/html", html);
}


