//HTML Index Page for the controller server
/*
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP Web Server Remote</title>
  <style>
    body {
        background-color: #f0f0f0;
        text-align: center;
        font-family: Arial, Helvetica, sans-serif;
    }
  </style>
</head>
<body>
    <h1>ESP Web Server Remote</h1>
    <h2>X-Axis Angle: <span id='Xaxis'>-</span></h2>
    <h2>Y-Axis Angle: <span id='Yaxis'>-</span></h2>
<script>
  var Socket;
  function init() {
    Socket = new WebSocket("ws://" + window.location.hostname + ":81/");
    Socket.onmessage = function(event) {
      console.log("Connected to server");
      processData(event);
    };
  }
  function processData(event) {
      var data = JSON.parse(event.data);
      console.log(data);
      document.getElementById('Xaxis').innerHTML = data.Xangle;
      document.getElementById('Yaxis').innerHTML = data.Yangle;    
  }
  window.onload = function(event) {
    init();
  }
</script>
</body>
</html>
)rawliteral";
*/

const char index_html[] PROGMEM = R"rawliteral(
  Hello!
  )rawliteral";
