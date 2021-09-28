<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0" charset=utf-8>
<title>%platform%. %modulename%</title>
<style type="text/css">
* {
	margin: 0;
	padding: 0;
	border: 0;
	vertical-align: baseline;
	box-sizing: border-box;
}
div#main {
	font: 14pt/12pt sans-serif;
	text-align: center;
	max-width: 1000px;
	height: auto;
	width: 35em;
	margin: 1em auto 0;
}
div#main p {
	padding: 0.4em;
}
div#main a {
	color: #333;
}
form div {
	display: inline-block;
	padding-bottom: 0.2em;
}
form div label {
	display: inline-block;
	width: 6em;
}
table.block {
	width: 90%%;
	margin: 0 auto;
}
table.block th {
	text-align: left;
	color: white;
	border: 1px solid #888;
	border-radius: 0.2em;
}
table.header td {
	padding: 0.5em;
}
div.header-left {
	text-align: right;
	vertical-align: top;
	float: none;
	display: inline-block;
}
div.header-right {
	font-size: 80%%;
	text-align: left;
	float: none;
	display: inline-block;
}
p.logo {
	font-size: 300%%;
	padding: 0.5em;
}
input {
	border: 1px black solid;
	border-radius: 0.2em;
	padding: 0.3em;
}
div.large-input {
	border: 2px black solid;
	border-radius: 0.5em;
	text-align: right;
	padding: 0.7em;
	margin-top: 0.5em;
	width: 90%%;
	font-size: 125%%;
	margin: 0 auto;
}
div#cold {
	border-color: #09c;
}
div#hot {
	border-color: #c63;
}
button.set-conf {
	margin-top: 2em;
	padding: 0.5em 3.5em;
	height: 2em;
	border: 1px solid #888;
	border-radius: 0.2em;
	font-size: 100%%;
}
button.settings-button {
	margin-top: 2em;
	width: 90%%;
	padding: 0.3em;
	height: 3em;
	border: 1px solid #888;
	border-radius: 0.2em;
	font-size: 120%%;
}
@media (max-width: 1024px) {
	div#main {
		width: 100%%;
		height: auto;
	}
}
@media (max-width: 768px) {
	div#main {
		width: 100%%;
		height: auto;
	}
}
@media (max-width: 480px) {
	div#main {
		width: 100%%;
		height: auto;
	}
	p.logo {
		font-size: 100%%;
		padding: 0.5em;
	}
}
@media (max-width: 320px) {
	div#main {
		width: 100%%;
		height: auto;
	}
	p.logo {
		font-size: 100%%;
		padding: 0.5em;
	}
}
</style></head>
<body>
<iframe width="0" height="0" border="0" name="dummyframe" id="dummyframe"></iframe>
<div id="main">
<p></p>
<div style="background-color: white; margin: 0.6em;">
<div class="header-left">
<p class="logo" style="padding-top: 0.5em;">
<strong>
<span style="color: #00008B">%firstname%</span>
<span style="color: #FF0000">%lastname%</span>
</strong>
</p>
<p>%platform%</p>
<p style="font-size: 80%%">%version%</p>
</div>
<div class="header-right">
<p><p>Free memory: %heap% bytes</p>
<p>Uptime: %uptime%</p>
<p>%volt% %rssi%</p><p>%localtime%</p>
</div>
</div>
<div>
<form action="/settings" target="dummyframe">
<table class="block">
<tr>
<th style="background-color: #999"><p>Admin auth settings:</p></th>
</tr>
<tr>
<td>
<p>
<div class="input-w">
<label for="login">Login:</label>
<input type="text" name="login" id="login" value="%adminlogin%" maxlength="15">
</div>
<div class="input-w">
<label for="password">Password:</label>
<input type="text" name="password" id="password" value="%adminpassword%" maxlength="15">
</div>
</p>
<p>
<input type="checkbox" name="fullsecurity" id="fullsecurity" value="true" %fullsecurity%>
<label for="fullsecurity">Full Security</label>
<input type="checkbox" name="configsecurity" id="configsecurity" value="true" %configsecurity%>
<label for="configsecurity">Config Security</label>
</p>
</td>
</tr>
</table>
<table class="block">
<tr>
<th style="background-color: #999"><p>WiFi options:</p></th>
</tr>
<tr>
<td>
<p>
<input type="radio" name="wifi-mode" id="wifi-mode-station" value="station" %wifistamode%>
<label for="wifi-mode-station">Station mode</label>
<input type="radio" name="wifi-mode" id="wifi-mode-ap" value="ap" %wifiapmode%>
<label for="wifi-mode-ap">AP mode</label>
</p>
<p>
<div class="input-w">
<label for="wifi-ap-name">AP name:</label>
<input type="text" name="wifi-ap-name" id="wifi-ap-name" value="%wifiapname%" maxlength="15">
</div>
<div class="input-w">
<label for="wifi-ap-pass">AP Pass:</label>
<input type="text" name="wifi-ap-pass" id="wifi-ap-pass" value="%wifiappassword%" maxlength="15" placeholder="8 characters minimum">
</div>
<div class="input-w">
<label for="wifi-ap-name">STA name:</label>
<input type="text" name="wifi-sta-name" id="wifi-sta-name" value="%wifistaname%" maxlength="15">
</div>
<div class="input-w">
<label for="wifi-ap-pass">STA Pass:</label>
<input type="text" name="wifi-sta-pass" id="wifi-sta-pass" value="%wifistapassword%" maxlength="15" placeholder="8 characters minimum">
</div>
</p>
</td>
</tr>
</table>
<table class="block">
<tr>
<th style="background-color: #999"><p>MQTT settings:</p></th>
</tr>
<tr>
<td>
<p>
<div class="input-w">
<label for="mqtt-user">User:</label>
<input type="text" name="mqtt-user" id="mqtt-user" value="%mqttuser%" maxlength="15">
</div>
<div class="input-w">
<label for="mqtt-pass">Password:</label>
<input type="text" name="mqtt-pass" id="mqtt-pass" value="%mqttpassword%" maxlength="15">
</div>
<div class="input-w">
<label for="mqtt-broker">Broker:</label>
<input type="text" name="mqtt-broker" id="mqtt-broker" value="%mqttbroker%" maxlength="31">
</div>
<div class="input-w">
<label for="mqtt-topic">Topic:</label>
<input type="text" name="mqtt-topic" id="mqtt-topic" value="%mqtttopic%" maxlength="63">
</div>
</p>
</td>
</tr>
</table>
<table class="block">
<tr>
<th style="background-color: #999"><p>NTP settings:</p></th>
</tr>
<tr>
<td>
<p>
<div class="input-w">
<label for="serv-ntp">NTP server:</label>
<input type="text" name="serv-ntp" id="serv-ntp" value="%ntpserver%" maxlength="31">
</div>
<div class="input-w">
<label for="gmt-zone">GMT zone:</label>
<input type="number" name="gmt-zone" id="gmt-zone" value="%timezone%" maxlength="2">
</div>
</p>
</td>
</tr>
</table>
<table class="block">
<tr>
<th style="background-color: #999"><p>WaterMeter module settings:</p></th>
</tr>
<tr>
<td>
<p>
<div class="input-w">
<label for="hotw">Hot liters:</label>
<input type="number" name="hotw" id="hotw" value="%hotwater%" max="4294967295">
</div>
<div class="input-w">
<label for="coldw">Cold liters:</label>
<input type="number" name="coldw" id="coldw" value="%coldwater%" max="4294967295">
</div>
<div class="input-w">
<label for="countw">Liters in pulse:</label>
<input type="number" name="countw" id="countw" value="%litersperpulse%" min="1" max="100">
</div>
</p>
<p>
<input type="checkbox" name="reboot" id="reboot" value="true">
<label for="reboot">Reboot</label>
<input type="checkbox" name="defconfig" id="defconfig" value="true">
<label for="defconfig">Default setting (New start)</label>
</p>
<p>
<button class="set-conf" name="set-conf" value="set">
Set config
</button></p>
<p><a href="/">Main</a>
<a href="/upload">Update Firmware</a>
<a href="/log">Log</a></p>
</p>
</td>
</tr>
</table>
</div>
</form>
</div><br><br>
</body>
</html>
