/*
 * Reserved fields for later records (who changed a value, command number,
 * request vs applied). Not sent on CAN or HTTP yet — do not change live frames.
 */
#ifndef HVAC_RECORD_H
#define HVAC_RECORD_H

#include <stdint.h>

#define HVAC_RECORD_USER_LEN 32u

#define HVAC_RECORD_PHASE_NONE     0u
#define HVAC_RECORD_PHASE_REQUEST  1u
#define HVAC_RECORD_PHASE_APPLIED  2u

typedef struct
{
  uint32_t cmd_id; /* 0 = unused */
  char user[HVAC_RECORD_USER_LEN]; /* empty = unknown */
  uint8_t phase;
} hvac_record_cmd_t;

#endif
