# bme280
Arduino and linux I2C sketch for Bosch BME280 atmospheric preassure, temperature and humidity sensor. No external library is required for Arduino. Linux app uses i2c library.

It is written based on BME280 datasheet published at https://ae-bst.resource.bosch.com/media/_tech/media/datasheets/BST-BME280-DS002.pdf
and uses the same logic as the provided driver https://github.com/BoschSensortec/BME280_driver

Now in Raspberry Pi, it is easy to collect environmental data in any time series databases; for example, in RRD:

```
rrdtool create /var/env.rrd \
          --step 1m \
          DS:temperature:GAUGE:1d:-50:50 \
          RRA:AVERAGE:0.5:1m:1h \
          RRA:AVERAGE:0.5:1h:1d \
          RRA:AVERAGE:0.5:1h:1M \
          RRA:AVERAGE:0.5:1d:1y \
          RRA:AVERAGE:0.5:1d:10y \
          RRA:MIN:0.5:1m:1h \
          RRA:MIN:0.5:1h:1d \
          RRA:MIN:0.5:1h:1M \
          RRA:MIN:0.5:1d:1y \
          RRA:MIN:0.5:1d:10y \
          RRA:MAX:0.5:1m:1h \
          RRA:MAX:0.5:1h:1d \
          RRA:MAX:0.5:1h:1M \
          RRA:MAX:0.5:1d:1y \
          RRA:MAX:0.5:1d:10y \
          DS:humidity:GAUGE:1d:0:100 \
          RRA:AVERAGE:0.5:1m:1h \
          RRA:AVERAGE:0.5:1h:1d \
          RRA:AVERAGE:0.5:1h:1M \
          RRA:AVERAGE:0.5:1d:1y \
          RRA:AVERAGE:0.5:1d:10y \
          RRA:MIN:0.5:1m:1h \
          RRA:MIN:0.5:1h:1d \
          RRA:MIN:0.5:1h:1M \
          RRA:MIN:0.5:1d:1y \
          RRA:MIN:0.5:1d:10y \
          RRA:MAX:0.5:1m:1h \
          RRA:MAX:0.5:1h:1d \
          RRA:MAX:0.5:1h:1M \
          RRA:MAX:0.5:1d:1y \
          RRA:MAX:0.5:1d:10y \
          DS:pressure:GAUGE:1d:800:1200 \
          RRA:AVERAGE:0.5:1m:1h \
          RRA:AVERAGE:0.5:1h:1d \
          RRA:AVERAGE:0.5:1h:1M \
          RRA:AVERAGE:0.5:1d:1y \
          RRA:AVERAGE:0.5:1d:10y \
          RRA:MIN:0.5:1m:1h \
          RRA:MIN:0.5:1h:1d \
          RRA:MIN:0.5:1h:1M \
          RRA:MIN:0.5:1d:1y \
          RRA:MIN:0.5:1d:10y \
          RRA:MAX:0.5:1m:1h \
          RRA:MAX:0.5:1h:1d \
          RRA:MAX:0.5:1h:1M \
          RRA:MAX:0.5:1d:1y \
          RRA:MAX:0.5:1d:10y
          
cat bme280.php
<?php
$t = htmlspecialchars($_REQUEST['t']);
$h = htmlspecialchars($_REQUEST['h']);
$p = htmlspecialchars($_REQUEST['p']);
$values = array(sprintf("N:%.1f:%.1f:%.1f", $t, $h, $p));
if (!rrd_update("/var/env.rrd", $values)) {
  header('HTTP/1.1 500 Internal Server Error');
  print_r(rrd_error());
  exit;
}
?>

cat bme280.sh
#!/bin/bash
curl "http://example.com/bme280.php?$(./bme280 /dev/i2c-1 1 1 --raw)"

crontab -e
0-59 * * * * bme280.sh

```

Later you may graph the data:

```
cat /opt/nginx/html/env.php
<?php
$height = "-h 100";
$width = "-w 1000";
$start = "-s end-1w";
$end = "-e now";
$file_t = "/opt/nginx/html/images/temperature.png";
$file_h = "/opt/nginx/html/images/humidity.png";
$file_p = "/opt/nginx/html/images/pressure.png";
$def_t = "DEF:t=/var/env.rrd:temperature:LAST";
$def_h = "DEF:h=/var/env.rrd:humidity:LAST";
$def_p = "DEF:p=/var/env.rrd:pressure:LAST";
$line_t = "LINE1:t#0000FF:'temperature'";
$line_h = "LINE1:h#0000FF:'humidity'";
$line_p = "LINE1:p#0000FF:'pressure'";
$values = [$end, $start, $height, $width, "-t Temperature, deg C", $def_t, $line_t];
rrd_graph($file_t, $values);
print_r(rrd_error());
$values = [$end, $start, $height, $width, "-t Relative Humidity, %", $def_h, $line_h];
rrd_graph($file_h, $values);
print_r(rrd_error());
#$values = [$end, $start, $height, $width, "-l 990", "-u 1040", "-r", $def_p, $line_p];
$values = [$end, $start, $height, $width, "-t Atmospheric Pressure, mb(hPa)", "-A", "-Y", $def_p, $line_p];
rrd_graph($file_p, $values);
print_r(rrd_error());
?>
<html>
<head>
<meta http-equiv="Cache-control" content="no-store">
</head>
<body>
<img src="images/temperature.png" />
<br />
<img src="images/humidity.png" />
<br />
<img src="images/pressure.png" />
</body>
</html>

http://example.com/env.php
```
Or you may just get the last number of any metric using this PHP

```
cat env_lastupdate_temperature.php
<?php
$last_update = rrd_lastupdate("/var/env.rrd");
print_r($last_update["data"][0]);
?>
```

This can be useful in timer/relay applications that use ESP8266 wifi chip to get simple environmental number over http and act upon it. For example, turn on/off the heater when the temperature drops/exceeds certain limits, or turn on/off the fan when humidity rises/drops certain levels.
