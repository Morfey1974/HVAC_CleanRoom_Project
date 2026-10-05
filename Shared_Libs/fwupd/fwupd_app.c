/*
 * fwupd_app.c — firmware update support inside a module application.
 */
#include "fwupd_app.h"

#define FWUPD_APP_CONFIRM_MS   5000u /* running this long = successful start, clears boot_fails */

extern FwupdShared _fwupd_shared;

static FwupdAppSendFn s_send;
static FwupdAnnounce s_ann;
static uint32_t s_tag;
static uint32_t s_last_announce;
static uint8_t s_confirmed;
static volatile uint8_t s_announce_req;
static volatile uint8_t s_reboot_req;

static void FwupdApp_SendAnnounce(void)
{
  uint8_t d[8];

  d[0] = s_ann.module_type;
  d[1] = s_ann.board_rev;
  d[2] = s_ann.state;
  d[3] = s_ann.boot_version;
  d[4] = (uint8_t)s_ann.app_version;
  d[5] = (uint8_t)(s_ann.app_version >> 8);
  d[6] = s_ann.boot_fails;
  d[7] = 0u;
  s_send(FWUPD_ID(FWUPD_MSG_ANNOUNCE, s_tag), d);
}

void FwupdApp_Init(FwupdAppSendFn send, uint8_t module_type, uint8_t board_rev, uint16_t version)
{
  const uint32_t uid[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };
  FwupdShared *sh = &_fwupd_shared;

  s_send = send;
  s_tag = fwupd_node_tag(uid);
  s_ann.module_type = module_type;
  s_ann.board_rev = board_rev;
  s_ann.state = FWUPD_ST_APP_RUNNING;
  s_ann.boot_version = 0u;
  s_ann.app_version = version;
  s_ann.boot_fails = (sh->magic == FWUPD_SHARED_MAGIC && sh->check == fwupd_shared_check(sh))
                     ? (uint8_t)sh->boot_fails : 0u;
  s_last_announce = HAL_GetTick() - FWUPD_ANNOUNCE_APP_MS;
}

void FwupdApp_OnRx(uint32_t id, uint8_t is_ext, const uint8_t *data, uint32_t len)
{
  uint32_t node;

  if (!is_ext || s_send == 0) return;
  node = FWUPD_ID_NODE(id);
  if (node != s_tag && node != FWUPD_NODE_ALL) return;

  switch (FWUPD_ID_TYPE(id))
  {
    case FWUPD_MSG_DISCOVER:
      s_announce_req = 1u;
      break;
    case FWUPD_MSG_CONNECT:
      if (node == s_tag && len >= 8u && fwupd_get32(&data[0]) == FWUPD_CONNECT_MAGIC &&
          fwupd_get32(&data[4]) == FWUPD_CONNECT_KEY(s_tag))
      {
        s_reboot_req = 1u;
      }
      break;
    default:
      break;
  }
}

void FwupdApp_Poll(void)
{
  uint32_t now;

  if (s_send == 0) return;
  now = HAL_GetTick();

  if (!s_confirmed && now >= FWUPD_APP_CONFIRM_MS)
  {
    FwupdShared *sh = &_fwupd_shared;
    sh->magic = FWUPD_SHARED_MAGIC;
    sh->request = FWUPD_REQ_NONE;
    sh->boot_fails = 0u;
    sh->check = fwupd_shared_check(sh);
    s_ann.boot_fails = 0u;
    s_confirmed = 1u;
  }

  if (s_reboot_req)
  {
    FwupdShared *sh = &_fwupd_shared;
    sh->magic = FWUPD_SHARED_MAGIC;
    sh->request = FWUPD_REQ_STAY_IN_BOOT;
    sh->boot_fails = 0u;
    sh->check = fwupd_shared_check(sh);
    HAL_Delay(5);
    NVIC_SystemReset();
  }

  if (s_announce_req || (now - s_last_announce) >= FWUPD_ANNOUNCE_APP_MS)
  {
    s_announce_req = 0u;
    s_last_announce = now;
    FwupdApp_SendAnnounce();
  }
}
