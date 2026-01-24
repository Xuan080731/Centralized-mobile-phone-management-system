var SS_ID = "填入試算表 ID"; 

var SCHEDULE_SHEET_NAME = "排程設定";

function getMainSheet() {
  var ss = SpreadsheetApp.openById(SS_ID);
  var sheet = ss.getSheetByName("Sheet1");
  if (!sheet) sheet = ss.getSheetByName("工作表1");
  if (!sheet) sheet = ss.getSheets()[0];
  return sheet; 
}

function doGet(e) {
  var params = e.parameter;
  if (!params.data && !params.valve) {
    return HtmlService.createHtmlOutputFromFile('Index')
        .setTitle('智慧手機保管箱監控系統')
        .addMetaTag('viewport', 'width=device-width, initial-scale=1');
  }

  var lock = LockService.getScriptLock();
  if (lock.tryLock(10000)) { 
    try {
      var sheet = getMainSheet();
      var date = new Date();
      var dateStr = Utilities.formatDate(date, Session.getScriptTimeZone(), "yyyy/MM/dd");
      var timeStr = Utilities.formatDate(date, Session.getScriptTimeZone(), "HH:mm:ss");

      var dataRaw = params.data || "0"; 
      var valve = params.valve || "0";
      var temp = params.temp || "0";
      var hum = params.hum || "0";
      var smoke = params.smoke || "0";
      var user = params.user || "";
      var door = params.door || "0";

      var currentPins = new Array(20).fill("No");
      if (dataRaw !== "0") {
        var activePins = dataRaw.split(",");
        for (var i = 0; i < activePins.length; i++) {
          var pinIndex = parseInt(activePins[i]) - 1;
          if (pinIndex >= 0 && pinIndex < 20) currentPins[pinIndex] = "Yes";
        }
      }

      var valveStr = (valve == 1 || valve == "1" || valve == "開啟") ? "開啟" : "關閉";
      var doorStr = (door == "1" || door == 1) ? "1" : "0"; 

      var lastRow = sheet.getLastRow();
      var needAppend = true; 

      if (lastRow > 1) { 
        var maxCol = sheet.getLastColumn();
        var lastValues = sheet.getRange(lastRow, 1, 1, maxCol).getValues()[0];
        
        var lastPinsStr = (maxCol >= 22) ? lastValues.slice(2, 22).join(",") : "";
        var lastValve = (maxCol >= 23) ? lastValues[22] : "";
        var lastUser = (maxCol >= 27) ? lastValues[26] : "";  
        var lastDoor = (maxCol >= 28) ? String(lastValues[27]) : "0";
        var currentPinsStr = currentPins.join(",");
        
        if (lastPinsStr === currentPinsStr && lastValve === valveStr && lastUser === user && lastDoor === doorStr) {
            needAppend = false;
        }
      }

      if (needAppend) {
        var rowData = [dateStr, timeStr].concat(currentPins);
        rowData.push(valveStr, temp, hum, smoke, user, doorStr);
        sheet.appendRow(rowData);
      } else {
        sheet.getRange(lastRow, 1, 1, 2).setValues([[dateStr, timeStr]]);
        if (sheet.getLastColumn() >= 26) {
           sheet.getRange(lastRow, 24, 1, 3).setValues([[temp, hum, smoke]]);
        }
      }

      var responseString = "";
      var ss = SpreadsheetApp.openById(SS_ID);
      var scheduleSheet = ss.getSheetByName(SCHEDULE_SHEET_NAME);
      if (scheduleSheet) {
          var remoteFlag = scheduleSheet.getRange("B1").getValue();
          if (remoteFlag == 1 || remoteFlag == "1") {
              responseString = "CMD_UNLOCK,";
              scheduleSheet.getRange("B1").setValue(0);
          }
      }
      responseString += getScheduleString();
      return ContentService.createTextOutput(responseString);

    } catch (e) {
      return ContentService.createTextOutput("Error");
    } finally {
      lock.releaseLock();
    }
  } else {
    return ContentService.createTextOutput("Busy");
  }
}

function getData() {
  try {
    var sheet = getMainSheet();
    var lastRow = sheet.getLastRow();
    
    if (lastRow < 2) {
       return { 
         fullDateTime: "--", rawTimestamp: 0, 
         pins: new Array(20).fill("No"), 
         valve: "關閉", temp: "--", hum: "--", smoke: "0", user: "", door: "0" 
       };
    }

    var maxCol = sheet.getLastColumn();
    var values = sheet.getRange(lastRow, 1, 1, maxCol).getValues()[0];
    
    var dateStr = values[0];
    var timeStr = values[1];
    if (typeof dateStr === 'object') dateStr = Utilities.formatDate(dateStr, Session.getScriptTimeZone(), "yyyy/MM/dd");
    if (typeof timeStr === 'object') timeStr = Utilities.formatDate(timeStr, Session.getScriptTimeZone(), "HH:mm:ss");

    var fullDateTimeStr = dateStr + " " + timeStr;
    var lastTimestamp = new Date(fullDateTimeStr).getTime(); // 修正：確實解析時間

    var pins = [];
    for(var i=2; i<22; i++) {
        pins.push((i < values.length) ? values[i] : "No");
    }
    
    var rawValve = (22 < values.length) ? values[22] : "0";
    var valveStr = (rawValve == 1 || rawValve == "1" || rawValve == "開啟") ? "開啟" : "關閉";
    var tempVal = (23 < values.length) ? values[23] : "--";
    var humVal = (24 < values.length) ? values[24] : "--";
    var smokeVal = (25 < values.length) ? values[25] : "0";
    var userVal = (26 < values.length) ? values[26] : "";
    var doorVal = (27 < values.length) ? values[27] : "0";

    return {
      fullDateTime: fullDateTimeStr,
      rawTimestamp: lastTimestamp,
      pins: pins,
      valve: valveStr,
      temp: tempVal,
      hum: humVal,
      smoke: smokeVal,
      user: userVal,
      door: doorVal
    };
  } catch (e) {
    throw new Error("Backend Error: " + e.toString()); 
  }
}

function triggerRemoteUnlock() {
  var ss = SpreadsheetApp.openById(SS_ID);
  var sheet = ss.getSheetByName(SCHEDULE_SHEET_NAME);
  if (!sheet) sheet = ss.insertSheet(SCHEDULE_SHEET_NAME);
  sheet.getRange("B1").setValue(1);
  return "OK";
}

function getScheduleString() {
  var ss = SpreadsheetApp.openById(SS_ID);
  var sheet = ss.getSheetByName(SCHEDULE_SHEET_NAME);
  if (!sheet) return "";
  var lastRow = sheet.getLastRow();
  if (lastRow < 1) return "";
  var values = sheet.getRange(1, 1, lastRow, 1).getValues();
  var arr = [];
  for (var i = 0; i < values.length; i++) {
    var v = values[i][0];
    if(v) {
       if (v instanceof Date) v = Utilities.formatDate(v, Session.getScriptTimeZone(), "HH:mm");
       var s = String(v);
       if (s.indexOf(":") > -1) arr.push(s);
    }
  }
  return arr.join(",");
}

function getScheduleList() {
  var ss = SpreadsheetApp.openById(SS_ID);
  var sheet = ss.getSheetByName(SCHEDULE_SHEET_NAME);
  if (!sheet) return [];
  var lastRow = sheet.getLastRow();
  if (lastRow < 1) return [];
  var values = sheet.getRange(1, 1, lastRow, 1).getValues();
  var list = [];
  for(var i=0; i<values.length; i++){
     var v = values[i][0];
     if(v) {
        if (v instanceof Date) v = Utilities.formatDate(v, Session.getScriptTimeZone(), "HH:mm");
        var s = String(v);
        if (s.indexOf(":") > -1) list.push(s);
     }
  }
  return list;
}

function saveScheduleList(arr) {
  var ss = SpreadsheetApp.openById(SS_ID);
  var sheet = ss.getSheetByName(SCHEDULE_SHEET_NAME);
  if (!sheet) sheet = ss.insertSheet(SCHEDULE_SHEET_NAME);
  var remoteFlag = sheet.getRange("B1").getValue();
  sheet.clearContents();
  sheet.getRange("B1").setValue(remoteFlag);
  if (arr && arr.length > 0) {
    var rows = arr.map(function(t){ return [t]; });
    sheet.getRange(1, 1, rows.length, 1).setValues(rows);
  }
  return "OK";
}
