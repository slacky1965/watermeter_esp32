function TogglePassAdmin() {
  let btt = password;
  if (btt.type === "password") {
    btt.type = "text";
  } else {
    btt.type = "password";
  }
}

function TogglePassWiFi() {
  let btt = wifiappass;
  if (btt.type === "password") {
    btt.type = "text";
  } else {
    btt.type = "password";
  }
  let btt2 = wifistapass;
  if (btt2.type === "password") {
    btt2.type = "text";
  } else {
    btt2.type = "password";
  }
}

function TogglePassMqtt() {
  let btt = mqttpass;
  if (btt.type === "password") {
    btt.type = "text";
  } else {
    btt.type = "password";
  }
}

function setpath(elem) {
    var default_path = elem.files[0].name;
    if (elem.id == "newhtmlfile") {
        document.getElementById("uploadhtml").value = default_path;
    } else if (elem.id == "newcertfile") {
        document.getElementById("uploadcert").value = default_path;
    } else if (elem.id == "newbinfile") {
        document.getElementById("uploadbin").value = default_path;
    }
}
function upload(elem) {
    var filePath = elem.value;
    var upload_path = "";
    var fileInput = "";
    if (elem.id == "uploadhtml") {
        upload_path = "/upload/html/" + filePath;
        fileInput = document.getElementById("newhtmlfile").files;
    } else if (elem.id == "uploadcert") {
        upload_path = "/upload/certs/" + filePath;
        fileInput = document.getElementById("newcertfile").files;
    } else if (elem.id == "uploadbin") {
        upload_path = "/upload/image/" + filePath;
        fileInput = document.getElementById("newbinfile").files;
    }

    /* Max size of an individual file. Make sure this
     * value is same as that set in file_server.c */
    var MAX_FILE_SIZE = 200*1024;
    var MAX_FILE_SIZE_STR = "200KB";

    if (fileInput.length == 0) {
        alert("No file selected!");
    } else if (filePath.length == 0) {
        alert("File path on server is not set!");
    } else if (filePath.indexOf(' ') >= 0) {
        alert("File path on server cannot have spaces!");
    } else if (filePath[filePath.length-1] == '/') {
        alert("File name not specified after path!");
    } else if (fileInput[0].size > 200*1024 && elem.id != "uploadbin") {
        alert("File size must be less than 200KB!");
    } else {
        elem.disabled = true;
        if (elem.id == "uploadhtml") {
            document.getElementById("newhtmlfile").disabled = true;
        } else if (elem.id == "uploadcert") {
            document.getElementById("newcertfile").disabled = true;
        } else if (elem.id == "uploadbin") {
            document.getElementById("newbinfile").disabled = true;
        }

        var file = fileInput[0];
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (xhttp.readyState == 4) {
                if (xhttp.status == 200) {
                    document.open();
                    document.write(xhttp.responseText);
                    document.close();
                } else if (xhttp.status == 0) {
                    alert("Server closed the connection abruptly!");
                    location.reload()
                } else {
                    alert(xhttp.status + " Error!\n" + xhttp.responseText);
                    location.reload()
                }
            }
        };
        xhttp.open("POST", upload_path, true);
        xhttp.send(file);
    }
}


