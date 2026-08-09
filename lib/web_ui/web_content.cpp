#include "web_content.h"

#include <cstdarg>
#include <cstdio>

namespace {

std::string fmt(const char* format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  return std::string(buf);
}

const char* kStyle =
    "body{font-family:sans-serif;background:#111;color:#eee;margin:0;padding:16px}"
    "nav{margin-bottom:16px}nav a{color:#4fc3e0;margin-right:14px;text-decoration:none;"
    "font-weight:bold}"
    "table{border-collapse:collapse;width:100%;margin-top:8px}"
    "td,th{padding:4px 10px;border:1px solid #444;text-align:right}"
    "th{background:#222}td:first-child,th:first-child{text-align:left}"
    ".status-ok{color:#4caf50;font-weight:bold}.status-bad{color:#e53935;font-weight:bold}"
    "button,select{font-size:1rem;padding:6px 10px}"
    "p{font-size:1.1rem}"
    ".cal-status{display:block;margin-top:6px;font-size:.8rem;font-weight:normal;opacity:.85}"
    ".hint{font-size:.85rem;color:#888}";

}  // namespace

std::string page_shell(const std::string& title, const std::string& body_html) {
  std::string out;
  out += "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
  out += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  out += "<title>" + title + " - OXY-LD2</title><style>";
  out += kStyle;
  out += "</style></head><body>";
  out +=
      "<nav><a href=\"/materiel\">Materiel</a><a href=\"/plongee\">Plongee</a>"
      "<a href=\"/tables\">Tables</a></nav>";
  out += "<h1>" + title + "</h1>";
  out += body_html;
  out += "</body></html>";
  return out;
}

std::string build_hardware_page(const HardwareStatus& status) {
  std::string body;

  body += "<p>ADS1115 (cellule O2) : ";
  body += status.ads_ok ? "<span class=\"status-ok\">detecte</span>"
                         : "<span class=\"status-bad\">absent</span>";
  body += "</p>";
  if (status.ads_ok) {
    body += fmt("<p>Derniere tension lue : %.3f mV</p>", static_cast<double>(status.last_v_mv));
  }

  body += "<p>RTC (horloge) : ";
  if (status.rtc_ok) {
    body += "<span class=\"status-ok\">detecte</span> - heure ";
    body += fmt("%02d:%02d", status.rtc_hour, status.rtc_minute);
  } else {
    body += "<span class=\"status-bad\">absent</span>";
  }
  body += "</p>";

  return page_shell("Materiel", body);
}

std::string build_dive_page(const DiveStatus& status) {
  std::string body;

  body += "<form method=\"POST\" action=\"/ppo2\">";
  body += "<label>ppO2 de reference : <select name=\"ppo2\">";
  body += fmt("<option value=\"1.4\"%s>1.4</option>",
              (status.ppo2_setpoint < 1.5f) ? " selected" : "");
  body += fmt("<option value=\"1.6\"%s>1.6</option>",
              (status.ppo2_setpoint >= 1.5f) ? " selected" : "");
  body += "</select></label> <button type=\"submit\">Appliquer</button></form>";

  body += "<h2>Mesure courante</h2>";
  if (status.o2_percent >= 0) {
    body += fmt("<p>O2 : %d %%</p>", status.o2_percent);
    body += status.mod_meters >= 0 ? fmt("<p>MOD : %d m</p>", status.mod_meters)
                                    : "<p>MOD : --</p>";
    body += status.stable ? "<p>Stabilite : <span class=\"status-ok\">OK</span></p>"
                           : "<p>Stabilite : en cours...</p>";
  } else {
    body += "<p>Pas de mesure valide pour l'instant.</p>";
  }

  body += "<h2>Calibration</h2>";
  body += "<form method=\"POST\" action=\"/calibrate\">";
  body += "<button type=\"submit\">Calibrer maintenant</button>";
  body += "</form>";
  body += status.calibrated ? "<div class=\"cal-status status-ok\">calibre</div>"
                             : "<div class=\"cal-status status-bad\">non calibre</div>";
  body +=
      "<p class=\"hint\">Exposer la cellule a l'air ambiant et attendre que la stabilite passe "
      "a OK AVANT de cliquer - une demande faite pendant que la mesure bouge encore est ignoree "
      "silencieusement (pas de file d'attente), reessayer une fois stable.</p>";

  return page_shell("Plongee", body);
}

std::string build_tables_page() {
  std::string body =
      "<p>Table MOD (Maximum Operating Depth), generee et validee - voir "
      "docs/mod_table_review.html.</p>";
  body += "<table><thead><tr><th>O2 %</th><th>MOD @ 1.4</th><th>MOD @ 1.6</th></tr></thead><tbody>";

  for (int o2 = kModTableMinO2; o2 <= kModTableMaxO2; ++o2) {
    int mod14 = mod_lookup(o2, PPO2Setpoint::P14);
    int mod16 = mod_lookup(o2, PPO2Setpoint::P16);
    body += fmt("<tr><td>%d%%</td><td>%d m</td><td>%d m</td></tr>", o2, mod14, mod16);
  }

  body += "</tbody></table>";
  return page_shell("Tables MOD", body);
}

std::string build_status_json(const HardwareStatus& hw, const DiveStatus& dive) {
  return fmt(
      "{\"o2\":%d,\"mod\":%d,\"ppo2\":%.1f,\"stable\":%s,\"calibrated\":%s,\"ads_ok\":%s,"
      "\"rtc_ok\":%s}",
      dive.o2_percent, dive.mod_meters, static_cast<double>(dive.ppo2_setpoint),
      dive.stable ? "true" : "false", dive.calibrated ? "true" : "false",
      hw.ads_ok ? "true" : "false", hw.rtc_ok ? "true" : "false");
}
