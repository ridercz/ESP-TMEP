// Web server configuration
#define HTTP_PORT 80

// Common header of HTML pages
#define HTML_HEADER "<html>\
<head>\
<title>ESP-TMEP</title>\
<link rel=\"stylesheet\" type=\"text/css\" href=\"/styles.css\" />\
<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\
</head>\
<body>"

// Common footer of HTML pages
#define HTML_FOOTER "<footer><a href=\"https://github.com/ridercz/ESP-TMEP\">ESP-TMEP/" VERSION "</a> by <a href=\"https://www.rider.cz/\">Michal Altair Valasek</a></footer></body></html>"

// The stylesheet
#define HTML_CSS "html { font-family: Arial, Helvetica, sans-serif; font-size: 16px; background-color: #333; margin: 0; padding: 0; }\
body { background-color: #eee; color: #333; margin: 0 auto; padding: 0; text-align: center; max-width: 500px; min-height: 100vw; }\
h1 { font-size: 200%; font-weight: normal; margin: 0; padding: 1ex; background-color: #c00; color: #fff; }\
a:link, a:visited {color: #c00; }\
a:hover, a:active {color: #f00; }\
p { margin: 0; padding: 1ex; }\
p.curtemp { font-size: 300%; }\
p.link a { display: block; background-color: #fff; border: 1px solid #c00; padding: .5ex 1ex; width: 100%; text-decoration: none; margin-bottom: 1ex; box-sizing: border-box; }\
footer { font-size: 80%; border-top: 1px solid #c00; color: #c00; padding: 1ex;}"

// HTTP error page
#define HTML_404 "<h1>404 Object Not Found</h1><p>Requested page was not found.</p>"

// Static part of homepage
#define HTML_HOME "<p class=\"link\">\
<a href=\"/api\">REST API</a>\
<a href=\"javascript:resetConfig();\">Reset configuration</a>\
\</p>\
<script>function resetConfig() {\
  var pin = window.prompt('Enter config PIN:');\
  if(pin) window.location.href = '/reset?pin=' + pin;\
}</script>"
