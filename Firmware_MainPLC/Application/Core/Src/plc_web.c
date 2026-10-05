/*
 * plc_web.c — Main PLC HTTP server: sensor page, JSON API, firmware store API.
 *
 *   GET  /                 sensor page
 *   GET  /api/ai           sensor JSON
 *   GET  /api/fw/status?log=N   store, roll call, update run, log entries with id > N
 *   POST /api/fw/upload?size=N&crc=HEX   body = module .bin (size multiple of 8) -> NEW slot
 *   POST /api/fw/run?type=T&mode=M      update run (PLC_FWUPD_MODE_*)
 *   POST /api/fw/cancel
 *   POST /api/cfg?size=N&crc=HEX        body = configuration file (hvac_cfg.h) -> stored and applied
 *   GET  /api/cfg/status                configuration, module check
 */
#include "plc_web.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lwip/api.h"
#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "cmsis_os2.h"
#include "main.h"
#include "plc_can.h"
#include "plc_fwupd.h"
#include "plc_fwstore.h"
#include "plc_w25q.h"
#include "plc_cfg.h"

#define PLC_WEB_PORT        80u
#define PLC_WEB_RX_TMO_MS   5000
#define PLC_WEB_HDR_MAX     1024u
#define PLC_WEB_JSON_MAX    6144u

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

static const char s_400[] =
  "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nBad request";

typedef struct
{
  char     method[8];
  char     path[48];
  char     query[96];
  uint32_t content_length;
} WebReq;

static char s_hdr[PLC_WEB_HDR_MAX + 1u];
static char s_json[PLC_WEB_JSON_MAX];
static uint32_t s_json_len;
static uint32_t s_boot_key;

/* ---------- helpers ---------- */

static void Json_Add(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void Json_Add(const char *fmt, ...)
{
  va_list ap;
  int n;

  if (s_json_len >= sizeof(s_json) - 1u) return;
  va_start(ap, fmt);
  n = vsnprintf(&s_json[s_json_len], sizeof(s_json) - s_json_len, fmt, ap);
  va_end(ap);
  if (n > 0) s_json_len += (uint32_t)n;
  if (s_json_len > sizeof(s_json) - 1u) s_json_len = sizeof(s_json) - 1u;
}

static void Json_Send(struct netconn *c)
{
  netconn_write(c, s_hdr_json, sizeof(s_hdr_json) - 1u, NETCONN_COPY);
  netconn_write(c, s_json, s_json_len, NETCONN_COPY);
}

/* Query parameter as unsigned number; hex if base 16. Returns def if missing. */
static uint32_t Query_U32(const char *q, const char *key, int base, uint32_t def)
{
  size_t kl = strlen(key);
  const char *p = q;

  while (p != NULL && *p != '\0')
  {
    if (strncmp(p, key, kl) == 0 && p[kl] == '=') return (uint32_t)strtoul(&p[kl + 1u], NULL, base);
    p = strchr(p, '&');
    if (p != NULL) p++;
  }
  return def;
}

static uint8_t Req_Parse(WebReq *r)
{
  char *sp1 = strchr(s_hdr, ' ');
  char *sp2;
  char *q;
  char *cl;
  size_t n;

  memset(r, 0, sizeof(*r));
  if (sp1 == NULL || (size_t)(sp1 - s_hdr) >= sizeof(r->method)) return 0u;
  memcpy(r->method, s_hdr, (size_t)(sp1 - s_hdr));
  sp2 = strchr(sp1 + 1, ' ');
  if (sp2 == NULL) return 0u;
  *sp2 = '\0';
  q = strchr(sp1 + 1, '?');
  if (q != NULL)
  {
    *q = '\0';
    strncpy(r->query, q + 1, sizeof(r->query) - 1u);
  }
  n = strlen(sp1 + 1);
  if (n >= sizeof(r->path)) return 0u;
  memcpy(r->path, sp1 + 1, n);

  cl = strstr(sp2 + 1, "Content-Length:");
  if (cl == NULL) cl = strstr(sp2 + 1, "content-length:");
  if (cl != NULL) r->content_length = (uint32_t)strtoul(cl + 15, NULL, 10);
  return 1u;
}

/* ---------- sensor ---------- */

/* newlib-nano printf has no %f: print 0.01-unit fixed point. */
static int PlcWeb_FmtCenti(char *buf, size_t len, float v)
{
  int32_t c = hvac_can_to_centi(v);
  uint32_t a = (uint32_t)((c < 0) ? -c : c);
  return snprintf(buf, len, "%s%lu.%02lu", (c < 0) ? "-" : "",
                  (unsigned long)(a / 100u), (unsigned long)(a % 100u));
}

static void PlcWeb_SendAi(struct netconn *c)
{
  PlcAiSnapshot s;
  char t[16], h[16];

  PlcCan_GetSnapshot(&s);
  PlcWeb_FmtCenti(t, sizeof(t), s.meas.temperature_c);
  PlcWeb_FmtCenti(h, sizeof(h), s.meas.humidity_pct);

  s_json_len = 0u;
  Json_Add("{\"t\":%s,\"h\":%s,\"st\":%u,\"cnt\":%u,\"link\":%u,\"init\":%u,"
           "\"age\":%lu,\"rx_loco\":%lu,\"tx_hub\":%lu,\"tx_err\":%lu,"
           "\"rx_hub\":%lu,\"drop\":%lu,\"boff1\":%u,\"boff2\":%u}",
           t, h, s.meas.status, s.meas.counter, s.loco_link, s.init_ok,
           (unsigned long)s.age_ms, (unsigned long)s.rx_loco, (unsigned long)s.tx_hub,
           (unsigned long)s.tx_hub_err, (unsigned long)s.rx_hub, (unsigned long)s.rx_drop,
           s.loco_bus_off, s.hub_bus_off);
  Json_Send(c);
}

/* ---------- firmware API ---------- */

static void PlcWeb_FwStatus(struct netconn *c, const WebReq *r)
{
  static PlcFwupdLog log[PLC_FWUPD_LOG_LEN];
  uint32_t after = Query_U32(r->query, "log", 10, 0u);
  uint32_t now = osKernelGetTickCount();
  uint32_t nlog;
  const char *sep = "";

  s_json_len = 0u;
  Json_Add("{\"up\":%lu,\"boot\":%lu,\"store\":%u,\"jedec\":\"%06lx\",\"upload\":%u,",
           (unsigned long)now, (unsigned long)s_boot_key, g_plc_fwupd.store_ok,
           (unsigned long)W25q_JedecId(), FwStore_UploadActive());
  Json_Add("\"run\":{\"active\":%u,\"type\":%u,\"mode\":%u,\"ver\":%u,\"done\":%u,\"failed\":%u},",
           g_plc_fwupd.run_active, g_plc_fwupd.run_type, g_plc_fwupd.run_mode, g_plc_fwupd.run_version,
           g_plc_fwupd.run_done, g_plc_fwupd.run_failed);
  Json_Add("\"busy\":%u,\"busyTag\":\"%06lx\",\"step\":%u,\"progress\":%u,",
           g_plc_fwupd.busy, (unsigned long)g_plc_fwupd.busy_tag, g_plc_fwupd.step, g_plc_fwupd.progress);
  Json_Add("\"stats\":{\"ok\":%lu,\"failed\":%lu,\"retries\":%lu,\"rx\":%lu,\"drop\":%lu},",
           (unsigned long)g_plc_fwupd.updates_ok, (unsigned long)g_plc_fwupd.updates_failed,
           (unsigned long)g_plc_fwupd.block_retries, (unsigned long)g_plc_fwupd.rx_frames,
           (unsigned long)g_plc_fwupd.rx_drop);

  Json_Add("\"slots\":[");
  for (uint8_t t = 1u; t < FWSTORE_TYPES; t++)
  {
    for (uint8_t i = 0u; i < 2u; i++)
    {
      FwStoreSlot s;

      FwStore_GetSlot(t, i, &s);
      if (s.role == FWSTORE_ROLE_EMPTY) continue;
      Json_Add("%s{\"type\":%u,\"idx\":%u,\"role\":%u,\"board\":%u,\"ver\":%u,\"size\":%lu,\"crc\":\"%08lx\"}",
               sep, t, i, s.role, s.board_rev, s.version, (unsigned long)s.size, (unsigned long)s.crc);
      sep = ",";
    }
  }

  Json_Add("],\"nodes\":[");
  sep = "";
  for (uint32_t i = 0u; i < PLC_FWUPD_MAX_NODES; i++)
  {
    volatile PlcFwupdNode *n = &g_plc_fwupd.nodes[i];

    if (n->tag == 0u) continue;
    Json_Add("%s{\"tag\":\"%06lx\",\"type\":%u,\"board\":%u,\"state\":%u,\"boot\":%u,\"ver\":%u,\"fails\":%u,"
             "\"link\":%u,\"mismatch\":%u,\"result\":%u,\"attempts\":%u,\"err\":%u,\"step\":%u,\"age\":%lu}",
             sep, (unsigned long)n->tag, n->module_type, n->board_rev, n->state, n->boot_version, n->app_version,
             n->boot_fails, n->link, n->version_mismatch, n->result, n->attempts, n->last_err, n->last_step,
             (unsigned long)(now - n->last_seen));
    sep = ",";
  }

  Json_Add("],\"log\":[");
  sep = "";
  nlog = PlcFwupd_GetLog(after, log, PLC_FWUPD_LOG_LEN);
  for (uint32_t i = 0u; i < nlog; i++)
  {
    Json_Add("%s{\"id\":%lu,\"t\":%lu,\"tag\":\"%06lx\",\"type\":%u,\"ev\":%u,\"err\":%u,\"step\":%u,\"from\":%u,\"to\":%u}",
             sep, (unsigned long)log[i].id, (unsigned long)log[i].t_ms, (unsigned long)log[i].tag, log[i].module_type,
             log[i].event, log[i].err, log[i].step, log[i].ver_from, log[i].ver_to);
    sep = ",";
  }
  Json_Add("]}");
  Json_Send(c);
}

static void PlcWeb_FwResult(struct netconn *c, uint8_t err)
{
  s_json_len = 0u;
  if (err == 0u) Json_Add("{\"ok\":true}");
  else           Json_Add("{\"ok\":false,\"err\":%u}", err);
  Json_Send(c);
}

static void PlcWeb_FwRun(struct netconn *c, const WebReq *r)
{
  uint8_t type = (uint8_t)Query_U32(r->query, "type", 10, 0u);
  uint8_t mode = (uint8_t)Query_U32(r->query, "mode", 10, 0u);

  PlcWeb_FwResult(c, PlcFwupd_RequestRun(type, mode));
}

static void PlcWeb_FwUploadDone(struct netconn *c, uint8_t err)
{
  FwStoreSlot s;

  if (err == FWSTORE_OK) err = FwStore_UploadEnd(&s);
  else FwStore_UploadAbort();

  s_json_len = 0u;
  if (err == FWSTORE_OK)
  {
    PlcFwupd_Log(PLC_FWUPD_EV_UPLOAD_OK, 0u, s.module_type, 0u, 0u, 0u, s.version);
    Json_Add("{\"ok\":true,\"type\":%u,\"board\":%u,\"ver\":%u,\"size\":%lu,\"crc\":\"%08lx\"}",
             s.module_type, s.board_rev, s.version, (unsigned long)s.size, (unsigned long)s.crc);
  }
  else
  {
    PlcFwupd_Log(PLC_FWUPD_EV_UPLOAD_FAIL, 0u, 0u, err, 0u, 0u, 0u);
    Json_Add("{\"ok\":false,\"err\":%u}", err);
  }
  Json_Send(c);
}

/* ---------- configuration API ---------- */

static void PlcWeb_CfgStatus(struct netconn *c)
{
  PlcCfgSummary s;
  const char *sep = "";

  PlcCfg_GetSummary(&s);
  s_json_len = 0u;
  Json_Add("{\"up\":%lu,\"flash\":%u,\"present\":%u,\"gen\":%u,\"state\":%u,\"size\":%lu,\"crc\":\"%08lx\","
           "\"project\":\"%s\",\"modules\":%u,\"channels\":%u,\"applied\":%lu,\"resends\":%lu,"
           "\"extraLoco\":%u,\"extraAi\":%u,\"plcVer\":%u,\"mods\":[",
           (unsigned long)osKernelGetTickCount(), s.flash_ok, s.present, s.gen, s.state, (unsigned long)s.size,
           (unsigned long)s.crc, s.project, s.module_count, s.channel_count, (unsigned long)s.applied_ms,
           (unsigned long)s.resends, s.extra_loco, s.extra_ai, PLC_FW_VERSION);
  for (uint16_t i = 0u; i < s.module_count; i++)
  {
    HvacCfgModule m;
    PlcCfgModState st;

    if (!PlcCfg_GetModule(i, &m, &st)) break;
    Json_Add("%s{\"type\":%u,\"l\":%u,\"r\":%u,\"p\":%u,\"st\":%u,\"err\":%u,\"ok\":%u,\"bad\":%u,\"ver\":%u}",
             sep, m.type, m.line, m.rail, m.place, st.state, st.err, st.ok_mask, st.bad_mask, st.version);
    sep = ",";
  }
  Json_Add("]}");
  Json_Send(c);
}

static void PlcWeb_CfgUploadDone(struct netconn *c, uint8_t err)
{
  uint8_t gen = 0u;

  if (err == PLC_CFG_OK) err = PlcCfg_UploadEnd(&gen);
  s_json_len = 0u;
  if (err == PLC_CFG_OK) Json_Add("{\"ok\":true,\"gen\":%u}", gen);
  else                   Json_Add("{\"ok\":false,\"err\":%u}", err);
  Json_Send(c);
}

/* ---------- request handling ---------- */

#define BODY_NONE    0u
#define BODY_UPLOAD  1u
#define BODY_DRAIN   2u
#define BODY_CFG     3u

static void PlcWeb_Serve(struct netconn *c)
{
  struct netbuf *nb = NULL;
  WebReq r;
  uint32_t hlen = 0u;
  uint8_t hdr_done = 0u;
  uint8_t body = BODY_NONE;
  uint8_t up_err = FWSTORE_OK;
  uint32_t got = 0u;

  for (;;)
  {
    if (netconn_recv(c, &nb) != ERR_OK)
    {
      if (body == BODY_UPLOAD) FwStore_UploadAbort();
      return;
    }

    do
    {
      uint8_t *p;
      u16_t dl;

      netbuf_data(nb, (void **)&p, &dl);
      while (dl > 0u)
      {
        if (!hdr_done)
        {
          char *end;

          while (dl > 0u && hlen < PLC_WEB_HDR_MAX)
          {
            s_hdr[hlen++] = (char)*p++;
            dl--;
            if (hlen >= 4u && memcmp(&s_hdr[hlen - 4u], "\r\n\r\n", 4u) == 0) break;
          }
          s_hdr[hlen] = '\0';
          end = strstr(s_hdr, "\r\n\r\n");
          if (end == NULL)
          {
            if (hlen >= PLC_WEB_HDR_MAX) { netbuf_delete(nb); netconn_write(c, s_400, sizeof(s_400) - 1u, NETCONN_COPY); return; }
            continue;
          }
          hdr_done = 1u;
          if (!Req_Parse(&r)) { netbuf_delete(nb); netconn_write(c, s_400, sizeof(s_400) - 1u, NETCONN_COPY); return; }

          if (strcmp(r.method, "POST") == 0 && strcmp(r.path, "/api/fw/upload") == 0)
          {
            uint32_t size = Query_U32(r.query, "size", 10, 0u);

            if (g_plc_fwupd.run_active) up_err = FWSTORE_E_BUSY;
            else if (size != r.content_length) up_err = FWSTORE_E_SIZE;
            else up_err = FwStore_UploadBegin(size, Query_U32(r.query, "crc", 16, 0u));
            body = (up_err == FWSTORE_OK) ? BODY_UPLOAD : BODY_DRAIN;
          }
          else if (strcmp(r.method, "POST") == 0 && strcmp(r.path, "/api/cfg") == 0)
          {
            uint32_t size = Query_U32(r.query, "size", 10, 0u);

            up_err = (size != r.content_length) ? PLC_CFG_E_SIZE
                                                : PlcCfg_UploadBegin(size, Query_U32(r.query, "crc", 16, 0u));
            body = (up_err == PLC_CFG_OK) ? BODY_CFG : BODY_DRAIN;
          }
          else if (r.content_length > 0u)
          {
            body = BODY_DRAIN;
          }
        }
        else
        {
          uint32_t n = r.content_length - got;

          if (n > dl) n = dl;
          if (body == BODY_UPLOAD && up_err == FWSTORE_OK) up_err = FwStore_UploadWrite(p, n);
          if (body == BODY_CFG) PlcCfg_UploadWrite(p, n);
          got += n;
          p += n;
          dl = (u16_t)(dl - n);
          if (got >= r.content_length) dl = 0u;
        }
      }
    } while (netbuf_next(nb) >= 0);
    netbuf_delete(nb);

    if (hdr_done && got >= r.content_length) break;
  }

  if (s_boot_key == 0u) s_boot_key = (HAL_GetUIDw0() ^ (SysTick->VAL << 12) ^ osKernelGetTickCount()) | 1u;

  if (strcmp(r.path, "/api/fw/upload") == 0 && strcmp(r.method, "POST") == 0)
  {
    PlcWeb_FwUploadDone(c, up_err);
  }
  else if (strcmp(r.path, "/api/cfg") == 0 && strcmp(r.method, "POST") == 0)
  {
    PlcWeb_CfgUploadDone(c, up_err);
  }
  else if (strcmp(r.method, "GET") == 0 && strcmp(r.path, "/api/cfg/status") == 0)
  {
    PlcWeb_CfgStatus(c);
  }
  else if (strcmp(r.method, "GET") == 0 && strcmp(r.path, "/api/fw/status") == 0)
  {
    PlcWeb_FwStatus(c, &r);
  }
  else if (strcmp(r.method, "POST") == 0 && strcmp(r.path, "/api/fw/run") == 0)
  {
    PlcWeb_FwRun(c, &r);
  }
  else if (strcmp(r.method, "POST") == 0 && strcmp(r.path, "/api/fw/cancel") == 0)
  {
    PlcFwupd_Cancel();
    PlcWeb_FwResult(c, 0u);
  }
  else if (strcmp(r.method, "GET") == 0 && strcmp(r.path, "/api/ai") == 0)
  {
    PlcWeb_SendAi(c);
  }
  else if (strcmp(r.method, "GET") == 0 && strcmp(r.path, "/") == 0)
  {
    netconn_write(c, s_hdr_html, sizeof(s_hdr_html) - 1u, NETCONN_COPY);
    netconn_write(c, s_page, sizeof(s_page) - 1u, NETCONN_COPY);
  }
  else
  {
    netconn_write(c, s_404, sizeof(s_404) - 1u, NETCONN_COPY);
  }
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
