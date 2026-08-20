/*
 * Copyright 2016-2022, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
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
