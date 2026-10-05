/*
 * plc_cfg.c — system configuration: storage in W25Q128, module check, settings to modules.
 */
#include "plc_cfg.h"

#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "fwupd/fwupd_proto.h"
#include "plc_can.h"
#include "plc_fwupd.h"
#include "plc_fwstore.h"
#include "plc_w25q.h"

/* Two copies in the reserved first megabyte of W25Q128 (module images start at 1 MB). */
#define CFG_FLASH_BASE        0x00000000u
#define CFG_SLOT_SIZE         0x4000u
#define CFG_SLOT_HDR          32u
#define CFG_SLOT_MAGIC        0x53464348u /* "HCFS" */

#define CFG_EVAL_MS           200u
#define CFG_FLASH_WAIT_MS     10000u /* no W25Q128 after this: run without a stored file */
#define CFG_STATUS_FRESH_MS   3000u
#define CFG_RESEND_MS         1000u
#define CFG_CONFIRM_TRIES     5u

typedef struct
{
  uint32_t magic;
  uint32_t seq;
  uint32_t size;
  uint32_t crc;
  uint8_t  gen;
  uint8_t  reserved[3];
  uint32_t check;
} CfgSlotHdr;

static osMutexId_t s_mx;

static uint8_t  s_act[HVAC_CFGF_MAX_SIZE];
static uint32_t s_act_len;
static uint32_t s_act_crc;
static uint8_t  s_gen;
static uint32_t s_seq;
static int8_t   s_slot = -1;      /* flash copy holding the active file */
static uint8_t  s_loaded;
static uint8_t  s_flash_ok;

static uint8_t  s_up[HVAC_CFGF_MAX_SIZE];
static uint32_t s_up_size;
static uint32_t s_up_crc;
static uint32_t s_up_got;

static PlcCfgModState s_mod[HVAC_CFGF_MAX_MODULES];
static uint8_t  s_state;
static uint32_t s_applied_ms;
static uint32_t s_resends;
static uint8_t  s_extra_loco;
static uint8_t  s_extra_ai;
static uint32_t s_last_eval;

/* AI module: last CFG_STATUS and the apply state machine */
static volatile uint8_t  s_ai_st[8];
static volatile uint32_t s_ai_seen;
static uint8_t  s_ai_tries;
static uint32_t s_ai_last_send;
static uint8_t  s_ai_prev_gen;

/* ---------- file helpers ---------- */

static uint32_t Cfg_SlotCheck(const CfgSlotHdr *h)
{
  return ~(h->magic ^ h->seq ^ h->size ^ h->crc ^ (uint32_t)h->gen);
}

static void Cfg_Header(const uint8_t *b, HvacCfgHeader *h)
{
  memcpy(h, b, sizeof(*h));
}

static void Cfg_Module(const uint8_t *b, uint16_t i, HvacCfgModule *m)
{
  memcpy(m, &b[sizeof(HvacCfgHeader) + (uint32_t)i * sizeof(HvacCfgModule)], sizeof(*m));
}

static void Cfg_Channel(const uint8_t *b, uint16_t modules, uint16_t i, HvacCfgChannel *c)
{
  memcpy(c, &b[sizeof(HvacCfgHeader) + (uint32_t)modules * sizeof(HvacCfgModule) +
               (uint32_t)i * sizeof(HvacCfgChannel)], sizeof(*c));
}

static uint8_t Cfg_Validate(const uint8_t *b, uint32_t n)
{
  HvacCfgHeader h;

  if (n < sizeof(h) || n > HVAC_CFGF_MAX_SIZE) return PLC_CFG_E_SIZE;
  Cfg_Header(b, &h);
  if (h.magic != HVAC_CFGF_MAGIC || h.format != HVAC_CFGF_FORMAT) return PLC_CFG_E_FORMAT;
  if (h.module_count > HVAC_CFGF_MAX_MODULES || h.channel_count > HVAC_CFGF_MAX_CHANNELS) return PLC_CFG_E_FORMAT;
  if (n != sizeof(h) + (uint32_t)h.module_count * sizeof(HvacCfgModule) +
           (uint32_t)h.channel_count * sizeof(HvacCfgChannel)) return PLC_CFG_E_SIZE;

  for (uint16_t i = 0u; i < h.module_count; i++)
  {
    HvacCfgModule m;
    Cfg_Module(b, i, &m);
    if (m.channels > 8u) return PLC_CFG_E_FORMAT;
  }
  for (uint16_t i = 0u; i < h.channel_count; i++)
  {
    HvacCfgChannel c;
    HvacCfgModule m;
    Cfg_Channel(b, h.module_count, i, &c);
    if (c.module >= h.module_count) return PLC_CFG_E_FORMAT;
    Cfg_Module(b, c.module, &m);
    if (c.channel < 1u || c.channel > m.channels || c.signal > HVAC_SIG_NTC10K) return PLC_CFG_E_FORMAT;
  }
  return PLC_CFG_OK;
}

static void Cfg_ResetRuntime(void)
{
  memset(s_mod, 0, sizeof(s_mod));
  for (uint32_t i = 0u; i < HVAC_CFGF_MAX_MODULES; i++) s_mod[i].state = PLC_CFG_MOD_APPLYING;
  s_state = PLC_CFG_ST_APPLYING;
  s_applied_ms = 0u;
  s_ai_tries = 0u;
  s_ai_last_send = osKernelGetTickCount() - CFG_RESEND_MS;
  s_ai_prev_gen = 0u;
}

/* Reads one flash copy into s_act; returns 1 if it is complete and valid. */
static uint8_t Cfg_ReadSlot(uint8_t slot, CfgSlotHdr *h)
{
  uint32_t base = CFG_FLASH_BASE + (uint32_t)slot * CFG_SLOT_SIZE;

  if (!W25q_Read(base, h, sizeof(*h))) return 0u;
  if (h->magic != CFG_SLOT_MAGIC || h->check != Cfg_SlotCheck(h) || h->size > HVAC_CFGF_MAX_SIZE) return 0u;
  if (!W25q_Read(base + CFG_SLOT_HDR, s_act, h->size)) return 0u;
  if (fwupd_crc32(s_act, h->size) != h->crc) return 0u;
  return (Cfg_Validate(s_act, h->size) == PLC_CFG_OK) ? 1u : 0u;
}

static void Cfg_Load(void)
{
  CfgSlotHdr a, b;
  uint8_t va = Cfg_ReadSlot(0u, &a);
  uint8_t vb = Cfg_ReadSlot(1u, &b);
  int8_t use = -1;

  if (va && vb) use = (b.seq > a.seq) ? 1 : 0;
  else if (va) use = 0;
  else if (vb) use = 1;
  if (use < 0) return;

  /* s_act holds whatever copy 1 left there; read copy 0 again if it is the chosen one. */
  if (use == 0 && !Cfg_ReadSlot(0u, &a)) return;
  {
    const CfgSlotHdr *h = (use == 0) ? &a : &b;
    s_act_len = h->size;
    s_act_crc = h->crc;
    s_gen = h->gen;
    s_seq = h->seq;
    s_slot = use;
  }
  Cfg_ResetRuntime();
}

static uint8_t Cfg_Store(const uint8_t *blob, uint32_t len, uint32_t crc, uint8_t gen)
{
  uint8_t slot = (s_slot == 0) ? 1u : 0u;
  uint32_t base = CFG_FLASH_BASE + (uint32_t)slot * CFG_SLOT_SIZE;
  CfgSlotHdr h = {0};

  h.magic = CFG_SLOT_MAGIC;
  h.seq = s_seq + 1u;
  h.size = len;
  h.crc = crc;
  h.gen = gen;
  h.check = Cfg_SlotCheck(&h);

  /* Header last: an interrupted write leaves the copy invalid, the other copy stays in use. */
  if (!W25q_Erase(base, CFG_SLOT_SIZE)) return 0u;
  if (!W25q_Write(base + CFG_SLOT_HDR, blob, len)) return 0u;
  if (!W25q_Write(base, &h, sizeof(h))) return 0u;
  s_seq = h.seq;
  s_slot = (int8_t)slot;
  return 1u;
}

/* ---------- module check ---------- */

static void Cfg_SendAiChannel(uint16_t mod_idx, const HvacCfgHeader *h, uint8_t ch)
{
  uint8_t d[8] = {0};

  d[0] = HVAC_CFG_PLACE_ANY;
  d[1] = ch;
  d[2] = HVAC_SIG_OFF;
  d[3] = s_gen;
  for (uint16_t i = 0u; i < h->channel_count; i++)
  {
    HvacCfgChannel c;
    Cfg_Channel(s_act, h->module_count, i, &c);
    if (c.module == mod_idx && c.channel == ch)
    {
      d[2] = c.signal;
      hvac_cfg_put16(&d[4], c.min_x10);
      hvac_cfg_put16(&d[6], c.max_x10);
      break;
    }
  }
  (void)PlcCan_SendLoco(HVAC_CAN_ID_CFG_SET, 0u, d);
}

static void Cfg_CheckAi(uint16_t idx, const HvacCfgModule *m, const HvacCfgHeader *h, PlcCfgModState *st, uint32_t now)
{
  uint8_t want = (m->channels >= 8u) ? 0xFFu : (uint8_t)((1u << m->channels) - 1u);
  uint8_t d[8];
  uint32_t seen;

  taskENTER_CRITICAL();
  memcpy(d, (const void *)s_ai_st, 8u);
  seen = s_ai_seen;
  taskEXIT_CRITICAL();

  if (seen == 0u || (now - seen) >= CFG_STATUS_FRESH_MS || d[0] != FWUPD_TYPE_AI)
  {
    st->state = PLC_CFG_MOD_MISSING;
    st->version = 0u;
    s_ai_tries = 0u;
    return;
  }

  st->version = fwupd_get16(&d[2]);
  st->ok_mask = d[5] & want;
  st->bad_mask = d[6] & want;

  if (d[4] == s_gen && ((d[5] | d[6]) & want) == want)
  {
    s_ai_tries = 0u;
    s_ai_prev_gen = d[4];
    if (st->bad_mask != 0u)
    {
      st->state = PLC_CFG_MOD_ERROR;
      st->err = (d[7] != 0u) ? d[7] : HVAC_CFG_E_SIGNAL;
    }
    else
    {
      st->state = PLC_CFG_MOD_OK;
      st->err = PLC_CFG_ERR_NONE;
    }
    return;
  }

  /* Module answered with this generation before and lost it: it restarted. */
  if (s_ai_prev_gen == s_gen && d[4] != s_gen)
  {
    s_resends++;
    PlcFwupd_Log(PLC_FWUPD_EV_CFG_RESEND, 0u, HVAC_CAT_AI, 0u, 0u, d[4], s_gen);
  }
  s_ai_prev_gen = d[4];

  if (s_ai_tries >= CFG_CONFIRM_TRIES)
  {
    st->state = PLC_CFG_MOD_ERROR;
    st->err = PLC_CFG_ERR_NO_CONFIRM;
  }
  else
  {
    st->state = PLC_CFG_MOD_APPLYING;
  }

  if ((now - s_ai_last_send) >= CFG_RESEND_MS)
  {
    for (uint8_t ch = 1u; ch <= m->channels; ch++) Cfg_SendAiChannel(idx, h, ch);
    s_ai_last_send = now;
    if (s_ai_tries < 255u) s_ai_tries++;
  }
}

static void Cfg_Evaluate(uint32_t now)
{
  HvacCfgHeader h;
  uint8_t loco_nodes[PLC_FWUPD_MAX_NODES];
  uint8_t nloco = 0u, loco_k = 0u, ai_k = 0u;
  uint8_t any_err = 0u, any_apply = 0u;
  uint8_t err_type = 0u, err_code = 0u;
  uint8_t ai_fresh;
  uint8_t state;

  Cfg_Header(s_act, &h);

  for (uint8_t i = 0u; i < PLC_FWUPD_MAX_NODES; i++)
  {
    volatile PlcFwupdNode *n = &g_plc_fwupd.nodes[i];
    if (n->tag != 0u && n->link && n->module_type == FWUPD_TYPE_LOCOMOTIVE) loco_nodes[nloco++] = i;
  }

  for (uint16_t i = 0u; i < h.module_count; i++)
  {
    HvacCfgModule m;
    PlcCfgModState *st = &s_mod[i];

    Cfg_Module(s_act, i, &m);
    switch (m.type)
    {
      case HVAC_CAT_PLC:
        st->state = PLC_CFG_MOD_OK;
        st->version = PLC_FW_VERSION;
        st->err = PLC_CFG_ERR_NONE;
        break;

      case HVAC_CAT_LOCOMOTIVE:
        if (loco_k < nloco)
        {
          volatile PlcFwupdNode *n = &g_plc_fwupd.nodes[loco_nodes[loco_k++]];
          st->version = n->app_version;
          st->state = (n->state == FWUPD_ST_APP_RUNNING) ? PLC_CFG_MOD_OK : PLC_CFG_MOD_ERROR;
          st->err = (n->state == FWUPD_ST_APP_RUNNING) ? PLC_CFG_ERR_NONE : PLC_CFG_ERR_IN_BOOT;
        }
        else
        {
          st->state = PLC_CFG_MOD_MISSING;
          st->version = 0u;
          st->err = PLC_CFG_ERR_NONE;
        }
        break;

      case HVAC_CAT_AI:
        if (ai_k == 0u)
        {
          ai_k = 1u;
          Cfg_CheckAi(i, &m, &h, st, now);
        }
        else
        {
          st->state = PLC_CFG_MOD_UNCHECKED;
        }
        break;

      case HVAC_CAT_HUB_DISPLAYS:
        st->state = PlcCan_HubBusOk() ? PLC_CFG_MOD_ON_BUS : PLC_CFG_MOD_MISSING;
        break;

      default:
        st->state = PLC_CFG_MOD_UNCHECKED;
        break;
    }

    if (st->state == PLC_CFG_MOD_MISSING || st->state == PLC_CFG_MOD_ERROR)
    {
      if (!any_err) { err_type = m.type; err_code = st->err; }
      any_err = 1u;
    }
    if (st->state == PLC_CFG_MOD_APPLYING) any_apply = 1u;
  }

  ai_fresh = (s_ai_seen != 0u && (now - s_ai_seen) < CFG_STATUS_FRESH_MS) ? 1u : 0u;
  s_extra_loco = (nloco > loco_k) ? (uint8_t)(nloco - loco_k) : 0u;
  s_extra_ai = (ai_fresh && ai_k == 0u) ? 1u : 0u;
  if (s_extra_loco || s_extra_ai) any_err = 1u;

  state = any_err ? PLC_CFG_ST_ERRORS : (any_apply ? PLC_CFG_ST_APPLYING : PLC_CFG_ST_OK);
  if (state != s_state)
  {
    if (state == PLC_CFG_ST_OK)
    {
      s_applied_ms = now;
      PlcFwupd_Log(PLC_FWUPD_EV_CFG_OK, 0u, 0u, 0u, 0u, 0u, s_gen);
    }
    else if (state == PLC_CFG_ST_ERRORS)
    {
      PlcFwupd_Log(PLC_FWUPD_EV_CFG_ERROR, 0u, err_type, err_code, 0u, 0u, s_gen);
    }
    s_state = state;
  }
}

/* ---------- API ---------- */

void PlcCfg_Setup(void)
{
  s_mx = osMutexNew(NULL);
}

void PlcCfg_OnStatusIsr(const uint8_t d[8])
{
  memcpy((void *)s_ai_st, d, 8u);
  s_ai_seen = osKernelGetTickCount();
  if (s_ai_seen == 0u) s_ai_seen = 1u;
}

void PlcCfg_Poll(void)
{
  uint32_t now = osKernelGetTickCount();

  if (!s_loaded)
  {
    if (FwStore_FlashOk())
    {
      s_flash_ok = 1u;
      if (osMutexAcquire(s_mx, osWaitForever) == osOK)
      {
        Cfg_Load();
        osMutexRelease(s_mx);
      }
      s_loaded = 1u;
    }
    else if (now >= CFG_FLASH_WAIT_MS)
    {
      s_loaded = 1u;
    }
    return;
  }

  if ((now - s_last_eval) < CFG_EVAL_MS) return;
  s_last_eval = now;
  if (osMutexAcquire(s_mx, 20u) != osOK) return;
  if (s_act_len == 0u) s_state = PLC_CFG_ST_NONE;
  else Cfg_Evaluate(now);
  osMutexRelease(s_mx);
}

uint8_t PlcCfg_UploadBegin(uint32_t size, uint32_t crc)
{
  s_up_got = 0u;
  s_up_size = 0u;
  if (size < sizeof(HvacCfgHeader) || size > HVAC_CFGF_MAX_SIZE) return PLC_CFG_E_SIZE;
  s_up_size = size;
  s_up_crc = crc;
  return PLC_CFG_OK;
}

void PlcCfg_UploadWrite(const uint8_t *p, uint32_t n)
{
  if (s_up_got + n > s_up_size) n = s_up_size - s_up_got;
  memcpy(&s_up[s_up_got], p, n);
  s_up_got += n;
}

uint8_t PlcCfg_UploadEnd(uint8_t *gen_out)
{
  uint32_t crc;
  uint8_t err;
  uint8_t gen;

  if (s_up_size == 0u || s_up_got != s_up_size) err = PLC_CFG_E_SIZE;
  else if ((crc = fwupd_crc32(s_up, s_up_size)), (s_up_crc != 0u && crc != s_up_crc)) err = PLC_CFG_E_CRC;
  else err = Cfg_Validate(s_up, s_up_size);

  if (err == PLC_CFG_OK && !s_flash_ok && !(s_flash_ok = FwStore_FlashOk())) err = PLC_CFG_E_FLASH;
  if (err == PLC_CFG_OK && osMutexAcquire(s_mx, 2000u) != osOK) err = PLC_CFG_E_BUSY;
  if (err != PLC_CFG_OK)
  {
    PlcFwupd_Log(PLC_FWUPD_EV_CFG_SAVED, 0u, 0u, err, 0u, 0u, 0u);
    return err;
  }

  gen = (uint8_t)((s_gen % 255u) + 1u);
  if (!Cfg_Store(s_up, s_up_size, crc, gen))
  {
    osMutexRelease(s_mx);
    PlcFwupd_Log(PLC_FWUPD_EV_CFG_SAVED, 0u, 0u, PLC_CFG_E_FLASH, 0u, 0u, 0u);
    return PLC_CFG_E_FLASH;
  }
  memcpy(s_act, s_up, s_up_size);
  s_act_len = s_up_size;
  s_act_crc = crc;
  s_gen = gen;
  s_loaded = 1u;
  Cfg_ResetRuntime();
  osMutexRelease(s_mx);

  PlcFwupd_Log(PLC_FWUPD_EV_CFG_SAVED, 0u, 0u, 0u, 0u, 0u, gen);
  if (gen_out != NULL) *gen_out = gen;
  return PLC_CFG_OK;
}

void PlcCfg_GetSummary(PlcCfgSummary *out)
{
  memset(out, 0, sizeof(*out));
  out->flash_ok = s_flash_ok;
  if (osMutexAcquire(s_mx, 200u) != osOK) return;
  if (s_act_len > 0u)
  {
    HvacCfgHeader h;
    Cfg_Header(s_act, &h);
    out->present = 1u;
    out->gen = s_gen;
    out->size = s_act_len;
    out->crc = s_act_crc;
    out->module_count = h.module_count;
    out->channel_count = h.channel_count;
    for (uint32_t i = 0u; i < HVAC_CFGF_PROJECT_LEN && h.project[i] != '\0'; i++)
    {
      char c = h.project[i];
      out->project[i] = (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') ? c : '_';
    }
  }
  out->state = s_state;
  out->applied_ms = s_applied_ms;
  out->resends = s_resends;
  out->extra_loco = s_extra_loco;
  out->extra_ai = s_extra_ai;
  osMutexRelease(s_mx);
}

uint8_t PlcCfg_GetModule(uint16_t i, HvacCfgModule *m, PlcCfgModState *st)
{
  uint8_t ok = 0u;

  if (osMutexAcquire(s_mx, 200u) != osOK) return 0u;
  if (s_act_len > 0u)
  {
    HvacCfgHeader h;
    Cfg_Header(s_act, &h);
    if (i < h.module_count)
    {
      Cfg_Module(s_act, i, m);
      *st = s_mod[i];
      ok = 1u;
    }
  }
  osMutexRelease(s_mx);
  return ok;
}
