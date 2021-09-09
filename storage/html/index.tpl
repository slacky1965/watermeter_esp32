
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0" charset=utf-8>
<meta charset="UTF-8">
<meta http-equiv="refresh" content="15";URL="/"><title>%platform%. %modulename%</title>
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
<p><div class="large-input" id="cold">%coldwhole%.%coldfract%</div></p>
<p><div class="large-input" id="hot">%hotwhole%.%hotfract%</div></p>
<form>
<input type="button" class="settings-button" onclick="window.location='/config';" value="Configure Settings">
</form>
</div>
</div>
</body>
</html>
