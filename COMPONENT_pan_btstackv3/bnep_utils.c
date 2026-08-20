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

#if defined(WICED_APP_PANU_INCLUDED) || defined(WICED_APP_PANNAP_INCLUDED)

#include <stdio.h>
#include "bt_types.h"
#include "bnep_int.h"
#include "wiced_bt_trace.h"
#include "wiced_memory.h"


/********************************************************************************/
/*              L O C A L    F U N C T I O N     P R O T O T Y P E S            */
/********************************************************************************/
static uint8_t *bnepu_init_hdr (BT_HDR *p_buf, uint16_t hdr_len, uint8_t pkt_type);
void bnepu_process_peer_multicast_filter_set (tBNEP_CONN *p_bcb, uint8_t *p_filters,
                                              uint16_t len);
void bnepu_send_peer_multicast_filter_rsp (tBNEP_CONN *p_bcb, uint16_t response_code);
void *wiced_bt_bnep_get_pool_buf(void);

/*******************************************************************************
**
** Function         bnepu_find_bcb_by_cid
**
** Description      This function searches the bcb table for an entry with the
**                  passed CID.
**
** Returns          the BCB address, or NULL if not found.
**
*******************************************************************************/
tBNEP_CONN *bnepu_find_bcb_by_cid (uint16_t cid)
{
    uint16_t          xx;
    tBNEP_CONN     *p_bcb;

    /* Look through each connection control block */
    for (xx = 0, p_bcb = bnep_cb.bcb; xx < BNEP_MAX_CONNECTIONS; xx++, p_bcb++)
    {
        if ((p_bcb->con_state != BNEP_STATE_IDLE) && (p_bcb->l2cap_cid == cid))
            return (p_bcb);
    }

    /* If here, not found */
    return (NULL);
}


/*******************************************************************************
**
** Function         bnepu_find_bcb_by_bd_addr
**
** Description      This function searches the BCB table for an entry with the
**                  passed Bluetooth Address.
**
** Returns          the BCB address, or NULL if not found.
**
*******************************************************************************/
tBNEP_CONN *bnepu_find_bcb_by_bd_addr (uint8_t *p_bda)
{
    uint16_t          xx;
    tBNEP_CONN     *p_bcb;

    /* Look through each connection control block */
    for (xx = 0, p_bcb = bnep_cb.bcb; xx < BNEP_MAX_CONNECTIONS; xx++, p_bcb++)
    {
        if (p_bcb->con_state != BNEP_STATE_IDLE)
        {
            if (!memcmp ((uint8_t *)(p_bcb->rem_bda), p_bda, BD_ADDR_LEN))
                return (p_bcb);
        }
    }

    /* If here, not found */
    return (NULL);
}


/*******************************************************************************
**
** Function         bnepu_allocate_bcb
**
** Description      This function allocates a new BCB.
**
** Returns          BCB address, or NULL if none available.
**
*******************************************************************************/
tBNEP_CONN *bnepu_allocate_bcb (BD_ADDR p_rem_bda)
{
    uint16_t          xx;
    tBNEP_CONN     *p_bcb;

    /* Look through each connection control block for a free one */
    for (xx = 0, p_bcb = bnep_cb.bcb; xx < BNEP_MAX_CONNECTIONS; xx++, p_bcb++)
    {
        if (p_bcb->con_state == BNEP_STATE_IDLE)
        {
            memset ((uint8_t *)p_bcb, 0, sizeof (tBNEP_CONN));
            wiced_init_timer(&p_bcb->conn_tle, bnep_process_timeout, (wiced_timer_callback_arg_t)p_bcb, WICED_SECONDS_TIMER);
            memcpy ((uint8_t *)(p_bcb->rem_bda), (uint8_t *)p_rem_bda, BD_ADDR_LEN);
            p_bcb->handle = xx + 1;

            return (p_bcb);
        }
    }

    /* If here, no free BCB found */
    return (NULL);
}


/*******************************************************************************
**
** Function         bnepu_release_bcb
**
** Description      This function releases a BCB.
**
** Returns          void
**
*******************************************************************************/
void bnepu_release_bcb (tBNEP_CONN *p_bcb)
{
    /* Ensure timer is stopped */
    wiced_stop_timer(&p_bcb->conn_tle);
    /* Drop any response pointer we may be holding */
    p_bcb->con_state        = BNEP_STATE_IDLE;
    if(p_bcb->p_pending_data)
    {
        wiced_bt_free_buffer (p_bcb->p_pending_data);
    }
    p_bcb->p_pending_data   = NULL;

    /* Free transmit queue */
    while (p_bcb->xmit_q.count)
    {
#if BTSTACK_VER >= 0x03000001
        wiced_bt_free_buffer (wiced_bt_dequeue (&p_bcb->xmit_q));
#else // BTSTACK_VER
        wiced_bt_free_buffer (GKI_dequeue (&p_bcb->xmit_q));
#endif
    }

    while (p_bcb->xmit_pending_q.count)
    {
#if BTSTACK_VER >= 0x03000001
        wiced_bt_free_buffer (wiced_bt_dequeue (&p_bcb->xmit_pending_q));
#else // BTSTACK_VER
        wiced_bt_free_buffer (GKI_dequeue (&p_bcb->xmit_pending_q));
#endif
    }
}


/*******************************************************************************
**
** Function         bnep_send_conn_req
**
** Description      This function sends a BNEP connection request to peer
**
** Returns          void
**
*******************************************************************************/
void bnep_send_conn_req (tBNEP_CONN *p_bcb)
{
    BT_HDR  *p_buf;
    uint8_t   *p, *p_start;

    WICED_BT_TRACE ("BNEP sending setup req with dst uuid %x\n",
                       p_bcb->dst_uuid.uu.uuid16);

    if ((p_buf = (BT_HDR *)wiced_bt_bnep_get_pool_buf ()) == NULL)
    {
        /* Close L2C connection and give close indication to PAN */
        /* TBD */
        WICED_BT_TRACE ("BNEP - not able to send connection request\n");
        return;
    }

    p_buf->offset = L2CAP_MIN_OFFSET;
    p = p_start = (uint8_t *)(p_buf + 1) + L2CAP_MIN_OFFSET;

    /* Put in BNEP frame type - filter control */
    UINT8_TO_BE_STREAM (p, BNEP_FRAME_CONTROL);

    /* Put in filter message type - set filters */
    UINT8_TO_BE_STREAM (p, BNEP_SETUP_CONNECTION_REQUEST_MSG);

    UINT8_TO_BE_STREAM (p, p_bcb->dst_uuid.len);

    if (p_bcb->dst_uuid.len == 2)
    {
        UINT16_TO_BE_STREAM (p, p_bcb->dst_uuid.uu.uuid16);
        UINT16_TO_BE_STREAM (p, p_bcb->src_uuid.uu.uuid16);
    }
#if (defined (BNEP_SUPPORTS_ALL_UUID_LENGTHS) && BNEP_SUPPORTS_ALL_UUID_LENGTHS == TRUE)
    else if (p_bcb->dst_uuid.len == 4)
    {
        UINT32_TO_BE_STREAM (p, p_bcb->dst_uuid.uu.uuid32);
        UINT32_TO_BE_STREAM (p, p_bcb->src_uuid.uu.uuid32);
    }
    else
    {
        memcpy (p, p_bcb->dst_uuid.uu.uuid128, p_bcb->dst_uuid.len);
        p += p_bcb->dst_uuid.len;
        memcpy (p, p_bcb->src_uuid.uu.uuid128, p_bcb->dst_uuid.len);
        p += p_bcb->dst_uuid.len;
    }
#endif

    p_buf->len = (uint16_t)(p - p_start);

    bnepu_check_send_packet (p_bcb, p_buf);
}


/*******************************************************************************
**
** Function         bnep_send_conn_responce
**
** Description      This function sends a BNEP setup response to peer
**
** Returns          void
**
*******************************************************************************/
void bnep_send_conn_responce (tBNEP_CONN *p_bcb, uint16_t resp_code)
{
    BT_HDR  *p_buf;
    uint8_t   *p;

    WICED_BT_TRACE ("BNEP - bnep_send_conn_responce for CID: 0x%x", p_bcb->l2cap_cid);
    if ((p_buf = (BT_HDR *)wiced_bt_bnep_get_pool_buf ()) == NULL)
    {
        /* Close L2C connection and give close indication to PAN */
        /* TBD */
        WICED_BT_TRACE ("BNEP - not able to send connection response");
        return;
    }

    p_buf->offset = L2CAP_MIN_OFFSET;
    p = (uint8_t *)(p_buf + 1) + L2CAP_MIN_OFFSET;

    /* Put in BNEP frame type - filter control */
    UINT8_TO_BE_STREAM (p, BNEP_FRAME_CONTROL);

    /* Put in filter message type - set filters */
    UINT8_TO_BE_STREAM (p, BNEP_SETUP_CONNECTION_RESPONSE_MSG);

    UINT16_TO_BE_STREAM (p, resp_code);

    p_buf->len = 4;

    bnepu_check_send_packet (p_bcb, p_buf);
}


/*******************************************************************************
**
** Function         bnepu_send_peer_our_filters
**
** Description      This function sends our filters to a peer
**
** Returns          void
**
*******************************************************************************/
void bnepu_send_peer_our_filters (tBNEP_CONN *p_bcb)
{
#if (defined (BNEP_SUPPORTS_PROT_FILTERS) && BNEP_SUPPORTS_PROT_FILTERS == TRUE)
    BT_HDR      *p_buf;
    uint8_t       *p;
    uint16_t      xx;

    WICED_BT_TRACE ("BNEP sending peer our filters");

    if ((p_buf = (BT_HDR *)wiced_bt_bnep_get_pool_buf ()) == NULL)
    {
        WICED_BT_TRACE ("BNEP - no buffer send filters");
        return;
    }

    p_buf->offset = L2CAP_MIN_OFFSET;
    p = (uint8_t *)(p_buf + 1) + L2CAP_MIN_OFFSET;

    /* Put in BNEP frame type - filter control */
    UINT8_TO_BE_STREAM (p, BNEP_FRAME_CONTROL);

    /* Put in filter message type - set filters */
    UINT8_TO_BE_STREAM (p, BNEP_FILTER_NET_TYPE_SET_MSG);

    UINT16_TO_BE_STREAM (p, (4 * p_bcb->sent_num_filters));
    for (xx = 0; xx < p_bcb->sent_num_filters; xx++)
    {
        UINT16_TO_BE_STREAM (p, p_bcb->sent_prot_filter_start[xx]);
        UINT16_TO_BE_STREAM (p, p_bcb->sent_prot_filter_end[xx]);
    }

    p_buf->len = 4 + (4 * p_bcb->sent_num_filters);

    bnepu_check_send_packet (p_bcb, p_buf);

    p_bcb->con_flags |= BNEP_FLAGS_FILTER_RESP_PEND;

    /* Start timer waiting for setup response */
    wiced_start_timer(&p_bcb->conn_tle, BNEP_FILTER_SET_TIMEOUT);
#endif
}


/*******************************************************************************
**
** Function         bnepu_send_peer_our_multi_filters
**
** Description      This function sends our multicast filters to a peer
**
** Returns          void
**
*******************************************************************************/
void bnepu_send_peer_our_multi_filters (tBNEP_CONN *p_bcb)
{
#if (defined (BNEP_SUPPORTS_MULTI_FILTERS) && BNEP_SUPPORTS_MULTI_FILTERS == TRUE)
    BT_HDR      *p_buf;
    uint8_t       *p;
    uint16_t      xx;

    WICED_BT_TRACE ("BNEP sending peer our multicast filters");

    if ((p_buf = (BT_HDR *)wiced_bt_bnep_get_pool_buf ()) == NULL)
    {
        WICED_BT_TRACE ("BNEP - no buffer to send multicast filters");
        return;
    }

    p_buf->offset = L2CAP_MIN_OFFSET;
    p = (uint8_t *)(p_buf + 1) + L2CAP_MIN_OFFSET;

    /* Put in BNEP frame type - filter control */
    UINT8_TO_BE_STREAM (p, BNEP_FRAME_CONTROL);

    /* Put in filter message type - set filters */
    UINT8_TO_BE_STREAM (p, BNEP_FILTER_MULTI_ADDR_SET_MSG);

    UINT16_TO_BE_STREAM (p, (2 * BD_ADDR_LEN * p_bcb->sent_mcast_filters));
    for (xx = 0; xx < p_bcb->sent_mcast_filters; xx++)
    {
        memcpy (p, p_bcb->sent_mcast_filter_start[xx], BD_ADDR_LEN);
        p += BD_ADDR_LEN;
        memcpy (p, p_bcb->sent_mcast_filter_end[xx], BD_ADDR_LEN);
        p += BD_ADDR_LEN;
    }

    p_buf->len = 4 + (2 * BD_ADDR_LEN * p_bcb->sent_mcast_filters);

    bnepu_check_send_packet (p_bcb, p_buf);

    p_bcb->con_flags |= BNEP_FLAGS_MULTI_RESP_PEND;

    /* Start timer waiting for setup response */
    wiced_start_timer(&p_bcb->conn_tle, BNEP_FILTER_SET_TIMEOUT);
#endif
}


/*******************************************************************************
**
** Function         bnepu_send_peer_filter_rsp
**
** Description      This function sends a filter response to a peer
**
** Returns          void
**
*******************************************************************************/
void bnepu_send_peer_filter_rsp (tBNEP_CONN *p_bcb, uint16_t response_code)
{
    BT_HDR  *p_buf;
    uint8_t   *p;

    WICED_BT_TRACE ("BNEP sending filter response");
    if ((p_buf = (BT_HDR *)wiced_bt_bnep_get_pool_buf ()) == NULL)
    {
        WICED_BT_TRACE ("BNEP - no buffer filter rsp");
        return;
    }

    p_buf->offset = L2CAP_MIN_OFFSET;
    p = (uint8_t *)(p_buf + 1) + L2CAP_MIN_OFFSET;

    /* Put in BNEP frame type - filter control */
    UINT8_TO_BE_STREAM (p, BNEP_FRAME_CONTROL);

    /* Put in filter message type - set filters */
    UINT8_TO_BE_STREAM (p, BNEP_FILTER_NET_TYPE_RESPONSE_MSG);

    UINT16_TO_BE_STREAM (p, response_code);

    p_buf->len = 4;

    bnepu_check_send_packet (p_bcb, p_buf);
}


/*******************************************************************************
**
** Function         bnep_send_command_not_understood
**
** Description      This function sends a BNEP command not understood message
**
** Returns          void
**
*******************************************************************************/
void bnep_send_command_not_understood (tBNEP_CONN *p_bcb, uint8_t cmd_code)
{
    BT_HDR  *p_buf;
    uint8_t   *p;

    WICED_BT_TRACE ("BNEP - bnep_send_command_not_understood for CID: 0x%x, cmd 0x%x",
                       p_bcb->l2cap_cid, cmd_code);
    if ((p_buf = (BT_HDR *)wiced_bt_bnep_get_pool_buf ()) == NULL)
    {
        /* Close L2C connection and give close indication to PAN */
        /* TBD */
        WICED_BT_TRACE ("BNEP - not able to send connection response");
        return;
    }

    p_buf->offset = L2CAP_MIN_OFFSET;
    p = (uint8_t *)(p_buf + 1) + L2CAP_MIN_OFFSET;

    /* Put in BNEP frame type - filter control */
    UINT8_TO_BE_STREAM (p, BNEP_FRAME_CONTROL);

    /* Put in filter message type - set filters */
    UINT8_TO_BE_STREAM (p, BNEP_CONTROL_COMMAND_NOT_UNDERSTOOD);

    UINT8_TO_BE_STREAM (p, cmd_code);

    p_buf->len = 3;

    bnepu_check_send_packet (p_bcb, p_buf);
}


/*******************************************************************************
**
** Function         bnepu_check_send_packet
**
** Description      This function tries to send a packet to L2CAP.
**                  If L2CAP is flow controlled, it enqueues the
**                  packet to the transmit queue
**
** Returns          void
**
*******************************************************************************/
void bnepu_check_send_packet (tBNEP_CONN *p_bcb, BT_HDR *p_buf)
{
    if (p_bcb->con_flags & BNEP_FLAGS_L2CAP_CONGESTED)
    {
        if (p_bcb->xmit_q.count >= BNEP_MAX_XMITQ_DEPTH)
        {
            WICED_BT_TRACE ("BNEP - congested, dropping buf, CID: 0x%x\n", p_bcb->l2cap_cid);
            wiced_bt_free_buffer (p_buf);
        }
        else
        {
#if BTSTACK_VER >= 0x03000001
            wiced_bt_enqueue(&p_bcb->xmit_q, (wiced_bt_buffer_t *)p_buf);
#else // BTSTACK_VER
            GKI_enqueue(&p_bcb->xmit_q, (void *)p_buf);
#endif // BTSTACK_VER
        }
    }
    else
    {
        if(p_bcb->xmit_pending_q.count >= BNEP_XMIT_PENDING_HIGH_DEPTH)
        {
            p_bcb->con_flags |= BNEP_FLAGS_L2CAP_CONGESTED;
            if(bnep_cb.p_tx_data_flow_cb)
            {
                bnep_cb.p_tx_data_flow_cb(p_bcb->handle, BNEP_TX_FLOW_OFF);
            }
#if BTSTACK_VER >= 0x03000001
            wiced_bt_enqueue(&p_bcb->xmit_q, (wiced_bt_buffer_t *)p_buf);
#else // BTSTACK_VER
            GKI_enqueue(&p_bcb->xmit_q, (void *)p_buf);
#endif // BTSTACK_VER
        }
        else
        {
#if BTSTACK_VER >= 0x03000001
            wiced_bt_enqueue(&p_bcb->xmit_pending_q, (wiced_bt_buffer_t *)p_buf);
#endif // BTSTACK_VER
            p_bcb->xmit_pending_num++;
            if(L2CAP_DATAWRITE_SUCCESS != wiced_bt_l2cap_data_write (p_bcb->l2cap_cid, (uint8_t *)(p_buf + 1) + p_buf->offset, p_buf->len, L2CAP_FLUSHABLE_PACKET))
            {
                p_bcb->xmit_pending_num--;
            }
        }
    }
}


/*******************************************************************************
**
** Function         bnepu_build_bnep_hdr
**
** Description      This function builds the BNEP header for a packet
**                  Extension headers are not sent yet, so there is no
**                  check for that.
**
** Returns          void
**
*******************************************************************************/
void bnepu_build_bnep_hdr (tBNEP_CONN *p_bcb, BT_HDR *p_buf, uint16_t protocol,
                          uint8_t *p_src_addr, uint8_t *p_dest_addr, BOOLEAN fw_ext_present)
{
    uint8_t    ext_bit, *p = (uint8_t *)NULL;
    uint8_t    type = BNEP_FRAME_COMPRESSED_ETHERNET;

    ext_bit = fw_ext_present ? 0x80 : 0x00;

    if ((p_src_addr) && (memcmp (p_src_addr, bnep_cb.my_bda, BD_ADDR_LEN)))
        type = BNEP_FRAME_COMPRESSED_ETHERNET_SRC_ONLY;

    if (memcmp (p_dest_addr, p_bcb->rem_bda, BD_ADDR_LEN))
        type = (type == BNEP_FRAME_COMPRESSED_ETHERNET) ?
                        BNEP_FRAME_COMPRESSED_ETHERNET_DEST_ONLY :
                        BNEP_FRAME_GENERAL_ETHERNET;

    if (!p_src_addr)
        p_src_addr = (uint8_t *)bnep_cb.my_bda;

    switch (type)
    {
    case BNEP_FRAME_GENERAL_ETHERNET:
        p = bnepu_init_hdr (p_buf, 15, (uint8_t)(ext_bit | BNEP_FRAME_GENERAL_ETHERNET));

        memcpy (p, p_dest_addr, BD_ADDR_LEN);
        p += BD_ADDR_LEN;

        memcpy (p, p_src_addr, BD_ADDR_LEN);
        p += BD_ADDR_LEN;
        break;

    case BNEP_FRAME_COMPRESSED_ETHERNET:
        p = bnepu_init_hdr (p_buf, 3, (uint8_t)(ext_bit | BNEP_FRAME_COMPRESSED_ETHERNET));
        break;

    case BNEP_FRAME_COMPRESSED_ETHERNET_SRC_ONLY:
        p = bnepu_init_hdr (p_buf, 9, (uint8_t)(ext_bit
                    | BNEP_FRAME_COMPRESSED_ETHERNET_SRC_ONLY));

        memcpy (p, p_src_addr, BD_ADDR_LEN);
        p += BD_ADDR_LEN;
        break;

    case BNEP_FRAME_COMPRESSED_ETHERNET_DEST_ONLY:
        p = bnepu_init_hdr (p_buf, 9, (uint8_t)(ext_bit
                    | BNEP_FRAME_COMPRESSED_ETHERNET_DEST_ONLY));

        memcpy (p, p_dest_addr, BD_ADDR_LEN);
        p += BD_ADDR_LEN;
        break;
    }

    UINT16_TO_BE_STREAM (p, protocol);
}


/*******************************************************************************
**
** Function         bnepu_init_hdr
**
** Description      This function initializes the BNEP header
**
** Returns          pointer to header in buffer
**
*******************************************************************************/
static uint8_t *bnepu_init_hdr (BT_HDR *p_buf, uint16_t hdr_len, uint8_t pkt_type)
{
    uint8_t    *p = (uint8_t *)(p_buf + 1) + p_buf->offset;

    /* See if we need to make space in the buffer */
    if (p_buf->offset < (hdr_len + L2CAP_MIN_OFFSET))
    {
        uint16_t xx, diff = BNEP_MINIMUM_OFFSET - p_buf->offset;
        p = p + p_buf->len - 1;
        for (xx = 0; xx < p_buf->len; xx++, p--)
            p[diff] = *p;

        p_buf->offset = BNEP_MINIMUM_OFFSET;
        p = (uint8_t *)(p_buf + 1) + p_buf->offset;
    }

    p_buf->len    += hdr_len;
    p_buf->offset -= hdr_len;
    p             -= hdr_len;

    *p++ = pkt_type;

    return (p);
}


/*******************************************************************************
**
** Function         bnep_process_setup_conn_req
**
** Description      This function processes a peer's setup connection request
**                  message. The destination UUID is verified and response sent
**                  Connection open indication will be given to PAN profile
**
** Returns          void
**
*******************************************************************************/
void bnep_process_setup_conn_req (tBNEP_CONN *p_bcb, uint8_t *p_setup, uint8_t len)
{
    WICED_BT_TRACE ("BNEP - bnep_process_setup_conn_req for CID: 0x%x",
                        p_bcb->l2cap_cid);
    WICED_BT_TRACE ("BNEP - bnep_process_setup_conn_req p_bcb->con_state = %d", p_bcb->con_state);
    WICED_BT_TRACE ("BNEP - bnep_process_setup_conn_req p_bcb->con_flags = 0x%x", p_bcb->con_flags);

    if (p_bcb->con_state != BNEP_STATE_CONN_SETUP &&
        p_bcb->con_state != BNEP_STATE_SEC_CHECKING &&
        p_bcb->con_state != BNEP_STATE_CONNECTED)
    {
        WICED_BT_TRACE ("BNEP - setup request in bad state %d", p_bcb->con_state);
        bnep_send_conn_responce (p_bcb, BNEP_SETUP_CONN_NOT_ALLOWED);
        return;
    }

    /* Check if we already initiated security check or if waiting for user responce */
    if (p_bcb->con_flags & BNEP_FLAGS_SETUP_RCVD)
    {
        WICED_BT_TRACE ("BNEP - Duplicate Setup message "
                           "received while doing security check");
        return;
    }

    /* Check if peer is the originator */
    if (p_bcb->con_state != BNEP_STATE_CONNECTED &&
        (!(p_bcb->con_flags & BNEP_FLAGS_SETUP_RCVD)) &&
        (p_bcb->con_flags & BNEP_FLAGS_IS_ORIG))
    {
        WICED_BT_TRACE ("BNEP - setup request when we are originator",
                           p_bcb->con_state);
        bnep_send_conn_responce (p_bcb, BNEP_SETUP_CONN_NOT_ALLOWED);
        return;
    }

    if (p_bcb->con_state == BNEP_STATE_CONNECTED)
    {
        memcpy ((uint8_t *)&(p_bcb->prv_src_uuid), (uint8_t *)&(p_bcb->src_uuid),
                sizeof (tBT_UUID));
        memcpy ((uint8_t *)&(p_bcb->prv_dst_uuid), (uint8_t *)&(p_bcb->dst_uuid),
                sizeof (tBT_UUID));
    }

    p_bcb->dst_uuid.len = p_bcb->src_uuid.len = len;

    if (p_bcb->dst_uuid.len == 2)
    {
        /* because peer initiated connection keep src uuid as dst uuid */
        BE_STREAM_TO_UINT16 (p_bcb->src_uuid.uu.uuid16, p_setup);
        BE_STREAM_TO_UINT16 (p_bcb->dst_uuid.uu.uuid16, p_setup);

        /* If nothing has changed don't bother the profile */
        if (p_bcb->con_state == BNEP_STATE_CONNECTED &&
            p_bcb->src_uuid.uu.uuid16 == p_bcb->prv_src_uuid.uu.uuid16 &&
            p_bcb->dst_uuid.uu.uuid16 == p_bcb->prv_dst_uuid.uu.uuid16)
        {
            bnep_send_conn_responce (p_bcb, BNEP_SETUP_CONN_OK);
            return;
        }
    }
#if (defined (BNEP_SUPPORTS_ALL_UUID_LENGTHS) && BNEP_SUPPORTS_ALL_UUID_LENGTHS == TRUE)
    else if (p_bcb->dst_uuid.len == 4)
    {
        BE_STREAM_TO_UINT32 (p_bcb->src_uuid.uu.uuid32, p_setup);
        BE_STREAM_TO_UINT32 (p_bcb->dst_uuid.uu.uuid32, p_setup);
    }
    else if (p_bcb->dst_uuid.len == 16)
    {
        memcpy (p_bcb->src_uuid.uu.uuid128, p_setup, p_bcb->src_uuid.len);
        p_setup += p_bcb->src_uuid.len;
        memcpy (p_bcb->dst_uuid.uu.uuid128, p_setup, p_bcb->dst_uuid.len);
        p_setup += p_bcb->dst_uuid.len;
    }
#endif
    else
    {
        WICED_BT_TRACE ("BNEP - Bad UID len %d in ConnReq", p_bcb->dst_uuid.len);
        bnep_send_conn_responce (p_bcb, BNEP_SETUP_INVALID_UUID_SIZE);
        return;
    }

    p_bcb->con_state = BNEP_STATE_SEC_CHECKING;
    p_bcb->con_flags |= BNEP_FLAGS_SETUP_RCVD;

    WICED_BT_TRACE ("BNEP initiating security check for incoming call for uuid 0x%x",
                       p_bcb->src_uuid.uu.uuid16);
    bnep_sec_check_complete (p_bcb->rem_bda, FALSE, p_bcb, WICED_BT_SUCCESS);
    return;
}


/*******************************************************************************
**
** Function         bnep_process_setup_conn_responce
**
** Description      This function processes a peer's setup connection response
**                  message. The response code is verified and
**                  Connection open indication will be given to PAN profile
**
** Returns          void
**
*******************************************************************************/
void bnep_process_setup_conn_responce (tBNEP_CONN *p_bcb, uint8_t *p_setup)
{
    tBNEP_RESULT    resp;
    uint16_t          resp_code;

    WICED_BT_TRACE ("BNEP received setup responce\n");
    /* The state should be either SETUP or CONNECTED */
    if (p_bcb->con_state != BNEP_STATE_CONN_SETUP)
    {
        /* Should we disconnect ? */
        WICED_BT_TRACE ("BNEP - setup response in bad state %d\n", p_bcb->con_state);
        return;
    }

    /* Check if we are the originator */
    if (!(p_bcb->con_flags & BNEP_FLAGS_IS_ORIG))
    {
        WICED_BT_TRACE ("BNEP - setup response when we are not originator\n",
                           p_bcb->con_state);
        return;
    }

    BE_STREAM_TO_UINT16  (resp_code, p_setup);

    switch (resp_code)
    {
    case BNEP_SETUP_INVALID_SRC_UUID:
        resp = BNEP_CONN_FAILED_SRC_UUID;
        break;

    case BNEP_SETUP_INVALID_DEST_UUID:
        resp = BNEP_CONN_FAILED_DST_UUID;
        break;

    case BNEP_SETUP_INVALID_UUID_SIZE:
        resp = BNEP_CONN_FAILED_UUID_SIZE;
        break;

    case BNEP_SETUP_CONN_NOT_ALLOWED:
    default:
        resp = BNEP_CONN_FAILED;
        break;
    }

    /* Check the responce code */
    if (resp_code != BNEP_SETUP_CONN_OK)
    {
        if (p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)
        {
            WICED_BT_TRACE ("BNEP - role change response is %d\n", resp_code);

            /* Restore the earlier BNEP status */
            p_bcb->con_state = BNEP_STATE_CONNECTED;
            p_bcb->con_flags &= (~BNEP_FLAGS_SETUP_RCVD);
            memcpy ((uint8_t *)&(p_bcb->src_uuid), (uint8_t *)&(p_bcb->prv_src_uuid),
                    sizeof (tBT_UUID));
            memcpy ((uint8_t *)&(p_bcb->dst_uuid), (uint8_t *)&(p_bcb->prv_dst_uuid),
                    sizeof (tBT_UUID));

            /* Ensure timer is stopped */
            wiced_stop_timer(&p_bcb->conn_tle);
            p_bcb->re_transmits = 0;
            p_bcb->xmit_pending_num = 0;

            /* Tell the user if he has a callback */
            if (bnep_cb.p_conn_state_cb)
                (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda, resp, TRUE);

            return;
        }
        else
        {
            WICED_BT_TRACE ("BNEP - setup response %d is not OK\n", resp_code);

            wiced_bt_l2cap_disconnect_req(p_bcb->l2cap_cid);
            /* Tell the user if he has a callback */
            if ((p_bcb->con_flags & BNEP_FLAGS_IS_ORIG) && (bnep_cb.p_conn_state_cb))
                (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda, resp, FALSE);

            bnepu_release_bcb (p_bcb);
            return;
        }
    }

    /* Received successful responce */
    bnep_connected (p_bcb);
}


/*******************************************************************************
**
** Function         bnep_process_control_packet
**
** Description      This function processes a peer's setup connection request
**                  message. The destination UUID is verified and response sent
**                  Connection open indication will be given to PAN profile
**
** Returns          void
**
*******************************************************************************/
uint8_t *bnep_process_control_packet (tBNEP_CONN *p_bcb, uint8_t *p, uint16_t *p_rem_len,
                                    BOOLEAN is_ext)
{
    uint8_t       control_type;
    BOOLEAN     bad_pkt = FALSE;
    uint16_t      len, ext_len = 0;
    uint16_t      orig_rem_len = *p_rem_len;

    if (is_ext)
    {
        ext_len = *p++;
        *p_rem_len = *p_rem_len - 1;
    }

    control_type = *p++;
    *p_rem_len = *p_rem_len - 1;

    WICED_BT_TRACE ("bnep_process_control_packet BNEP proc'ing control pkt p_rem_len %d, is_ext %d, ctrl_type %d\n",
                       *p_rem_len, is_ext, control_type);

    /* check for underflow */
    if (*p_rem_len > orig_rem_len)
    {
        WICED_BT_TRACE ("BNEP - bad crl pkt header length : -x%04x\n", *p_rem_len);
        *p_rem_len = 0;
        return NULL;
    }

    switch (control_type)
    {
    case BNEP_CONTROL_COMMAND_NOT_UNDERSTOOD:
        WICED_BT_TRACE ("BNEP Received Cmd not understood for ctl pkt type: %d\n", *p);
        p++;
        *p_rem_len = *p_rem_len - 1;
        break;

    case BNEP_SETUP_CONNECTION_REQUEST_MSG:
        len = *p++;
        if (*p_rem_len < ((2 * len) + 1))
        {
            bad_pkt = TRUE;
            WICED_BT_TRACE("Received BNEP_SETUP_CONNECTION_REQUEST_MSG with bad length\n");
            break;
        }
        if (!is_ext)
            bnep_process_setup_conn_req (p_bcb, p, (uint8_t)len);
        p += (2 * len);
        *p_rem_len = *p_rem_len - (2 * len) - 1;
        break;

    case BNEP_SETUP_CONNECTION_RESPONSE_MSG:
        if (!is_ext)
            bnep_process_setup_conn_responce (p_bcb, p);
        p += 2;
        *p_rem_len = *p_rem_len - 2;
        break;

    case BNEP_FILTER_NET_TYPE_SET_MSG:
        BE_STREAM_TO_UINT16 (len, p);
        if (*p_rem_len < (len + 2))
        {
            bad_pkt = TRUE;
            WICED_BT_TRACE("Received BNEP_FILTER_NET_TYPE_SET_MSG with bad length\n");
            break;
        }
        bnepu_process_peer_filter_set (p_bcb, p, len);
        p += len;
        *p_rem_len = *p_rem_len - len - 2;
        break;

    case BNEP_FILTER_NET_TYPE_RESPONSE_MSG:
        bnepu_process_peer_filter_rsp (p_bcb, p);
        p += 2;
        *p_rem_len = *p_rem_len - 2;
        break;

    case BNEP_FILTER_MULTI_ADDR_SET_MSG:
        BE_STREAM_TO_UINT16 (len, p);
        if (*p_rem_len < (len + 2))
        {
            bad_pkt = TRUE;
            WICED_BT_TRACE ("BNEP Received Multicast Filter Set message with bad length\n");
            break;
        }
        bnepu_process_peer_multicast_filter_set (p_bcb, p, len);
        p += len;
        *p_rem_len = *p_rem_len - len - 2;
        break;

    case BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG:
        bnepu_process_multicast_filter_rsp (p_bcb, p);
        p += 2;
        *p_rem_len = *p_rem_len - 2;
        break;

    default :
        WICED_BT_TRACE ("BNEP - bad ctl pkt type: %d\n", control_type);
        bnep_send_command_not_understood (p_bcb, control_type);
        if (is_ext)
        {
            p += (ext_len - 1);
            *p_rem_len -= (ext_len - 1);
        }
        break;
    }

    if(bad_pkt || (*p_rem_len > orig_rem_len)) /* Add check for underflow */
    {
        WICED_BT_TRACE ("BNEP - bad ctl pkt length: 0x%04x\n", *p_rem_len);
        *p_rem_len = 0;
        return NULL;
    }
    return p;
}


/*******************************************************************************
**
** Function         bnepu_process_peer_filter_set
**
** Description      This function processes a peer's filter control
**                  'set' message. The filters are stored in the BCB,
**                  and an appropriate filter response message sent.
**
** Returns          void
**
*******************************************************************************/
void bnepu_process_peer_filter_set (tBNEP_CONN *p_bcb, uint8_t *p_filters, uint16_t len)
{
#if (defined (BNEP_SUPPORTS_PROT_FILTERS) && BNEP_SUPPORTS_PROT_FILTERS == TRUE)
    uint16_t      num_filters = 0;
    uint16_t      xx, resp_code = BNEP_FILTER_CRL_OK;
    uint16_t      start, end;
    uint8_t       *p_temp_filters;

    if ((p_bcb->con_state != BNEP_STATE_CONNECTED) &&
        (!(p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)))
    {
        WICED_BT_TRACE ("BNEP rec'd filter set from peer when no connection");
        return;
    }

    WICED_BT_TRACE ("BNEP received filter set from peer");
    /* Check for length not a multiple of 4 */
    if (len & 3)
    {
        WICED_BT_TRACE ("BNEP - bad filter len: %d", len);
        bnepu_send_peer_filter_rsp (p_bcb, BNEP_FILTER_CRL_BAD_RANGE);
        return;
    }

    if (len)
        num_filters = (uint16_t) (len >> 2);

    /* Validate filter values */
    if (num_filters <= BNEP_MAX_PROT_FILTERS)
    {
        p_temp_filters = p_filters;
        for (xx = 0; xx < num_filters; xx++)
        {
            BE_STREAM_TO_UINT16  (start, p_temp_filters);
            BE_STREAM_TO_UINT16  (end,   p_temp_filters);

            if (start > end)
            {
                resp_code = BNEP_FILTER_CRL_BAD_RANGE;
                break;
            }
        }
    }
    else
        resp_code   = BNEP_FILTER_CRL_MAX_REACHED;

    if (resp_code != BNEP_FILTER_CRL_OK)
    {
        bnepu_send_peer_filter_rsp (p_bcb, resp_code);
        return;
    }

    if (bnep_cb.p_filter_ind_cb)
        (*bnep_cb.p_filter_ind_cb) (p_bcb->handle, TRUE, 0, len, p_filters);

    p_bcb->rcvd_num_filters = num_filters;
    for (xx = 0; xx < num_filters; xx++)
    {
        BE_STREAM_TO_UINT16  (start, p_filters);
        BE_STREAM_TO_UINT16  (end,   p_filters);

        p_bcb->rcvd_prot_filter_start[xx] = start;
        p_bcb->rcvd_prot_filter_end[xx]   = end;
    }

    bnepu_send_peer_filter_rsp (p_bcb, resp_code);
#else
    bnepu_send_peer_filter_rsp (p_bcb, BNEP_FILTER_CRL_UNSUPPORTED);
#endif
}


/*******************************************************************************
**
** Function         bnepu_process_peer_filter_rsp
**
** Description      This function processes a peer's filter control
**                  'response' message.
**
** Returns          void
**
*******************************************************************************/
void bnepu_process_peer_filter_rsp (tBNEP_CONN *p_bcb, uint8_t *p_data)
{
#if (defined (BNEP_SUPPORTS_PROT_FILTERS) && BNEP_SUPPORTS_PROT_FILTERS == TRUE)
    uint16_t          resp_code;
    tBNEP_RESULT    result;

    WICED_BT_TRACE ("BNEP received filter responce");
    /* The state should be  CONNECTED */
    if ((p_bcb->con_state != BNEP_STATE_CONNECTED) &&
        (!(p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)))
    {
        WICED_BT_TRACE ("BNEP - filter response in bad state %d", p_bcb->con_state);
        return;
    }

    /* Check if we are the originator */
    if (!(p_bcb->con_flags & BNEP_FLAGS_FILTER_RESP_PEND))
    {
        WICED_BT_TRACE ("BNEP - filter response when not expecting");
        return;
    }

    /* Ensure timer is stopped */
    wiced_stop_timer(&p_bcb->conn_tle);
    p_bcb->con_flags &= ~BNEP_FLAGS_FILTER_RESP_PEND;
    p_bcb->re_transmits = 0;
    p_bcb->xmit_pending_num = 0;

    BE_STREAM_TO_UINT16  (resp_code, p_data);

    result = BNEP_SUCCESS;
    if (resp_code != BNEP_FILTER_CRL_OK)
        result = BNEP_SET_FILTER_FAIL;

    if (bnep_cb.p_filter_ind_cb)
        (*bnep_cb.p_filter_ind_cb) (p_bcb->handle, FALSE, result, 0, NULL);

    return;
#endif
}


/*******************************************************************************
**
** Function         bnepu_process_multicast_filter_rsp
**
** Description      This function processes multicast filter control
**                  'response' message.
**
** Returns          void
**
*******************************************************************************/
void bnepu_process_multicast_filter_rsp (tBNEP_CONN *p_bcb, uint8_t *p_data)
{
#if (defined (BNEP_SUPPORTS_MULTI_FILTERS) && BNEP_SUPPORTS_MULTI_FILTERS == TRUE)
    uint16_t          resp_code;
    tBNEP_RESULT    result;

    WICED_BT_TRACE ("BNEP received multicast filter responce");
    /* The state should be  CONNECTED */
    if ((p_bcb->con_state != BNEP_STATE_CONNECTED) &&
        (!(p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)))
    {
        WICED_BT_TRACE ("BNEP - multicast filter response in bad state %d",
                           p_bcb->con_state);
        return;
    }

    /* Check if we are the originator */
    if (!(p_bcb->con_flags & BNEP_FLAGS_MULTI_RESP_PEND))
    {
        WICED_BT_TRACE ("BNEP - multicast filter response when not expecting");
        return;
    }

    /* Ensure timer is stopped */
    wiced_stop_timer(&p_bcb->conn_tle);
    p_bcb->con_flags &= ~BNEP_FLAGS_MULTI_RESP_PEND;
    p_bcb->re_transmits = 0;
    p_bcb->xmit_pending_num = 0;

    BE_STREAM_TO_UINT16  (resp_code, p_data);

    result = BNEP_SUCCESS;
    if (resp_code != BNEP_FILTER_CRL_OK)
        result = BNEP_SET_FILTER_FAIL;

    if (bnep_cb.p_mfilter_ind_cb)
        (*bnep_cb.p_mfilter_ind_cb) (p_bcb->handle, FALSE, result, 0, NULL);

    return;
#endif
}


/*******************************************************************************
**
** Function         bnepu_process_peer_multicast_filter_set
**
** Description      This function processes a peer's filter control
**                  'set' message. The filters are stored in the BCB,
**                  and an appropriate filter response message sent.
**
** Returns          void
**
*******************************************************************************/
void bnepu_process_peer_multicast_filter_set (tBNEP_CONN *p_bcb, uint8_t *p_filters,
                                              uint16_t len)
{
#if (defined (BNEP_SUPPORTS_MULTI_FILTERS) && BNEP_SUPPORTS_MULTI_FILTERS == TRUE)
    uint16_t          resp_code = BNEP_FILTER_CRL_OK;
    uint16_t          num_filters, xx;
    uint8_t           *p_temp_filters, null_bda[BD_ADDR_LEN] = {0,0,0,0,0,0};

    if ((p_bcb->con_state != BNEP_STATE_CONNECTED) &&
        (!(p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)))
    {
        WICED_BT_TRACE ("BNEP rec'd multicast filt set from peer when no connection");
        return;
    }

    if (len % 12)
    {
        WICED_BT_TRACE ("BNEP - bad filter len: %d", len);
        bnepu_send_peer_multicast_filter_rsp (p_bcb, BNEP_FILTER_CRL_BAD_RANGE);
        return;
    }

    if (len > (BNEP_MAX_MULTI_FILTERS * 2 * BD_ADDR_LEN))
    {
        WICED_BT_TRACE ("BNEP - Too many filters");
        bnepu_send_peer_multicast_filter_rsp (p_bcb, BNEP_FILTER_CRL_MAX_REACHED);
        return;
    }

    num_filters = 0;
    if (len)
        num_filters = (uint16_t) (len / 12);

    /* Validate filter values */
    if (num_filters <= BNEP_MAX_MULTI_FILTERS)
    {
        p_temp_filters = p_filters;
        for (xx = 0; xx < num_filters; xx++)
        {
            if (memcmp (p_temp_filters, p_temp_filters + BD_ADDR_LEN, BD_ADDR_LEN) > 0)
            {
                bnepu_send_peer_multicast_filter_rsp (p_bcb, BNEP_FILTER_CRL_BAD_RANGE);
                return;
            }

            p_temp_filters += (BD_ADDR_LEN * 2);
        }
    }

    p_bcb->rcvd_mcast_filters = num_filters;
    p_temp_filters = p_filters;
    for (xx = 0; xx < num_filters; xx++)
    {
        memcpy (p_bcb->rcvd_mcast_filter_start[xx], p_temp_filters, BD_ADDR_LEN);
        memcpy (p_bcb->rcvd_mcast_filter_end[xx], p_temp_filters + BD_ADDR_LEN,
                BD_ADDR_LEN);
        p_temp_filters += (BD_ADDR_LEN * 2);

        /* Check if any ranges have all zeros as both starting and ending addresses */
        if ((memcmp (null_bda, p_bcb->rcvd_mcast_filter_start[xx], BD_ADDR_LEN) == 0) &&
            (memcmp (null_bda, p_bcb->rcvd_mcast_filter_end[xx], BD_ADDR_LEN) == 0))
        {
            p_bcb->rcvd_mcast_filters = 0xFFFF;
            break;
        }
    }

    WICED_BT_TRACE ("BNEP multicast filters %d", p_bcb->rcvd_mcast_filters);
    bnepu_send_peer_multicast_filter_rsp (p_bcb, resp_code);

    if (bnep_cb.p_mfilter_ind_cb)
        (*bnep_cb.p_mfilter_ind_cb) (p_bcb->handle, TRUE, 0, len, p_filters);
#else
    bnepu_send_peer_multicast_filter_rsp (p_bcb, BNEP_FILTER_CRL_UNSUPPORTED);
#endif
}


/*******************************************************************************
**
** Function         bnepu_send_peer_multicast_filter_rsp
**
** Description      This function sends a filter response to a peer
**
** Returns          void
**
*******************************************************************************/
void bnepu_send_peer_multicast_filter_rsp (tBNEP_CONN *p_bcb, uint16_t response_code)
{
    BT_HDR  *p_buf;
    uint8_t   *p;

    WICED_BT_TRACE ("BNEP sending multicast filter response %d", response_code);
    if ((p_buf = (BT_HDR *)wiced_bt_bnep_get_pool_buf ()) == NULL)
    {
        WICED_BT_TRACE ("BNEP - no buffer filter rsp");
        return;
    }

    p_buf->offset = L2CAP_MIN_OFFSET;
    p = (uint8_t *)(p_buf + 1) + L2CAP_MIN_OFFSET;

    /* Put in BNEP frame type - filter control */
    UINT8_TO_BE_STREAM (p, BNEP_FRAME_CONTROL);

    /* Put in filter message type - set filters */
    UINT8_TO_BE_STREAM (p, BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG);

    UINT16_TO_BE_STREAM (p, response_code);

    p_buf->len = 4;

    bnepu_check_send_packet (p_bcb, p_buf);
}


/*******************************************************************************
**
** Function         bnep_sec_check_complete
**
** Description      This function is registered with BTM and will be called
**                  after completing the security procedures
**
** Returns          void
**
*******************************************************************************/
void bnep_sec_check_complete (BD_ADDR bd_addr, BOOLEAN trasnport, void *p_ref_data,
                              uint8_t result)
{
    tBNEP_CONN      *p_bcb = (tBNEP_CONN *)p_ref_data;
    uint16_t          resp_code = BNEP_SETUP_CONN_OK;
    BOOLEAN         is_role_change;

    WICED_BT_TRACE ("BNEP security callback returned result %d\n", result);
    if (p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)
        is_role_change = TRUE;
    else
        is_role_change = FALSE;

    /* check if the port is still waiting for security to complete */
    if (p_bcb->con_state != BNEP_STATE_SEC_CHECKING)
    {
        WICED_BT_TRACE ("BNEP Conn in wrong state %d when security is completed\n",
                           p_bcb->con_state);
        return;
    }

    /* if it is outgoing call and result is FAILURE return security fail error */
    if (!(p_bcb->con_flags & BNEP_FLAGS_SETUP_RCVD))
    {
        if (result != WICED_BT_SUCCESS)
        {
            if (p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)
            {
                /* Tell the user that role change is failed because of security */
                if (bnep_cb.p_conn_state_cb)
                    (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda,
                                                BNEP_SECURITY_FAIL, is_role_change);

                p_bcb->con_state = BNEP_STATE_CONNECTED;
                memcpy ((uint8_t *)&(p_bcb->src_uuid), (uint8_t *)&(p_bcb->prv_src_uuid),
                        sizeof (tBT_UUID));
                memcpy ((uint8_t *)&(p_bcb->dst_uuid), (uint8_t *)&(p_bcb->prv_dst_uuid),
                        sizeof (tBT_UUID));
                return;
            }
            wiced_bt_l2cap_disconnect_req(p_bcb->l2cap_cid);
            /* Tell the user if he has a callback */
            if (bnep_cb.p_conn_state_cb)
                (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda,
                                            BNEP_SECURITY_FAIL, is_role_change);

            bnepu_release_bcb (p_bcb);
            return;
        }

        /* Transition to the next appropriate state, waiting for connection confirm. */
        p_bcb->con_state = BNEP_STATE_CONN_SETUP;

        bnep_send_conn_req (p_bcb);
        wiced_start_timer(&p_bcb->conn_tle, BNEP_CONN_TIMEOUT);
        return;
    }

    /* it is an incoming call respond appropriately */
    if (result != WICED_BT_SUCCESS)
    {
        bnep_send_conn_responce (p_bcb, BNEP_SETUP_CONN_NOT_ALLOWED);
        if (p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)
        {
            /* Role change is failed because of security. Revert back to connected state */
            p_bcb->con_state = BNEP_STATE_CONNECTED;
            p_bcb->con_flags &= (~BNEP_FLAGS_SETUP_RCVD);
            memcpy ((uint8_t *)&(p_bcb->src_uuid), (uint8_t *)&(p_bcb->prv_src_uuid),
                    sizeof (tBT_UUID));
            memcpy ((uint8_t *)&(p_bcb->dst_uuid), (uint8_t *)&(p_bcb->prv_dst_uuid),
                    sizeof (tBT_UUID));
            return;
        }

        wiced_bt_l2cap_disconnect_req(p_bcb->l2cap_cid);
        bnepu_release_bcb (p_bcb);
        return;
    }

    if (bnep_cb.p_conn_ind_cb)
    {
        p_bcb->con_state = BNEP_STATE_CONN_SETUP;
        (*bnep_cb.p_conn_ind_cb) (p_bcb->handle, p_bcb->rem_bda, &p_bcb->dst_uuid,
                                  &p_bcb->src_uuid, is_role_change);
    }
    else
    {
        /* Profile didn't register connection indication call back */
        bnep_send_conn_responce (p_bcb, resp_code);
        bnep_connected (p_bcb);
    }

    return;
}


/*******************************************************************************
**
** Function         bnep_is_packet_allowed
**
** Description      This function verifies whether the protocol passes through
**                  the protocol filters set by the peer
**
** Returns          BNEP_SUCCESS          - if the protocol is allowed
**                  BNEP_IGNORE_CMD       - if the protocol is filtered out
**
*******************************************************************************/
tBNEP_RESULT bnep_is_packet_allowed (tBNEP_CONN *p_bcb,
                                     BD_ADDR p_dest_addr,
                                     uint16_t protocol,
                                     BOOLEAN fw_ext_present,
                                     uint8_t *p_data)
{
#if (defined (BNEP_SUPPORTS_PROT_FILTERS) && BNEP_SUPPORTS_PROT_FILTERS == TRUE)
    if (p_bcb->rcvd_num_filters)
    {
        uint16_t          i, proto;

        /* Findout the actual protocol to check for the filtering */
        proto = protocol;
        if (proto == BNEP_802_1_P_PROTOCOL)
        {
            if (fw_ext_present)
            {
                uint8_t       len, ext;
                /* parse the extension headers and findout actual protocol */
                do {

                    ext     = *p_data++;
                    len     = *p_data++;
                    p_data += len;

                } while (ext & 0x80);
            }
            p_data += 2;
            BE_STREAM_TO_UINT16 (proto, p_data);
        }

        for (i=0; i<p_bcb->rcvd_num_filters; i++)
        {
            if ((p_bcb->rcvd_prot_filter_start[i] <= proto) &&
                (proto <= p_bcb->rcvd_prot_filter_end[i]))
                break;
        }

        if (i == p_bcb->rcvd_num_filters)
        {
            WICED_BT_TRACE ("Ignoring protocol 0x%x in BNEP data write", proto);
            return BNEP_IGNORE_CMD;
        }
    }
#endif

#if (defined (BNEP_SUPPORTS_MULTI_FILTERS) && BNEP_SUPPORTS_MULTI_FILTERS == TRUE)
    /* Ckeck for multicast address filtering */
    if ((p_dest_addr[0] & 0x01) &&
        p_bcb->rcvd_mcast_filters)
    {
        uint16_t          i = 0;

        /* Check if every multicast should be filtered */
        if (p_bcb->rcvd_mcast_filters != 0xFFFF)
        {
            /* Check if the address is mentioned in the filter range */
            for (i = 0; i < p_bcb->rcvd_mcast_filters; i++)
            {
                if ((memcmp (p_bcb->rcvd_mcast_filter_start[i], p_dest_addr,
                             BD_ADDR_LEN) <= 0) &&
                    (memcmp (p_bcb->rcvd_mcast_filter_end[i], p_dest_addr,
                             BD_ADDR_LEN) >= 0))
                    break;
            }
        }

        /*
        ** If every multicast should be filtered or the address is not in the filter
        ** range drop the packet
        */
        if ((p_bcb->rcvd_mcast_filters == 0xFFFF) || (i == p_bcb->rcvd_mcast_filters))
        {
            WICED_BT_TRACE ("Ignoring multicast address %x.%x.%x.%x.%x.%x "
                               "in BNEP data write",
                               p_dest_addr[0], p_dest_addr[1], p_dest_addr[2],
                               p_dest_addr[3], p_dest_addr[4], p_dest_addr[5]);
            return BNEP_IGNORE_CMD;
        }
    }
#endif

    return BNEP_SUCCESS;
}


/*******************************************************************************
**
** Function         bnep_get_uuid32
**
** Description      This function returns the 32 bit equivalent of the given UUID
**
** Returns          UINT32          - 32 bit equivalent of the UUID
**
*******************************************************************************/
uint32_t bnep_get_uuid32 (tBT_UUID *src_uuid)
{
    uint32_t      result;

    if (src_uuid->len == 2)
        return ((uint32_t)src_uuid->uu.uuid16);
    else if (src_uuid->len == 4)
        return (src_uuid->uu.uuid32 & 0x0000FFFF);
    else
    {
        result = src_uuid->uu.uuid128[2];
        result = (result << 8) | (src_uuid->uu.uuid128[3]);
        return result;
    }
}

#endif
