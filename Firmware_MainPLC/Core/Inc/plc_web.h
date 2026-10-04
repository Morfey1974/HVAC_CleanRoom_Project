/*
 * plc_web.h — Main PLC HTTP server (port 80, lwIP netconn).
 *
 *   GET /           page with temperature / humidity, refreshed every second
 *   GET /api/ai     JSON snapshot of the CAN gateway (PlcAiSnapshot)
 */
#ifndef PLC_WEB_H
#define PLC_WEB_H

/* Body of netTask after MX_LWIP_Init(), never returns. */
void PlcWeb_Task(void);

#endif /* PLC_WEB_H */
