/*
 * $ Copyright 2016-YEAR Cypress Semiconductor $
 */

#ifndef PANU_SDP_H
#define PANU_SDP_H

#ifdef WICED_APP_PANU_INCLUDED

#include "wiced_bt_dev.h"
#include "wiced_bt_sdp.h"
#include "wiced_bt_trace.h"
#include "wiced_bt_cfg.h"
#include "wiced_timer.h"
#include "wiced_bt_utils.h"

/******************************************************
 *                     Constants
 ******************************************************/
////// TEMP for compiling
typedef struct
{
#define     PANU_STATE_IDLE       0
#define     PANU_STATE_OPENING    1
#define     PANU_STATE_OPEN       2
#define     PANU_STATE_CONNECT    3
#define     PANU_STATE_CLOSING    4
    uint8_t             state;                  /* state machine state */
    uint16_t            app_handle;             /* Handle used to identify with the app */
    uint16_t            remote_profile_uuid;
    uint16_t            version;
    BD_ADDR             remote_addr;
    wiced_bt_sdp_discovery_db_t *p_sdp_discovery_db;
    uint32_t            remote_pan_features;
    uint16_t            remote_pan_version;
} pan_session_cb_t;

#define WICED_BUFF_MAX_SIZE             360

#endif

#endif /* PANU_SDP_H */
