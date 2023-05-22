#include <Wire.h>
#include <RTClib.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>

const char* ssid = "2.4 KariS";
const char* password = "12123402";

const char* htmlContent = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <title>Feeding Config</title>
  <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css">
  <style>
    body {
      padding-top: 100px;
      background-color: #f8f9fa;
    }

    .container {
      background-color: #fff;
      padding: 20px;
      border-radius: 5px;
      box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
    }

    h1 {
      color: #343a40;
      margin-bottom: 30px;
    }

    h2 {
      color: #343a40;
      margin-bottom: 20px;
    }

    .form-group label {
      color: #343a40;
      font-weight: bold;
    }

    .btn-primary {
      background-color: #007bff;
      border-color: #007bff;
    }

    .btn-primary:hover {
      background-color: #0069d9;
      border-color: #0062cc;
    }

    table {
      margin-top: 20px;
      border-collapse: collapse;
      width: 100%;
    }

    table thead th {
      background-color: #343a40;
      color: #fff;
      padding: 10px;
      text-align: center;
    }

    table tbody td {
      color: #343a40;
      padding: 10px;
      text-align: center;
    }

    table tbody tr:nth-child(even) {
      background-color: #f8f9fa;
    }

    table tbody tr:hover {
      background-color: #e9ecef;
    }

    .row {
      margin-bottom: 20px;
    }

    .col-md-6 {
      margin-bottom: 20px;
    }

    @media (max-width: 576px) {
      .col-md-6 {
        margin-bottom: 0;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Feeding Config</h1>
    <div class="row">
      <div class="col-md-6">
        <h2>Add New Feeding</h2>
        <form action="/save" method="GET">
          <div class="form-group">
            <label for="hour">Hour:</label>
            <input type="number" class="form-control" id="hour" name="hour" min="0" max="23" required>
          </div>
          <div class="form-group">
            <label for="minute">Minute:</label>
            <input type="number" class="form-control" id="minute" name="minute" min="0" max="59" required>
          </div>
          <div class="form-group">
            <label for="level">Level:</label>
            <input type="number" class="form-control" id="level" name="level" min="0" max="10" required>
          </div>
          <button type="submit" class="btn btn-primary">Save</button>
        </form>
      </div>
      <div class="col-md-6">
        <h2>Feeding List:</h2>
        <table class="table">
          <thead>
            <tr>
              <th scope="col">Order</th>
              <th scope="col">Time</th>
              <th scope="col">Level</th>
            </tr>
          </thead>
          <tbody>
            <!--FEEDING_LIST-->
          </tbody>
        </table>
      </div>
    </div>
  </div>
</body>
</html>
)HTML";




RTC_DS1307 rtc;
AsyncWebServer server(80);
Servo servo;

struct Feed_list {
  int index;
  int hour;
  int minute;
  int level;
};

Feed_list feedingList[10];
int feedingListSize = 0;
void sortFeedingList() {
  for (int i = 0; i < feedingListSize - 1; i++) {
    for (int j = 0; j < feedingListSize - i - 1; j++) {
      if (feedingList[j].hour > feedingList[j + 1].hour || (feedingList[j].hour == feedingList[j + 1].hour && feedingList[j].minute > feedingList[j + 1].minute)) {
        // Swap the feeding times
        Feed_list temp = feedingList[j];
        feedingList[j] = feedingList[j + 1];
        feedingList[j + 1] = temp;
      }
    }
  }

  // Update the order index
  for (int i = 0; i < feedingListSize; i++) {
    feedingList[i].index = i + 1;
  }
}
void setup() {
  Serial.begin(115200);

  Wire.begin();
  rtc.begin();

  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // Set the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("Local IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String listContent;
    for (int i = 0; i < feedingListSize; i++) {
      // Format hour and minute with leading '0' if necessary
      String hour = String(feedingList[i].hour, DEC);
      if (feedingList[i].hour < 10) {
        hour = "0" + hour;
      }

      String minute = String(feedingList[i].minute, DEC);
      if (feedingList[i].minute < 10) {
        minute = "0" + minute;
      }

      String time = hour + ":" + minute;
      listContent += "<tr>"
                     "<td>"
                     + String(feedingList[i].index) + "</td>"
                                                      "<td>"
                     + time + "</td>"
                              "<td>"
                     + String(feedingList[i].level) + "</td>"
                                                      "</tr>";
    }

    String response = String(htmlContent);
    int placeholderIndex = response.indexOf("<!--FEEDING_LIST-->");
    response = response.substring(0, placeholderIndex) + listContent + response.substring(placeholderIndex + 19);
    request->send(200, "text/html", response);
  });


  server.on("/save", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (request->hasParam("hour") && request->hasParam("minute") && request->hasParam("level")) {
      int hour = request->getParam("hour")->value().toInt();
      int minute = request->getParam("minute")->value().toInt();
      int level = request->getParam("level")->value().toInt();

      // Check if feeding time already exists
      bool exists = false;
      for (int i = 0; i < feedingListSize; i++) {
        if (feedingList[i].hour == hour && feedingList[i].minute == minute) {
          exists = true;
          break;
        }
      }

      if (!exists) {
        if (feedingListSize < 10) {
          feedingList[feedingListSize].index = feedingListSize + 1;
          feedingList[feedingListSize].hour = hour;
          feedingList[feedingListSize].minute = minute;
          feedingList[feedingListSize].level = level;
          feedingListSize++;

          // Sort the feeding list by time
          sortFeedingList();

          String response = "<link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.5.0/css/bootstrap.min.css'>";
          response += "<div class='alert alert-success' role='alert'>Feeding time saved successfully</div>";
          response += "<script>setTimeout(function() { window.location.href = '/'; }, 3000);</script>";
          request->send(200, "text/html", response);
        } else {
          Serial.println("Cannot add more feeding times. List is full!");
          String response = "<link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.5.0/css/bootstrap.min.css'>";
          response += "<div class='alert alert-danger' role='alert'>Cannot add more feeding times. List is full!</div>";
          response += "<script>setTimeout(function() { window.location.href = '/'; }, 3000);</script>";
          request->send(200, "text/html", response);
        }
      } else {
        String response = "<link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.5.0/css/bootstrap.min.css'>";
        response += "<div class='alert alert-warning' role='alert'>Feeding time already exists in the list</div>";
        response += "<script>setTimeout(function() { window.location.href = '/'; }, 3000);</script>";
        request->send(200, "text/html", response);
        Serial.println("Feeding time already exists in the list");
      }
    }
  });




  server.begin();

  servo.attach(D3);
}

bool feedingDone = false;

void loop() {
  DateTime now = rtc.now();
  int currentHour = now.hour();
  int currentMinute = now.minute();

  Serial.print("Current time: ");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.print(currentMinute);
  Serial.println();

  bool feedingTimeFound = false;  // Flag to indicate if a feeding time has been found for the current time
  for (int i = 0; i < feedingListSize; i++) {
    if (feedingList[i].hour != currentHour || feedingList[i].minute != currentMinute) {
      feedingDone = false;
    }
  }

  for (int i = 0; i < feedingListSize; i++) {
    if (feedingList[i].hour == currentHour && feedingList[i].minute == currentMinute) {
      if (!feedingDone) {  // Check if feeding has already been done
        int level = feedingList[i].level;
        Serial.print("Feeding time found at index ");
        Serial.print(i);
        Serial.print(". Spinning servo ");
        Serial.print(level);
        Serial.println(" times.");

        for (int j = 0; j < level; j++) {
          servo.write(180);
          delay(500);
          servo.write(0);
          delay(500);
        }

        feedingDone = true;  // Set the feeding done flag to true
      }
      break;  // Exit the loop after processing one feeding time
    }
  }

  Serial.println("Feeding List:");
  for (int i = 0; i < feedingListSize; i++) {
    Serial.print("Index: ");
    Serial.print(feedingList[i].index);
    Serial.print(", Hour: ");
    Serial.print(feedingList[i].hour);
    Serial.print(", Minute: ");
    Serial.print(feedingList[i].minute);
    Serial.print(", Level: ");
    Serial.println(feedingList[i].level);
  }

  delay(60000);
}
