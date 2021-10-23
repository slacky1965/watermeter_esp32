function TogglePassAdmin() {
  let btt = password
  if (btt.type === "password") {
    btt.type = "text";
  } else {
    btt.type = "password";
  }
}

function TogglePassWiFi() {
  let btt = wifiappass
  if (btt.type === "password") {
    btt.type = "text";
  } else {
    btt.type = "password";
  }
  let btt2 = wifistapass
  if (btt2.type === "password") {
    btt2.type = "text";
  } else {
    btt2.type = "password";
  }
}

function TogglePassMqtt() {
  let btt = mqttpass
  if (btt.type === "password") {
    btt.type = "text";
  } else {
    btt.type = "password";
  }
}

