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

#ifdef WICED_APP_PANU_INCLUDED

#include "wiced_bt_dev.h"
#include "wiced_bt_sdp.h"
#include "wiced_bt_cfg.h"
#include "hci_control_api.h"
#include "wiced_memory.h"
#include "wiced_transport.h"
#include "panu_sdp.h"
#include "pan_api.h"
#include "hci_control_panu.h"

/******************************************************
*                 Global Variables
******************************************************/
pan_session_cb_t sdp_panu_scb;

/******************************************************
*               Function Declarations
******************************************************/
static void panu_sdp_free_db(pan_session_cb_t *p_scb);
static BOOLEAN panu_sdp_find_attr(pan_session_cb_t *p_scb);

void wiced_bt_panu_sdp_init(void)
{
    pan_session_cb_t *p_scb = &sdp_panu_scb;

    WICED_BT_TRACE( "wiced_bt_panu_sdp_init\n");

    memset(&sdp_panu_scb, 0, sizeof(pan_session_cb_t));
    p_scb->app_handle = 1;
}

/*
 * SDP callback function.
 */
void panu_do_open(pan_session_cb_t *p_scb)
{
    tPAN_RESULT result = WICED_SUCCESS;
    panu_open_t    open;

    p_scb->state = PANU_STATE_OPEN;
    result = wiced_bt_pan_connect (p_scb->remote_addr, PAN_ROLE_CLIENT, PAN_ROLE_NAP_SERVER, &p_scb->app_handle);
    WICED_BT_TRACE("panu_do_open result:0x%x \n", result);

    //szopen.status = result;
    utl_bdcpy( open.bd_addr, p_scb->remote_addr );
    panu_hci_send_panu_event( HCI_CONTROL_PANU_EVENT_OPEN, p_scb->app_handle, (panu_event_t *)&open );
}

/*
 * Send open callback event to application.
 */
void panu_process_open_callback(pan_session_cb_t *p_scb, uint8_t status)
{
    panu_open_t open;

    WICED_BT_TRACE("panu_process_open_callback status=%d\n", status);

    if (status == HCI_CONTROL_PANU_STATUS_SUCCESS)
    {
        utl_bdcpy(open.bd_addr, p_scb->remote_addr);
        panu_hci_send_panu_event(HCI_CONTROL_PANU_EVENT_OPEN, p_scb->app_handle, ( panu_event_t * ) &open);
    }
    else
    {
        p_scb->state = PANU_STATE_IDLE;
        utl_bdcpy(open.bd_addr, p_scb->remote_addr);
        panu_hci_send_panu_event(HCI_CONTROL_PANU_EVENT_SERVICE_NOT_FOUND, p_scb->app_handle, ( panu_event_t * ) &open);
    }
}

static void panu_sdp_cback(uint16_t sdp_status)
{
    uint16_t                event;
    pan_session_cb_t *p_scb = &sdp_panu_scb;

    WICED_BT_TRACE("panu_sdp_cback status:0x%x, p_scb %x\n", sdp_status, p_scb );

    if ( ( sdp_status == WICED_BT_SDP_SUCCESS ) || ( sdp_status == WICED_BT_SDP_DB_FULL ) )
    {
        if ( panu_sdp_find_attr( p_scb ) )
        {
            panu_do_open( p_scb );
        }
        else
        {
            panu_process_open_callback( p_scb, HCI_CONTROL_PANU_STATUS_FAIL_SDP );
        }
    }
    else
    {
        panu_process_open_callback(p_scb, HCI_CONTROL_PANU_STATUS_FAIL_SDP);
    }
    panu_sdp_free_db( p_scb );
}

/*
 * Process SDP discovery results to find requested attributes for requested service.
 * Returns TRUE if results found, FALSE otherwise.
 */
BOOLEAN panu_sdp_find_attr(pan_session_cb_t *p_scb)
{
    wiced_bt_sdp_discovery_record_t     *p_rec = ( wiced_bt_sdp_discovery_record_t * ) NULL;
    wiced_bt_sdp_protocol_elem_t        pe;
    wiced_bt_sdp_discovery_attribute_t  *p_attr;
    BOOLEAN                             result = WICED_TRUE;
    wiced_bt_uuid_t                     uuid_list;

    WICED_BT_TRACE("Looking for NAP service\n");
    uuid_list.len       = LEN_UUID_16;
    uuid_list.uu.uuid16 = p_scb->remote_profile_uuid;

    p_rec = wiced_bt_sdp_find_service_uuid_in_db( p_scb->p_sdp_discovery_db, &uuid_list, p_rec );
    if ( p_rec == NULL )
    {
        WICED_BT_TRACE("panu_sdp_find_attr( ) - could not find NAP service\n");
        return ( WICED_FALSE );
    }

    if ( wiced_bt_sdp_find_protocol_list_elem_in_rec( p_rec, UUID_PROTOCOL_BNEP, &pe ) )
    {
        WICED_BT_TRACE("panu_sdp_find_attr - num of proto elements = 0x%x\n",  pe.num_params);
        if ( pe.num_params > 0 )
        {
            p_scb->version = pe.params[0];
            WICED_BT_TRACE("panu_sdp_find_attr - found version in SDP record. version =0x%x\n", p_scb->version);
        }
        else
            result = WICED_FALSE;
    }
    else
    {
        result = WICED_FALSE;
    }

    if ( wiced_bt_sdp_find_profile_version_in_rec( p_rec, UUID_SERVCLASS_NAP, &p_scb->remote_pan_version ) )
    {
        WICED_BT_TRACE("p_scb->remote_pan_version: 0x%x\n", p_scb->remote_pan_version);
    }

    return result;
}

/*
 * Do service discovery.
 */
void panu_sdp_start_discovery(pan_session_cb_t *p_scb)
{
    uint16_t        attr_list[4];
    uint8_t         num_attr;
    wiced_bt_uuid_t uuid_list;
    wiced_bool_t result = WICED_FALSE;

    attr_list[0] = ATTR_ID_SERVICE_CLASS_ID_LIST;
    attr_list[1] = ATTR_ID_PROTOCOL_DESC_LIST;
    attr_list[2] = ATTR_ID_BT_PROFILE_DESC_LIST;
    num_attr = 3;

    /* allocate buffer for sdp database */
    p_scb->p_sdp_discovery_db = ( wiced_bt_sdp_discovery_db_t * ) wiced_bt_get_buffer( WICED_BUFF_MAX_SIZE );

    uuid_list.len       = LEN_UUID_16;
    uuid_list.uu.uuid16 = p_scb->remote_profile_uuid;
    /* set up service discovery database; attr happens to be attr_list len */
    result = wiced_bt_sdp_init_discovery_db(p_scb->p_sdp_discovery_db, WICED_BUFF_MAX_SIZE, 1, &uuid_list, num_attr, attr_list);
    if (result == WICED_FALSE)
    {
        WICED_BT_TRACE("panu_sdp_start_discovery: wiced_bt_sdp_init_discovery_db fail\n");
        panu_sdp_free_db(p_scb);
        panu_process_open_callback(p_scb, HCI_CONTROL_PANU_STATUS_FAIL_SDP);
        return;
    }

    WICED_BT_TRACE("initiate service discovery app_handle = %x\n",p_scb->app_handle);

    /* initiate service discovery */
    if ( !wiced_bt_sdp_service_search_attribute_request(p_scb->remote_addr, p_scb->p_sdp_discovery_db, panu_sdp_cback))
    {
        WICED_BT_TRACE("panu_sdp_start_discovery: wiced_bt_sdp_service_search_attribute_request fail\n");
        /* Service discovery not initiated - free discover db, reopen server, tell app  */
        panu_sdp_free_db(p_scb);

        panu_process_open_callback(p_scb, HCI_CONTROL_PANU_STATUS_FAIL_SDP);
    }
}

/*
 * Free discovery database.
 */
void panu_sdp_free_db(pan_session_cb_t *p_scb)
{
    if ( p_scb->p_sdp_discovery_db != NULL )
    {
        wiced_bt_free_buffer(p_scb->p_sdp_discovery_db);
        p_scb->p_sdp_discovery_db = NULL;
    }
}

void wiced_bt_panu_connect(BD_ADDR bd_addr)
{
    pan_session_cb_t *p_scb = &sdp_panu_scb;

    if (p_scb->state != PANU_STATE_IDLE)
    {
        WICED_BT_TRACE("wiced_bt_panu_connect p_scb->state error : %d\n", p_scb->state);
        return;
    }

    p_scb->state = PANU_STATE_OPENING;

    /* store parameters */
    STREAM_TO_BDADDR(p_scb->remote_addr, bd_addr);

    p_scb->remote_profile_uuid = UUID_SERVCLASS_NAP;

    /* do service search */
    panu_sdp_start_discovery(p_scb);
}

void wiced_bt_panu_disconnect(uint16_t handle)
{
    pan_session_cb_t *p_scb = &sdp_panu_scb;

    WICED_BT_TRACE("[%u]wiced_bt_panu_disconnect State: %u\n", p_scb->app_handle, p_scb->state);

    if (handle != p_scb->app_handle)
    {
        WICED_BT_TRACE("handle error \n");
        return;
    }

    if (p_scb->state == PANU_STATE_OPENING)
    {
        p_scb->state = PANU_STATE_CLOSING;
    }
    else if (p_scb->state == PANU_STATE_CONNECT)
    {
        p_scb->state = PANU_STATE_CLOSING;
        tPAN_RESULT result = wiced_bt_pan_disconnect (p_scb->app_handle);
        WICED_BT_TRACE("wiced_bt_pan_disconnect result = %d\n", result);
        panu_disconnected(p_scb->app_handle);
    }
}

#endif
