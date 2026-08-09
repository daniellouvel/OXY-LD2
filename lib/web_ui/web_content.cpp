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

// Palette/typo "instrument de bord" (retour utilisateur : page trop brute,
// veut un rendu plus pro) - fond quasi-noir a dominante bleue, accent cyan
// (deja utilise pour les liens de nav dans la version precedente), pastilles
// de statut colorees plutot que du texte gras brut, cartes pour regrouper
// chaque section. Aucune ressource externe (police/JS/CSS) - le CSP de la
// carte n'a de toute facon pas d'acces reseau sortant utile ici, tout reste
// auto-suffisant comme avant.
const char* kStyle =
    ":root{--bg:#0b0f14;--surface:#131a22;--border:#243040;--text:#e7edf3;"
    "--text-dim:#8fa1b3;--accent:#22d3ee;--ok:#34d399;--ok-bg:#0f2e26;"
    "--bad:#f87171;--bad-bg:#2e1414;--wait:#fbbf24;--wait-bg:#2e2610}"
    "*{box-sizing:border-box}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Arial,sans-serif;"
    "background:var(--bg);color:var(--text);margin:0}"
    ".wrap{max-width:640px;margin:0 auto;padding:20px 16px 40px}"
    ".brand{font-size:.8rem;letter-spacing:.12em;color:var(--text-dim);"
    "text-transform:uppercase;margin-bottom:10px}"
    "h1{font-size:1.4rem;margin:14px 0 16px}"
    "h2{font-size:1rem;color:var(--text-dim);text-transform:uppercase;"
    "letter-spacing:.06em;margin:0 0 12px}"
    "nav{display:flex;gap:4px;background:var(--surface);border:1px solid var(--border);"
    "border-radius:999px;padding:4px}"
    "nav a{flex:1;text-align:center;padding:8px 6px;border-radius:999px;"
    "color:var(--text-dim);text-decoration:none;font-weight:600;font-size:.9rem}"
    "nav a.active{background:var(--accent);color:#04252b}"
    ".card{background:var(--surface);border:1px solid var(--border);border-radius:14px;"
    "padding:16px 18px;margin-top:14px}"
    ".row{display:flex;align-items:center;justify-content:space-between;"
    "padding:6px 0}"
    ".label{color:var(--text-dim);font-size:.9rem}"
    ".value{font-variant-numeric:tabular-nums;font-weight:600}"
    "p{font-size:1.05rem;margin:8px 0}"
    "p.stat{font-size:1.3rem;font-weight:700;font-variant-numeric:tabular-nums;margin:6px 0}"
    ".pill{display:inline-block;padding:3px 12px;border-radius:999px;font-weight:700;"
    "font-size:.85rem}"
    ".status-ok{color:var(--ok);background:var(--ok-bg)}"
    ".status-bad{color:var(--bad);background:var(--bad-bg)}"
    ".status-wait{color:var(--wait);background:var(--wait-bg)}"
    "form{margin:0}"
    ".field{margin-top:10px}"
    "label{display:block;color:var(--text-dim);font-size:.9rem;margin-bottom:6px}"
    "button,select,input[type=number]{font-size:1rem;padding:9px 12px;border-radius:9px;"
    "border:1px solid var(--border);background:#0e141b;color:var(--text)}"
    "button{background:var(--accent);color:#04252b;font-weight:700;border:none;"
    "cursor:pointer}"
    "button:active{opacity:.85}"
    "input[type=number]{width:90px}"
    "table{border-collapse:collapse;width:100%;margin-top:4px;font-variant-numeric:"
    "tabular-nums}"
    "td,th{padding:7px 10px;border-bottom:1px solid var(--border);text-align:right}"
    "th{color:var(--text-dim);font-size:.8rem;text-transform:uppercase;"
    "letter-spacing:.04em;font-weight:600}"
    "td:first-child,th:first-child{text-align:left}"
    "tbody tr:hover{background:#0f161f}"
    ".hint{font-size:.85rem;color:var(--text-dim);margin-top:12px}";

}  // namespace

std::string page_shell(const std::string& title, const std::string& body_html) {
  std::string out;
  out += "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
  out += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  out += "<title>" + title + " - OXY-LD2</title><style>";
  out += kStyle;
  out += "</style></head><body><div class=\"wrap\">";
  out += "<div class=\"brand\">OXY-LD2</div>";
  out += "<nav>";
  out += fmt("<a href=\"/materiel\"%s>Materiel</a>", title == "Materiel" ? " class=\"active\"" : "");
  out += fmt("<a href=\"/plongee\"%s>Plongee</a>", title == "Plongee" ? " class=\"active\"" : "");
  out += fmt("<a href=\"/tables\"%s>Tables</a>", title == "Tables MOD" ? " class=\"active\"" : "");
  out += "</nav>";
  out += "<h1>" + title + "</h1>";
  out += body_html;
  out += "</div></body></html>";
  return out;
}

std::string build_hardware_page(const HardwareStatus& status) {
  std::string body;

  body += "<div class=\"card\"><div class=\"row\"><span class=\"label\">ADS1115 (cellule O2)</span>";
  body += status.ads_ok ? "<span class=\"pill status-ok\">detecte</span>"
                         : "<span class=\"pill status-bad\">absent</span>";
  body += "</div>";
  if (status.ads_ok) {
    body += "<div class=\"row\"><span class=\"label\">Derniere tension lue</span>";
    body += fmt("<span class=\"value\">%.3f mV</span></div>", static_cast<double>(status.last_v_mv));
  }
  body += "</div>";

  body += "<div class=\"card\"><div class=\"row\"><span class=\"label\">RTC (horloge)</span>";
  if (status.rtc_ok) {
    body += "<span class=\"pill status-ok\">detecte</span>";
  } else {
    body += "<span class=\"pill status-bad\">absent</span>";
  }
  body += "</div>";
  if (status.rtc_ok) {
    body += "<div class=\"row\"><span class=\"label\">Heure</span>";
    body += fmt("<span class=\"value\">%02d:%02d</span></div>", status.rtc_hour, status.rtc_minute);
  }
  body += "</div>";

  return page_shell("Materiel", body);
}

std::string build_dive_page(const DiveStatus& status) {
  std::string body;

  body += "<div class=\"card\">";
  body += "<h2>ppO2 de reference</h2>";
  body += "<form method=\"POST\" action=\"/ppo2\"><div class=\"row\"><select name=\"ppo2\">";
  body += fmt("<option value=\"1.4\"%s>1.4</option>",
              (status.ppo2_setpoint < 1.5f) ? " selected" : "");
  body += fmt("<option value=\"1.6\"%s>1.6</option>",
              (status.ppo2_setpoint >= 1.5f) ? " selected" : "");
  body += "</select><button type=\"submit\">Appliquer</button></div></form>";
  body += "</div>";

  body += "<div class=\"card\"><h2>Mesure courante</h2>";
  if (status.o2_percent >= 0) {
    body += fmt("<p class=\"stat\">O2 : %d %%</p>", status.o2_percent);
    body += status.mod_meters >= 0 ? fmt("<p class=\"stat\">MOD : %d m</p>", status.mod_meters)
                                    : "<p class=\"stat\">MOD : --</p>";
    body += "<div class=\"row\"><span class=\"label\">Stabilite</span>";
    body += status.stable ? "<span class=\"pill status-ok\">OK</span>"
                           : "<span class=\"pill status-wait\">en cours...</span>";
    body += "</div>";
  } else {
    body += "<p>Pas de mesure valide pour l'instant.</p>";
  }
  body += "</div>";

  body += "<div class=\"card\"><h2>Calibration</h2>";
  body += "<div class=\"row\"><span class=\"label\">Etat</span>";
  body += status.calibrated ? "<span class=\"pill status-ok\">calibre</span>"
                             : "<span class=\"pill status-bad\">non calibre</span>";
  body += "</div>";
  body += "<div class=\"field\"><form method=\"POST\" action=\"/calibrate\">";
  body += "<button type=\"submit\">Calibrer maintenant</button>";
  body += "</form></div>";
  body +=
      "<p class=\"hint\">Exposer la cellule a l'air ambiant et attendre que la stabilite passe "
      "a OK AVANT de cliquer - une demande faite pendant que la mesure bouge encore est ignoree "
      "silencieusement (pas de file d'attente), reessayer une fois stable.</p>";

  body += "<div class=\"field\"><form method=\"POST\" action=\"/rest\">";
  body += "<label for=\"rest_hours\">Temps de repos avant auto-calibration (heures)</label>";
  body += "<div class=\"row\">";
  body += fmt("<input type=\"number\" id=\"rest_hours\" name=\"rest_hours\" min=\"1\" max=\"500\" value=\"%lu\">",
              static_cast<unsigned long>(status.rest_hours));
  body += "<button type=\"submit\">Appliquer</button>";
  body += "</div></form></div>";
  body +=
      "<p class=\"hint\">Duree minimale depuis la derniere calibration avant que "
      "l'auto-calibration au demarrage (ou pendant le fonctionnement) puisse se declencher - "
      "protection contre un redemarrage rapide avec du gaz residuel dans le circuit.</p>";
  body += "</div>";

  return page_shell("Plongee", body);
}

std::string build_tables_page() {
  std::string body = "<div class=\"card\">";
  body +=
      "<p>Table MOD (Maximum Operating Depth), generee et validee - voir "
      "docs/mod_table_review.html.</p>";
  body += "<table><thead><tr><th>O2 %</th><th>MOD @ 1.4</th><th>MOD @ 1.6</th></tr></thead><tbody>";

  for (int o2 = kModTableMinO2; o2 <= kModTableMaxO2; ++o2) {
    int mod14 = mod_lookup(o2, PPO2Setpoint::P14);
    int mod16 = mod_lookup(o2, PPO2Setpoint::P16);
    body += fmt("<tr><td>%d%%</td><td>%d m</td><td>%d m</td></tr>", o2, mod14, mod16);
  }

  body += "</tbody></table></div>";
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
