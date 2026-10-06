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
#include "plc_id.h"

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
#define CFG_STATUS_SLOTS      16u

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
static uint32_t s_last_chain;
static uint16_t s_walks_seen;

/* Found modules that are not in the configuration, and modules without ID outside the chain. */
static PlcIdFound s_extra[PLC_CFG_MAX_EXTRA];
static uint8_t  s_extra_n;
static uint8_t  s_chain[8];

/* Last CFG_STATUS of every rail module with an ID, keyed by rail and place. */
typedef struct
{
  uint8_t  rail;
  uint8_t  place;
  uint8_t  d[8];
  uint32_t seen;
} CfgStatus;

static volatile CfgStatus s_status[CFG_STATUS_SLOTS];

/* Apply state machine of every configured module (index = module table index). */
static uint8_t  s_tries[HVAC_CFGF_MAX_MODULES];
static uint8_t  s_prev_gen[HVAC_CFGF_MAX_MODULES];
static uint32_t s_last_send[HVAC_CFGF_MAX_MODULES];

static PlcIdFound s_found[PLC_ID_MAX_FOUND];

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
  memset(s_tries, 0, sizeof(s_tries));
  memset(s_prev_gen, 0, sizeof(s_prev_gen));
  for (uint32_t i = 0u; i < HVAC_CFGF_MAX_MODULES; i++) s_last_send[i] = osKernelGetTickCount() - CFG_RESEND_MS;
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

static uint8_t Cfg_FwupdType(uint8_t cat)
{
  switch (cat)
  {
    case HVAC_CAT_LOCOMOTIVE:   return FWUPD_TYPE_LOCOMOTIVE;
    case HVAC_CAT_HUB_DISPLAYS: return FWUPD_TYPE_HUB_DISPLAYS;
    case HVAC_CAT_TFT43:        return FWUPD_TYPE_DISPLAY_TFT43;
    case HVAC_CAT_AI:           return FWUPD_TYPE_AI;
    default:                    return 0u;
  }
}

/* Types that take part in the identification walk. */
static uint8_t Cfg_HasId(uint8_t cat)
{
  return (cat >= HVAC_CAT_LOCOMOTIVE && cat <= HVAC_CAT_TFT43) ? 1u : 0u;
}

static volatile PlcFwupdNode *Cfg_NodeByTag(uint32_t tag)
{
  for (uint32_t i = 0u; i < PLC_FWUPD_MAX_NODES; i++)
  {
    if (g_plc_fwupd.nodes[i].tag == tag && tag != 0u) return &g_plc_fwupd.nodes[i];
  }
  return NULL;
}

/* A module of this type that stays in its bootloader does not answer the walk. */
static uint8_t Cfg_TypeInBoot(uint8_t cat)
{
  uint8_t t = Cfg_FwupdType(cat);

  for (uint32_t i = 0u; i < PLC_FWUPD_MAX_NODES && t != 0u; i++)
  {
    volatile PlcFwupdNode *n = &g_plc_fwupd.nodes[i];
    if (n->tag != 0u && n->link && n->module_type == t && n->state != FWUPD_ST_APP_RUNNING) return 1u;
  }
  return 0u;
}

/* k-th running display that has no ID (old boards without the ID input), NULL if none. */
static volatile PlcFwupdNode *Cfg_TftWithoutId(uint8_t k, uint8_t nfound)
{
  for (uint32_t i = 0u; i < PLC_FWUPD_MAX_NODES; i++)
  {
    volatile PlcFwupdNode *n = &g_plc_fwupd.nodes[i];
    uint8_t has_id = 0u;

    if (n->tag == 0u || !n->link || n->module_type != FWUPD_TYPE_DISPLAY_TFT43 ||
        n->state != FWUPD_ST_APP_RUNNING) continue;
    for (uint8_t j = 0u; j < nfound; j++)
    {
      if (s_found[j].tag == n->tag) { has_id = 1u; break; }
    }
    if (has_id) continue;
    if (k == 0u) return n;
    k--;
  }
  return NULL;
}

static uint8_t Cfg_GetStatus(uint8_t rail, uint8_t place, uint32_t now, uint8_t d[8])
{
  uint8_t ok = 0u;

  taskENTER_CRITICAL();
  for (uint32_t i = 0u; i < CFG_STATUS_SLOTS; i++)
  {
    if (s_status[i].seen != 0u && s_status[i].rail == rail && s_status[i].place == place &&
        (now - s_status[i].seen) < CFG_STATUS_FRESH_MS)
    {
      memcpy(d, (const void *)s_status[i].d, 8u);
      ok = 1u;
      break;
    }
  }
  taskEXIT_CRITICAL();
  return ok;
}

static void Cfg_SendAiChannel(uint16_t mod_idx, const HvacCfgModule *m, const HvacCfgHeader *h, uint8_t ch)
{
  uint8_t d[8] = {0};
  uint8_t sig = HVAC_SIG_OFF;

  d[0] = m->place;
  d[1] = m->rail;
  d[3] = s_gen;
  for (uint16_t i = 0u; i < h->channel_count; i++)
  {
    HvacCfgChannel c;
    Cfg_Channel(s_act, h->module_count, i, &c);
    if (c.module == mod_idx && c.channel == ch)
    {
      sig = c.signal;
      hvac_cfg_put16(&d[4], c.min_x10);
      hvac_cfg_put16(&d[6], c.max_x10);
      break;
    }
  }
  d[2] = (uint8_t)((ch << 4) | (sig & 0x0Fu));
  (void)PlcCan_Send(PLC_CAN_BUS_LOCO, HVAC_CAN_ID_CFG_SET, 0u, d);
}

/* AI found at its place: channel settings until the module confirms this generation. */
static void Cfg_CheckAi(uint16_t idx, const HvacCfgModule *m, const HvacCfgHeader *h, PlcCfgModState *st, uint32_t now)
{
  uint8_t want = (m->channels >= 8u) ? 0xFFu : (uint8_t)((1u << m->channels) - 1u);
  uint8_t d[8];

  if (Cfg_GetStatus(m->rail, m->place, now, d) && d[0] == FWUPD_TYPE_AI)
  {
    st->ok_mask = d[4] & want;
    st->bad_mask = d[5] & want;

    if (d[3] == s_gen && ((d[4] | d[5]) & want) == want)
    {
      s_tries[idx] = 0u;
      s_prev_gen[idx] = d[3];
      st->state = (st->bad_mask != 0u) ? PLC_CFG_MOD_ERROR : PLC_CFG_MOD_OK;
      st->err = (st->bad_mask != 0u) ? ((d[6] != 0u) ? d[6] : HVAC_CFG_E_SIGNAL) : PLC_CFG_ERR_NONE;
      return;
    }

    /* Module answered with this generation before and lost it: it restarted. */
    if (s_prev_gen[idx] == s_gen && d[3] != s_gen)
    {
      s_resends++;
      PlcFwupd_Log(PLC_FWUPD_EV_CFG_RESEND, st->tag, HVAC_CAT_AI, 0u, 0u, d[3], s_gen);
    }
    s_prev_gen[idx] = d[3];
  }

  if (s_tries[idx] >= CFG_CONFIRM_TRIES)
  {
    st->state = PLC_CFG_MOD_ERROR;
    st->err = PLC_CFG_ERR_NO_CONFIRM;
  }
  else
  {
    st->state = PLC_CFG_MOD_APPLYING;
  }

  if ((now - s_last_send[idx]) >= CFG_RESEND_MS)
  {
    for (uint8_t ch = 1u; ch <= m->channels; ch++) Cfg_SendAiChannel(idx, m, h, ch);
    s_last_send[idx] = now;
    if (s_tries[idx] < 255u) s_tries[idx]++;
  }
}

static void Cfg_Problem(uint8_t *count, uint8_t p, uint8_t line, uint8_t rail, uint8_t place, uint8_t want, uint8_t found)
{
  if (*count == 0u)
  {
    s_chain[2] = p;
    s_chain[3] = line;
    s_chain[4] = rail;
    s_chain[5] = place;
    s_chain[6] = want;
    s_chain[7] = found;
  }
  if (*count < 255u) (*count)++;
}

static void Cfg_Evaluate(uint32_t now)
{
  HvacCfgHeader h;
  PlcIdSummary ids;
  uint8_t used[PLC_ID_MAX_FOUND];
  uint8_t nfound;
  uint8_t any_apply = 0u;
  uint8_t problems = 0u;
  uint8_t tft_k = 0u;
  volatile PlcFwupdNode *tft;
  uint8_t err_type = 0u, err_code = 0u;
  uint8_t state;

  Cfg_Header(s_act, &h);
  PlcId_GetSummary(&ids);
  nfound = PlcId_GetFound(s_found, PLC_ID_MAX_FOUND);
  memset(used, 0, sizeof(used));
  memset(s_chain, 0, sizeof(s_chain));

  /* New walk: modules have new IDs, confirmation counters start again. */
  if (ids.walks != s_walks_seen)
  {
    s_walks_seen = ids.walks;
    memset(s_tries, 0, sizeof(s_tries));
  }

  for (uint16_t i = 0u; i < h.module_count; i++)
  {
    HvacCfgModule m;
    PlcCfgModState *st = &s_mod[i];
    int f = -1;

    Cfg_Module(s_act, i, &m);
    st->found_cat = 0u;
    st->tag = 0u;

    if (m.type == HVAC_CAT_PLC)
    {
      st->state = PLC_CFG_MOD_OK;
      st->version = PLC_FW_VERSION;
      st->err = PLC_CFG_ERR_NONE;
      continue;
    }
    if (!Cfg_HasId(m.type))
    {
      st->state = PLC_CFG_MOD_UNCHECKED;
      continue;
    }
    if (ids.busy || ids.walks == 0u)
    {
      st->state = PLC_CFG_MOD_APPLYING;
      any_apply = 1u;
      continue;
    }

    for (uint8_t k = 0u; k < nfound; k++)
    {
      if (s_found[k].line == m.line && s_found[k].rail == m.rail && s_found[k].place == m.place) { f = k; break; }
    }

    if (f < 0 && m.type == HVAC_CAT_TFT43 && (tft = Cfg_TftWithoutId(tft_k, nfound)) != NULL)
    {
      tft_k++;
      st->state = PLC_CFG_MOD_ON_BUS; /* answers on the bus, place not checked */
      st->tag = tft->tag;
      st->found_cat = m.type;
      st->version = tft->app_version;
      st->err = PLC_CFG_ERR_NONE;
    }
    else if (f < 0)
    {
      st->state = PLC_CFG_MOD_MISSING;
      st->version = 0u;
      st->err = Cfg_TypeInBoot(m.type) ? PLC_CFG_ERR_IN_BOOT : PLC_CFG_ERR_NONE;
      Cfg_Problem(&problems, HVAC_CHAIN_P_MISSING, m.line, m.rail, m.place, m.type, 0u);
    }
    else
    {
      volatile PlcFwupdNode *n = Cfg_NodeByTag(s_found[f].tag);

      used[f] = 1u;
      st->tag = s_found[f].tag;
      st->found_cat = s_found[f].cat;
      st->version = (n != NULL) ? n->app_version : 0u;
      if (s_found[f].cat != m.type)
      {
        st->state = PLC_CFG_MOD_WRONG_TYPE;
        st->err = PLC_CFG_ERR_NONE;
        Cfg_Problem(&problems, HVAC_CHAIN_P_WRONG_TYPE, m.line, m.rail, m.place, m.type, s_found[f].cat);
      }
      else if (m.type == HVAC_CAT_AI)
      {
        Cfg_CheckAi(i, &m, &h, st, now);
        if (st->state == PLC_CFG_MOD_ERROR)
        {
          Cfg_Problem(&problems, HVAC_CHAIN_P_CONFIG, m.line, m.rail, m.place, m.type, m.type);
        }
      }
      else if (Cfg_FwupdType(m.type) != 0u)
      {
        st->state = PLC_CFG_MOD_OK;
        st->err = PLC_CFG_ERR_NONE;
      }
      else
      {
        st->state = PLC_CFG_MOD_ON_BUS; /* found at its place, settings not supported yet */
        st->err = PLC_CFG_ERR_NONE;
      }
    }

    if (st->state == PLC_CFG_MOD_APPLYING) any_apply = 1u;
    if ((st->state == PLC_CFG_MOD_MISSING || st->state == PLC_CFG_MOD_ERROR ||
         st->state == PLC_CFG_MOD_WRONG_TYPE) && err_type == 0u)
    {
      err_type = m.type;
      err_code = st->err;
    }
  }

  /* Found but not configured, then modules without ID outside the chain. */
  s_extra_n = 0u;
  s_extra_loco = 0u;
  s_extra_ai = 0u;
  if (!ids.busy && ids.walks != 0u)
  {
    PlcIdFound stray[PLC_ID_MAX_STRAY];
    uint8_t nstray = PlcId_GetStray(stray, PLC_ID_MAX_STRAY);

    for (uint8_t k = 0u; k < nfound; k++)
    {
      if (used[k]) continue;
      if (s_found[k].cat == HVAC_CAT_LOCOMOTIVE) s_extra_loco++;
      if (s_found[k].cat == HVAC_CAT_AI) s_extra_ai++;
      if (s_extra_n < PLC_CFG_MAX_EXTRA) s_extra[s_extra_n++] = s_found[k];
      Cfg_Problem(&problems, HVAC_CHAIN_P_EXTRA, s_found[k].line, s_found[k].rail, s_found[k].place, 0u, s_found[k].cat);
    }
    for (uint8_t k = 0u; k < nstray; k++)
    {
      if (s_extra_n < PLC_CFG_MAX_EXTRA) s_extra[s_extra_n++] = stray[k];
      Cfg_Problem(&problems, HVAC_CHAIN_P_EXTRA, stray[k].line, HVAC_ID_NONE, HVAC_ID_NONE, 0u, stray[k].cat);
    }
  }

  state = problems ? PLC_CFG_ST_ERRORS : (any_apply ? PLC_CFG_ST_APPLYING : PLC_CFG_ST_OK);
  s_chain[0] = problems ? HVAC_CHAIN_ST_ERROR : (any_apply ? HVAC_CHAIN_ST_BUSY : HVAC_CHAIN_ST_OK);
  s_chain[1] = problems;
  if (state != s_state)
  {
    if (state == PLC_CFG_ST_OK)
    {
      s_applied_ms = now;
      PlcFwupd_Log(PLC_FWUPD_EV_CFG_OK, 0u, 0u, 0u, 0u, 0u, s_gen);
    }
    else if (state == PLC_CFG_ST_ERRORS)
    {
      PlcFwupd_Log(PLC_FWUPD_EV_CFG_ERROR, 0u, err_type ? err_type : s_chain[7], err_type ? err_code : 0u,
                   s_chain[2], 0u, s_gen);
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
  uint32_t now = osKernelGetTickCount();
  int slot = -1;
  int oldest = 0;

  if (d[1] == HVAC_CFG_PLACE_ANY || d[2] == HVAC_CFG_PLACE_ANY) return; /* no ID yet */
  if (now == 0u) now = 1u;
  for (int i = 0; i < (int)CFG_STATUS_SLOTS; i++)
  {
    if (s_status[i].seen != 0u && s_status[i].rail == d[2] && s_status[i].place == d[1]) { slot = i; break; }
    if (slot < 0 && s_status[i].seen == 0u) slot = i;
    if (s_status[i].seen < s_status[oldest].seen) oldest = i;
  }
  if (slot < 0) slot = oldest;
  s_status[slot].rail = d[2];
  s_status[slot].place = d[1];
  memcpy((void *)s_status[slot].d, d, 8u);
  s_status[slot].seen = now;
}

uint8_t PlcCfg_Ready(void)
{
  return s_loaded;
}

/* Rails of the heads (locomotive, HUB) of one line in the configuration, ascending; k-th or 0. */
uint8_t PlcCfg_HeadRail(uint8_t line, uint8_t k)
{
  uint8_t rails[HVAC_CFGF_MAX_MODULES];
  uint8_t n = 0u;
  uint8_t r = 0u;

  if (osMutexAcquire(s_mx, 200u) != osOK) return 0u;
  if (s_act_len > 0u)
  {
    HvacCfgHeader h;
    Cfg_Header(s_act, &h);
    for (uint16_t i = 0u; i < h.module_count; i++)
    {
      HvacCfgModule m;
      Cfg_Module(s_act, i, &m);
      if (m.line == line && m.place == 0u && m.rail != 0u &&
          (m.type == HVAC_CAT_LOCOMOTIVE || m.type == HVAC_CAT_HUB_DISPLAYS))
      {
        uint8_t j = n++;
        while (j > 0u && rails[j - 1u] > m.rail) { rails[j] = rails[j - 1u]; j--; }
        rails[j] = m.rail;
      }
    }
  }
  osMutexRelease(s_mx);
  if (k < n) r = rails[k];
  return r;
}

uint8_t PlcCfg_MaxRail(void)
{
  uint8_t r = 0u;

  if (osMutexAcquire(s_mx, 200u) != osOK) return 0u;
  if (s_act_len > 0u)
  {
    HvacCfgHeader h;
    Cfg_Header(s_act, &h);
    for (uint16_t i = 0u; i < h.module_count; i++)
    {
      HvacCfgModule m;
      Cfg_Module(s_act, i, &m);
      if (m.rail > r && m.rail < HVAC_ID_NONE) r = m.rail;
    }
  }
  osMutexRelease(s_mx);
  return r;
}

void PlcCfg_Poll(void)
{
  uint32_t now = osKernelGetTickCount();

  /* Chain summary for the displays (also without a configuration: state NONE). */
  if (s_loaded && (now - s_last_chain) >= HVAC_ID_CHAIN_MS)
  {
    uint8_t d[8];

    s_last_chain = now;
    taskENTER_CRITICAL();
    memcpy(d, s_chain, 8u);
    taskEXIT_CRITICAL();
    if (s_act_len == 0u) memset(d, 0, sizeof(d));
    (void)PlcCan_Send(PLC_CAN_BUS_HUB, HVAC_CAN_ID_CHAIN, 0u, d);
  }

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
  PlcId_RequestWalk(); /* rail numbers may have changed */
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
  out->extra_count = s_extra_n;
  out->problems = s_chain[1];
  osMutexRelease(s_mx);
}

uint8_t PlcCfg_GetExtra(uint8_t i, PlcIdFound *out)
{
  uint8_t ok = 0u;

  if (osMutexAcquire(s_mx, 200u) != osOK) return 0u;
  if (i < s_extra_n)
  {
    *out = s_extra[i];
    ok = 1u;
  }
  osMutexRelease(s_mx);
  return ok;
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
