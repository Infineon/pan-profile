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

#ifndef  PAN_INT_H
#define  PAN_INT_H

#if defined(WICED_APP_PANU_INCLUDED) || defined(WICED_APP_PANNAP_INCLUDED)

#include "pan_api.h"
/******************************************************************************
**
** PAN
**
******************************************************************************/

/* Maximum number of PAN connections allowed */
#ifndef MAX_PAN_CONNS
#define MAX_PAN_CONNS                    1
#endif

/*
** This role is used to shutdown the profile. Used internally
** Applications should call wiced_bt_pan_deregister to shutdown the profile
*/
#define PAN_ROLE_INACTIVE      0

/* Protocols supported by the host internal stack, are registered with SDP */
#define PAN_PROTOCOL_IP        0x0800
#define PAN_PROTOCOL_ARP       0x0806

#define PAN_PROFILE_VERSION    0x0100   /* Version 1.00 */

/* Define the PAN Connection Control Block
*/
typedef struct
{
#define PAN_STATE_IDLE              0
#define PAN_STATE_CONN_START        1
#define PAN_STATE_CONNECTED         2
    uint8_t             con_state;

#define PAN_FLAGS_CONN_COMPLETED    0x01
    uint8_t             con_flags;

    uint16_t            handle;
    BD_ADDR           rem_bda;

    uint16_t            bad_pkts_rcvd;
    uint16_t            src_uuid;
    uint16_t            dst_uuid;
    uint16_t            prv_src_uuid;
    uint16_t            prv_dst_uuid;
    uint16_t            ip_addr_known;
    uint32_t            ip_addr;

} tPAN_CONN;


/*  The main PAN control block
*/
typedef struct
{
    uint8_t                       role;
    uint8_t                       active_role;
    uint8_t                       prv_active_role;
    tPAN_CONN                   pcb[MAX_PAN_CONNS];

    tPAN_CONN_STATE_CB          *pan_conn_state_cb;     /* Connection state callback */
    tPAN_BRIDGE_REQ_CB          *pan_bridge_req_cb;
    tPAN_DATA_IND_CB            *pan_data_ind_cb;
    tPAN_DATA_BUF_IND_CB        *pan_data_buf_ind_cb;
    tPAN_FILTER_IND_CB          *pan_pfilt_ind_cb;      /* protocol filter indication callback */
    tPAN_MFILTER_IND_CB         *pan_mfilt_ind_cb;      /* multicast filter indication callback */
    tPAN_TX_DATA_FLOW_CB        *pan_tx_data_flow_cb;

    BD_ADDR                     my_bda;                 /* BD Address of this device    */
    char                        *user_service_name;
    char                        *gn_service_name;
    char                        *nap_service_name;
    uint32_t                      pan_user_sdp_handle;
    uint32_t                      pan_gn_sdp_handle;
    uint32_t                      pan_nap_sdp_handle;
    uint8_t                       num_conns;
    uint8_t                       trace_level;
} tPAN_CB;


#ifdef __cplusplus
extern "C" {
#endif

/* Global PAN data
*/
#if PAN_DYNAMIC_MEMORY == FALSE
PAN_API extern tPAN_CB  pan_cb;
#else
PAN_API extern tPAN_CB  *pan_cb_ptr;
#define pan_cb (*pan_cb_ptr)
#endif

/*******************************************************************************/
extern void pan_register_with_bnep (void);
extern void pan_conn_ind_cb (uint16_t handle,
                             BD_ADDR p_bda,
                             tBT_UUID *remote_uuid,
                             tBT_UUID *local_uuid,
                             BOOLEAN is_role_change);
extern void pan_connect_state_cb (uint16_t handle, BD_ADDR rem_bda, tBNEP_RESULT result, BOOLEAN is_role_change);
extern void pan_data_ind_cb (uint16_t handle,
                             uint8_t *src,
                             uint8_t *dst,
                             uint16_t protocol,
                             uint8_t *p_data,
                             uint16_t len,
                             BOOLEAN fw_ext_present);
extern void pan_data_buf_ind_cb (uint16_t handle,
                                 uint8_t *src,
                                 uint8_t *dst,
                                 uint16_t protocol,
                                 uint8_t *data_buf,
                                 uint16_t buf_len,
                                 BOOLEAN ext);
extern void pan_tx_data_flow_cb (uint16_t handle,
                            tBNEP_RESULT  event);
void pan_proto_filt_ind_cb (uint16_t handle,
                            BOOLEAN indication,
                            tBNEP_RESULT result,
                            uint16_t num_filters,
                            uint8_t *p_filters);
void pan_mcast_filt_ind_cb (uint16_t handle,
                            BOOLEAN indication,
                            tBNEP_RESULT result,
                            uint16_t num_filters,
                            uint8_t *p_filters);
extern uint32_t pan_register_with_sdp (uint16_t uuid, uint8_t sec_mask, char *p_name, char *p_desc);
extern tPAN_CONN *pan_allocate_pcb (BD_ADDR p_bda, uint16_t handle);
extern tPAN_CONN *pan_get_pcb_by_handle (uint16_t handle);
extern tPAN_CONN *pan_get_pcb_by_addr (BD_ADDR p_bda);
extern void pan_close_all_connections (void);
extern void pan_release_pcb (tPAN_CONN *p_pcb);

/********************************************************************************/

#ifdef __cplusplus
}
#endif

#endif

#endif
