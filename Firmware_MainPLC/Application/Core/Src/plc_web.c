/*
 * plc_web.c — Main PLC HTTP server: sensor page and JSON API.
 */
#include "plc_web.h"

#include <stdio.h>
#include <string.h>
#include "lwip/api.h"
#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "cmsis_os2.h"
#include "plc_can.h"

#define PLC_WEB_PORT        80u
#define PLC_WEB_RX_TMO_MS   2000

static const char s_page[] =
  "<!doctype html><html lang=\"ru\"><head><meta charset=\"utf-8\">"
  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
  "<title>HVAC ПЛК</title><style>"
  "body{font-family:Segoe UI,Arial,sans-serif;background:#14181f;color:#e8e8e8;margin:24px}"
  ".row{margin:12px 0}.v{font-size:56px;font-weight:600}.u{font-size:24px;color:#9aa}"
  ".bad{color:#ff5a5a}.ok{color:#5ad17a}#d{color:#889;font-size:13px}"
  "</style></head><body>"
  "<h3>Главный ПЛК — датчик Rotronic HF523</h3>"
  "<div class=\"row\">Температура<br><span class=\"v\" id=\"t\">--</span> <span class=\"u\">&deg;C</span></div>"
  "<div class=\"row\">Влажность<br><span class=\"v\" id=\"h\">--</span> <span class=\"u\">%RH</span></div>"
  "<div class=\"row\" id=\"s\">Ожидание данных…</div><pre id=\"d\"></pre>"
  "<script>"
  "function f(v,bad){return bad?'--':v.toFixed(2);}"
  "function u(){fetch('/api/ai',{cache:'no-store'}).then(r=>r.json()).then(j=>{"
  "var nl=!j.link||(j.st&0x84);"
  "t.textContent=f(j.t,nl||(j.st&2));h.textContent=f(j.h,nl||(j.st&1));"
  "t.className='v'+((nl||(j.st&2))?' bad':'');h.className='v'+((nl||(j.st&1))?' bad':'');"
  "var m=[];"
  "m.push(j.link?'<span class=ok>Связь с локомотивом: есть</span>':'<span class=bad>Связь с локомотивом: нет</span>');"
  "if(j.st&0x80)m.push('<span class=bad>Модуль AI не отвечает локомотиву</span>');"
  "if(j.st&4)m.push('<span class=bad>АЦП модуля AI не отвечает</span>');"
  "if(j.st&2)m.push('<span class=bad>Обрыв канала температуры</span>');"
  "if(j.st&1)m.push('<span class=bad>Обрыв канала влажности</span>');"
  "if(!j.init)m.push('<span class=bad>CAN ПЛК не запустился</span>');"
  "s.innerHTML=m.join('<br>');"
  "d.textContent='счётчик AI '+j.cnt+' | возраст '+j.age+' мс | CAN1 принято '+j.rx_loco+"
  "' | CAN2 отправлено '+j.tx_hub+' ошибок '+j.tx_err+' принято '+j.rx_hub+"
  "' | потеряно '+j.drop+' | bus-off '+j.boff1+'/'+j.boff2;"
  "}).catch(()=>{s.innerHTML='<span class=bad>Нет ответа от ПЛК</span>';});}"
  "setInterval(u,1000);u();"
  "</script></body></html>";

static const char s_hdr_html[] =
  "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
  "Cache-Control: no-store\r\nConnection: close\r\n\r\n";

static const char s_hdr_json[] =
  "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
  "Cache-Control: no-store\r\nConnection: close\r\n\r\n";

static const char s_404[] =
  "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nNot found";

/* newlib-nano printf has no %f: print 0.01-unit fixed point. */
static int PlcWeb_FmtCenti(char *buf, size_t len, float v)
{
  int32_t c = hvac_can_to_centi(v);
  uint32_t a = (uint32_t)((c < 0) ? -c : c);
  return snprintf(buf, len, "%s%lu.%02lu", (c < 0) ? "-" : "",
                  (unsigned long)(a / 100u), (unsigned long)(a % 100u));
}

static void PlcWeb_SendJson(struct netconn *c)
{
  PlcAiSnapshot s;
  char t[16], h[16];
  char body[320];
  int n;

  PlcCan_GetSnapshot(&s);
  PlcWeb_FmtCenti(t, sizeof(t), s.meas.temperature_c);
  PlcWeb_FmtCenti(h, sizeof(h), s.meas.humidity_pct);

  n = snprintf(body, sizeof(body),
               "{\"t\":%s,\"h\":%s,\"st\":%u,\"cnt\":%u,\"link\":%u,\"init\":%u,"
               "\"age\":%lu,\"rx_loco\":%lu,\"tx_hub\":%lu,\"tx_err\":%lu,"
               "\"rx_hub\":%lu,\"drop\":%lu,\"boff1\":%u,\"boff2\":%u}",
               t, h, s.meas.status, s.meas.counter, s.loco_link, s.init_ok,
               (unsigned long)s.age_ms, (unsigned long)s.rx_loco, (unsigned long)s.tx_hub,
               (unsigned long)s.tx_hub_err, (unsigned long)s.rx_hub, (unsigned long)s.rx_drop,
               s.loco_bus_off, s.hub_bus_off);
  if (n < 0) return;
  if ((size_t)n >= sizeof(body)) n = (int)sizeof(body) - 1;

  netconn_write(c, s_hdr_json, sizeof(s_hdr_json) - 1u, NETCONN_COPY);
  netconn_write(c, body, (size_t)n, NETCONN_COPY);
}

static void PlcWeb_Serve(struct netconn *c)
{
  struct netbuf *nb = NULL;
  char *data;
  u16_t len;

  if (netconn_recv(c, &nb) != ERR_OK) return;
  netbuf_data(nb, (void **)&data, &len);

  if (len >= 11u && strncmp(data, "GET /api/ai", 11) == 0)
  {
    PlcWeb_SendJson(c);
  }
  else if (len >= 6u && (strncmp(data, "GET / ", 6) == 0 || strncmp(data, "GET /?", 6) == 0))
  {
    netconn_write(c, s_hdr_html, sizeof(s_hdr_html) - 1u, NETCONN_COPY);
    netconn_write(c, s_page, sizeof(s_page) - 1u, NETCONN_COPY);
  }
  else
  {
    netconn_write(c, s_404, sizeof(s_404) - 1u, NETCONN_COPY);
  }

  netbuf_delete(nb);
}

void PlcWeb_Task(void)
{
  struct netconn *srv;

  for (;;)
  {
    srv = netconn_new(NETCONN_TCP);
    if (srv != NULL)
    {
      ip_set_option(srv->pcb.tcp, SOF_REUSEADDR);
      if (netconn_bind(srv, IP_ADDR_ANY, PLC_WEB_PORT) == ERR_OK && netconn_listen(srv) == ERR_OK)
      {
        for (;;)
        {
          struct netconn *c = NULL;
          err_t e = netconn_accept(srv, &c);

          if (e == ERR_OK)
          {
            netconn_set_recvtimeout(c, PLC_WEB_RX_TMO_MS);
            PlcWeb_Serve(c);
            netconn_close(c);
            netconn_delete(c);
          }
          else if (e != ERR_TIMEOUT)
          {
            break;
          }
        }
      }
      netconn_delete(srv);
    }
    osDelay(1000);
  }
}
