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
#include "l2cdefs.h"
#include "hcidefs.h"
#include "l2cdefs.h"
#include "bnep_api.h"
#include "bnep_int.h"
#include "wiced_bt_l2c.h"
#include "wiced_bt_trace.h"
#include "wiced_memory.h"

/********************************************************************************/
/*                       G L O B A L    B N E P       D A T A                   */
/********************************************************************************/
#if BNEP_DYNAMIC_MEMORY == FALSE
tBNEP_CB   bnep_cb;
#endif

const uint16_t bnep_frame_hdr_sizes[] = {14, 1, 2, 8, 8};

/********************************************************************************/
/*              L O C A L    F U N C T I O N     P R O T O T Y P E S            */
/********************************************************************************/
static void bnep_connected_cb(BD_ADDR bd_addr, uint16_t lcid, uint16_t peer_mtu);
static void bnep_disconnect_ind (uint16_t l2cap_cid, uint16_t reason, wiced_bool_t ack_needed);
static void bnep_disconnect_cfm (uint16_t l2cap_cid, uint16_t result);
static void bnep_data_ind (uint16_t l2cap_cid, tDRB *p_drb);
static void bnep_congestion_ind (uint16_t l2cap_cid, BOOLEAN is_congested);
static void bnep_l2c_br_tx_complete (uint16_t lcid, void *p_data);

extern void   wiced_bt_release_dtcb (tDTCB *p_dtcb);

/*******************************************************************************
**
** Function         bnep_register_with_l2cap
**
** Description      This function registers BNEP PSM with L2CAP
**
** Returns          void
**
*******************************************************************************/
tBNEP_RESULT bnep_register_with_l2cap (void)
{
    WICED_BT_TRACE ("bnep_register_with_l2cap\n");
    memset(&bnep_cb.reg_info, 0, sizeof(bnep_cb.reg_info));

    bnep_cb.reg_info.connected_cback         = bnep_connected_cb;
    bnep_cb.reg_info.disconnect_indication_cback     = bnep_disconnect_ind;
    bnep_cb.reg_info.disconnect_confirm_cback     = bnep_disconnect_cfm;
    bnep_cb.reg_info.data_indication_cback           = bnep_data_ind;
    //bnep_cb.reg_info.congestion_status_cback  = bnep_congestion_ind;
    bnep_cb.reg_info.tx_complete_cback = (wiced_bt_l2cap_tx_complete_cback_t *)bnep_l2c_br_tx_complete;
    bnep_cb.reg_info.mtu = BNEP_MTU_SIZE;
    if (!wiced_bt_l2cap_register (BT_PSM_BNEP, &bnep_cb.reg_info))
    {
        WICED_BT_TRACE ("BNEP - Registration failed");
        return BNEP_SECURITY_FAIL;
    }

    return BNEP_SUCCESS;
}


static void bnep_connected_cb(BD_ADDR bd_addr, uint16_t l2cap_cid, uint16_t peer_mtu)
{
    tBNEP_CONN    *p_bcb;

    if ((p_bcb = bnepu_find_bcb_by_cid (l2cap_cid)) == NULL)
    {
        //PAN-NAP
        if ((p_bcb = bnepu_find_bcb_by_bd_addr (bd_addr)) == NULL)
        {
            if (!(bnep_cb.profile_registered) || ((p_bcb = bnepu_allocate_bcb(bd_addr)) == NULL))
            {
                WICED_BT_TRACE ("bnep_connected_cb, unknown CID: 0x%x", l2cap_cid);
                return;
            }

            p_bcb->con_state = BNEP_STATE_SEC_CHECKING;
            p_bcb->l2cap_cid = l2cap_cid;
            p_bcb->con_flags |= BNEP_FLAGS_HIS_CFG_DONE;
            p_bcb->con_flags |= BNEP_FLAGS_MY_CFG_DONE;

            WICED_BT_TRACE ("bnep_connected_cb, l2cap_cid: 0x%x", l2cap_cid);
            return;
        }
    }
    else
    {
        //PANU
        WICED_BT_TRACE ("p_bcb->con_state = %d, p_bcb->con_flags = 0x%x\n", p_bcb->con_state, p_bcb->con_flags);

        if (p_bcb->con_state == BNEP_STATE_CONN_START)
        {
            p_bcb->con_state = BNEP_STATE_SEC_CHECKING;

            if (p_bcb->con_flags & BNEP_FLAGS_IS_ORIG)
            {
                bnep_sec_check_complete(bd_addr, FALSE, p_bcb, WICED_BT_SUCCESS);
            }
        }
    }
}


/*******************************************************************************
**
** Function         bnep_disconnect_ind
**
** Description      This function handles a disconnect event from L2CAP. If
**                  requested to, we ack the disconnect before dropping the CCB
**
** Returns          void
**
*******************************************************************************/
static void bnep_disconnect_ind (uint16_t l2cap_cid, uint16_t reason, wiced_bool_t ack_needed)
{
    tBNEP_CONN    *p_bcb;

    WICED_BT_TRACE ("bnep_disconnect_ind ack_needed = %d\n", ack_needed);

    if (ack_needed)
        wiced_bt_l2cap_disconnect_rsp(l2cap_cid);

    /* Find CCB based on CID */
    if ((p_bcb = bnepu_find_bcb_by_cid (l2cap_cid)) == NULL)
    {
        WICED_BT_TRACE ("BNEP - Rcvd L2CAP disc, unknown CID: 0x%x\n", l2cap_cid);
        return;
    }

    WICED_BT_TRACE ("BNEP - Rcvd L2CAP disc, CID: 0x%x\n", l2cap_cid);

    /* Tell the user if he has a callback */
    if (p_bcb->con_state == BNEP_STATE_CONNECTED)
    {
        if (bnep_cb.p_conn_state_cb)
            (*bnep_cb.p_conn_state_cb)(p_bcb->handle, p_bcb->rem_bda, BNEP_CONN_DISCONNECTED, FALSE);
    }
    else
    {
        if (((p_bcb->con_flags & BNEP_FLAGS_IS_ORIG) && (bnep_cb.p_conn_state_cb)) ||
            ((p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED) && (bnep_cb.p_conn_state_cb)))
            (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda, BNEP_CONN_FAILED, FALSE);
    }

    bnepu_release_bcb (p_bcb);
}



/*******************************************************************************
**
** Function         bnep_disconnect_cfm
**
** Description      This function gets the disconnect confirm event from L2CAP
**
** Returns          void
**
*******************************************************************************/
static void bnep_disconnect_cfm (uint16_t l2cap_cid, uint16_t result)
{
    WICED_BT_TRACE ("BNEP - Rcvd L2CAP disc cfm, CID: 0x%x, Result 0x%x", l2cap_cid, result);
}

/*******************************************************************************
**
** Function         bnep_l2c_br_tx_complete
**
** Description      This is the L2CAP TX Complete callback function.
**
** Returns          void
**
*******************************************************************************/
static void bnep_l2c_br_tx_complete (uint16_t lcid, void *p_data)
{
    tBNEP_CONN    *p_bcb;
    int             i;
    uint8_t offset = 0x1B;

    WICED_BT_TRACE ("bnep_l2c_br_tx_complete: 0x%x, 0x%x", lcid, (uint8_t *)p_data);

    /* look up lcb for this channel */
    if ((p_bcb = bnepu_find_bcb_by_cid (lcid)) != NULL)
    {
        BT_HDR *p_buf = (BT_HDR *)wiced_bt_getfirst(&p_bcb->xmit_pending_q);

        if(p_bcb->xmit_pending_num > 0)
            p_bcb->xmit_pending_num--;

        WICED_BT_TRACE ("bnep_l2c_br_tx_complete: BNEP handle: %d, xmit pending num: ##############  %d\n",p_bcb->handle, p_bcb->xmit_pending_num);

        while(p_buf)
        {
            if(p_buf)
            {
                if(p_data == (uint8_t *)(p_buf + 1) + p_buf->offset)
                {
                    wiced_bt_remove_from_queue(&p_bcb->xmit_pending_q, p_buf);
                    wiced_bt_free_buffer(p_buf);
                    break;
                }
            }
            p_buf = (BT_HDR *)wiced_bt_getnext(p_buf);
        }

        if(p_bcb->xmit_pending_q.count <= BNEP_XMIT_PENDING_LOW_DEPTH)
        {
            if(p_bcb->con_flags & BNEP_FLAGS_L2CAP_CONGESTED)
            {
                p_bcb->con_flags &= ~BNEP_FLAGS_L2CAP_CONGESTED;

                if(bnep_cb.p_tx_data_flow_cb)
                {
                    bnep_cb.p_tx_data_flow_cb(p_bcb->handle, BNEP_TX_FLOW_ON);
                }

                /* While not congested, send as many buffers as we can */
                while (!(p_bcb->con_flags & BNEP_FLAGS_L2CAP_CONGESTED))
                {
                    BT_HDR   *p_buf = (BT_HDR *)wiced_bt_dequeue (&p_bcb->xmit_q);

                    if (!p_buf)
                        break;

                    wiced_bt_l2cap_data_write (lcid, (uint8_t *)(p_buf + 1) + p_buf->offset, p_buf->len, L2CAP_FLUSHABLE_PACKET);
                    wiced_bt_free_buffer(p_buf);
                }
            }
        }
    }
    else
    {
        WICED_BT_TRACE ("bnep_l2c_br_tx_complete: no BCB for lcid=0x%x", lcid);
    }


}

#if 0
/*******************************************************************************
**
** Function         bnep_congestion_ind
**
** Description      This is a callback function called by L2CAP when
**                  congestion status changes
**
*******************************************************************************/
static void bnep_congestion_ind (uint16_t l2cap_cid, BOOLEAN is_congested)
{
    tBNEP_CONN    *p_bcb;

    /* Find BCB based on CID */
    if ((p_bcb = bnepu_find_bcb_by_cid (l2cap_cid)) == NULL)
    {
        WICED_BT_TRACE ("BNEP - Rcvd L2CAP cong, unknown CID: 0x%x", l2cap_cid);
        return;
    }

    if (is_congested)
   {
        p_bcb->con_flags |= BNEP_FLAGS_L2CAP_CONGESTED;
       if(bnep_cb.p_tx_data_flow_cb)
       {
           bnep_cb.p_tx_data_flow_cb(p_bcb->handle, BNEP_TX_FLOW_OFF);
       }
   }
    else
    {
        p_bcb->con_flags &= ~BNEP_FLAGS_L2CAP_CONGESTED;

       if(bnep_cb.p_tx_data_flow_cb)
       {
           bnep_cb.p_tx_data_flow_cb(p_bcb->handle, BNEP_TX_FLOW_ON);
       }

        /* While not congested, send as many buffers as we can */
        while (!(p_bcb->con_flags & BNEP_FLAGS_L2CAP_CONGESTED))
        {
            BT_HDR   *p_buf = (BT_HDR *)GKI_dequeue (&p_bcb->xmit_q);

            if (!p_buf)
                break;

#if BT_TRACE_PROTOCOL == TRUE
            DispBnep (p_buf, FALSE);
#endif
            wiced_bt_l2cap_data_write (l2cap_cid, (uint8_t *)(p_buf + 1) + p_buf->offset, p_buf->len, L2CAP_FLUSHABLE_PACKET);
            wiced_bt_free_buffer(p_buf);
        }
    }
}
#endif


/*******************************************************************************
**
** Function         bnep_data_ind
**
** Description      This function is called when data is received from L2CAP.
**                  if we are the originator of the connection, we are the SDP
**                  client, and the received message is queued up for the client.
**
**                  If we are the destination of the connection, we are the SDP
**                  server, so the message is passed to the server processing
**                  function.
**
** Returns          void
**
*******************************************************************************/
static void bnep_data_ind (uint16_t l2cap_cid, tDRB *p_drb)
{
    tBNEP_CONN    *p_bcb;
    uint16_t       buf_len = p_drb->drb_data_len;
    uint8_t        *p_data = &p_drb->drb_data[p_drb->drb_data_offset];
    uint8_t         *p = (uint8_t *)p_data;
    uint16_t        rem_len = buf_len;
    uint8_t         type, ctrl_type, ext_type = 0;
    BOOLEAN       extension_present, fw_ext_present;
    uint16_t        protocol = 0;
    uint8_t         *p_src_addr, *p_dst_addr;


    /* Find CCB based on CID */
    if ((p_bcb = bnepu_find_bcb_by_cid (l2cap_cid)) == NULL)
    {
        WICED_BT_TRACE ("BNEP - Rcvd L2CAP data, unknown CID: 0x%x\n", l2cap_cid);
        return;
    }

    /* Get the type and extension bits */
    type = *p++;
    extension_present = type >> 7;
    type &= 0x7f;
    if ((rem_len <= bnep_frame_hdr_sizes[type]) || (rem_len > BNEP_MTU_SIZE))
    {
        WICED_BT_TRACE ("BNEP - rcvd frame, bad len: %d  type: 0x%02x\n", buf_len, type);
        return;
    }

    rem_len--;

    if ((p_bcb->con_state != BNEP_STATE_CONNECTED) &&
        (!(p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)) &&
        (type != BNEP_FRAME_CONTROL))
    {
        WICED_BT_TRACE ("BNEP - Ignored L2CAP data while in state: %d, CID: 0x%x\n", p_bcb->con_state, l2cap_cid);

        if (extension_present)
        {
            /*
            ** When there is no connection if a data packet is received
            ** with unknown control extension headers then those should be processed
            ** according to complain/ignore law
            */
            uint8_t       ext, length;
            uint16_t      org_len, new_len;
            /* parse the extension headers and process unknown control headers */
            org_len = rem_len;
            new_len = 0;

            do {

                ext     = *p++;
                length  = *p++;
                p += length;

                if ((!(ext & 0x7F)) && (*p > BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG))
                    bnep_send_command_not_understood (p_bcb, *p);

                new_len += (length + 2);

                if (new_len > org_len)
                    break;

            } while (ext & 0x80);
        }
        return;
    }

    if (type > BNEP_FRAME_COMPRESSED_ETHERNET_DEST_ONLY)
    {
        WICED_BT_TRACE ("BNEP - rcvd frame, unknown type: 0x%02x\n", type);
        return;
    }

    WICED_BT_TRACE ("bnep_data_ind BNEP - rcv frame, type: %d len: %d Ext: %d\n", type, buf_len, extension_present);

    /* Initialize addresses to 'not supplied' */
    p_src_addr = p_dst_addr = NULL;

    switch (type)
    {
    case BNEP_FRAME_GENERAL_ETHERNET:
        p_dst_addr = p;
        p += BD_ADDR_LEN;
        p_src_addr = p;
        p += BD_ADDR_LEN;
        BE_STREAM_TO_UINT16 (protocol, p);
        rem_len -= 14;
        break;

    case BNEP_FRAME_CONTROL:
        ctrl_type = *p;
        p = bnep_process_control_packet (p_bcb, p, &rem_len, FALSE);

        if (ctrl_type == BNEP_SETUP_CONNECTION_REQUEST_MSG &&
            p_bcb->con_state != BNEP_STATE_CONNECTED &&
            extension_present && p && rem_len)
        {
            if(p_bcb->p_pending_data) wiced_bt_free_buffer(p_bcb->p_pending_data);
            p_bcb->p_pending_data = (BT_HDR *)wiced_bt_get_buffer (rem_len + sizeof(BT_HDR));
            if (p_bcb->p_pending_data)
            {
                memcpy ((uint8_t *)(p_bcb->p_pending_data + 1), p, rem_len);
                p_bcb->p_pending_data->len    = rem_len;
                p_bcb->p_pending_data->offset = 0;
            }
        }
        else
        {
            while (extension_present && p && rem_len)
            {
                ext_type = *p++;
                extension_present = ext_type >> 7;
                ext_type &= 0x7F;

                /* if unknown extension present stop processing */
                if (ext_type)
                    break;

                p = bnep_process_control_packet (p_bcb, p, &rem_len, TRUE);
            }
        }
        return;

    case BNEP_FRAME_COMPRESSED_ETHERNET:
        BE_STREAM_TO_UINT16 (protocol, p);
        rem_len -= 2;
        break;

    case BNEP_FRAME_COMPRESSED_ETHERNET_SRC_ONLY:
        p_src_addr = p;
        p += BD_ADDR_LEN;
        BE_STREAM_TO_UINT16 (protocol, p);
        rem_len -= 8;
        break;

    case BNEP_FRAME_COMPRESSED_ETHERNET_DEST_ONLY:
        p_dst_addr = p;
        p += BD_ADDR_LEN;
        BE_STREAM_TO_UINT16 (protocol, p);
        rem_len -= 8;
        break;
    }

    /* Process the header extension if there is one */
    while (extension_present && p && rem_len)
    {
        ext_type = *p;
        extension_present = ext_type >> 7;
        ext_type &= 0x7F;

        /* if unknown extension present stop processing */
        if (ext_type)
        {
            WICED_BT_TRACE ("Data extension type 0x%x found\n", ext_type);
            break;
        }

        p++;
        rem_len--;
        p = bnep_process_control_packet (p_bcb, p, &rem_len, TRUE);
    }
    /* Always give the upper layer MAC addresses */
    if (!p_src_addr)
        p_src_addr = (uint8_t *) p_bcb->rem_bda;

    if (!p_dst_addr)
        p_dst_addr = (uint8_t *) bnep_cb.my_bda;

    /* check whether there are any extensions to be forwarded */
    if (ext_type)
        fw_ext_present = TRUE;
    else
        fw_ext_present = FALSE;

    if (bnep_cb.p_data_buf_cb)
    {
        (*bnep_cb.p_data_buf_cb)(p_bcb->handle, p_src_addr, p_dst_addr, protocol, p, rem_len, fw_ext_present);
    }
    else if (bnep_cb.p_data_ind_cb)
    {
        (*bnep_cb.p_data_ind_cb)(p_bcb->handle, p_src_addr, p_dst_addr, protocol, p, rem_len, fw_ext_present);
    }
}


/*******************************************************************************
**
** Function         bnep_process_timeout
**
** Description      This function processes a timeout. We check for reading our
**                  BD address if it failed in bnep_register(). If it is an
**                  L2CAP timeout, we send a disconnect req to L2CAP.
**
** Returns          void
**
*******************************************************************************/
void bnep_process_timeout(wiced_timer_callback_arg_t param)
{
    tBNEP_CONN *p_bcb;
    p_bcb = (tBNEP_CONN *)param;

    WICED_BT_TRACE ("BNEP - CCB timeout in state: %d  CID: 0x%x flags %x, re_transmit %d\n",
                       p_bcb->con_state, p_bcb->l2cap_cid, p_bcb->con_flags, p_bcb->re_transmits);

    if (p_bcb->con_state == BNEP_STATE_CONN_SETUP)
    {
        WICED_BT_TRACE ("BNEP - CCB timeout in state: %d  CID: 0x%x\n",
                           p_bcb->con_state, p_bcb->l2cap_cid);

        if (!(p_bcb->con_flags & BNEP_FLAGS_IS_ORIG))
        {
            wiced_bt_l2cap_disconnect_req(p_bcb->l2cap_cid);
            bnepu_release_bcb (p_bcb);
            return;
        }

        if (p_bcb->re_transmits++ != BNEP_MAX_RETRANSMITS)
        {
            bnep_send_conn_req (p_bcb);
            wiced_start_timer(&p_bcb->conn_tle, BNEP_CONN_TIMEOUT);
        }
        else
        {
            wiced_bt_l2cap_disconnect_req(p_bcb->l2cap_cid);
            if ((p_bcb->con_flags & BNEP_FLAGS_IS_ORIG) && (bnep_cb.p_conn_state_cb))
                (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda, BNEP_CONN_FAILED, FALSE);

            bnepu_release_bcb (p_bcb);
            return;
        }
    }
    else if (p_bcb->con_state != BNEP_STATE_CONNECTED)
    {
        WICED_BT_TRACE ("BNEP - CCB timeout in state: %d  CID: 0x%x\n",
                           p_bcb->con_state, p_bcb->l2cap_cid);
        wiced_bt_l2cap_disconnect_req(p_bcb->l2cap_cid);

        /* Tell the user if he has a callback */
        if ((p_bcb->con_flags & BNEP_FLAGS_IS_ORIG) && (bnep_cb.p_conn_state_cb))
            (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda, BNEP_CONN_FAILED, FALSE);

        bnepu_release_bcb (p_bcb);
    }
#if (defined (BNEP_SUPPORTS_PROT_FILTERS) && BNEP_SUPPORTS_PROT_FILTERS == TRUE)
    else if (p_bcb->con_flags & BNEP_FLAGS_FILTER_RESP_PEND)
    {
        if (p_bcb->re_transmits++ != BNEP_MAX_RETRANSMITS)
        {
            bnepu_send_peer_our_filters (p_bcb);
            wiced_start_timer(&p_bcb->conn_tle, BNEP_FILTER_SET_TIMEOUT);
        }
        else
        {
            wiced_bt_l2cap_disconnect_req(p_bcb->l2cap_cid);
            /* Tell the user if he has a callback */
            if (bnep_cb.p_conn_state_cb)
                (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda, BNEP_SET_FILTER_FAIL, FALSE);

            bnepu_release_bcb (p_bcb);
            return;
        }
    }
#endif
#if (defined (BNEP_SUPPORTS_MULTI_FILTERS) && BNEP_SUPPORTS_MULTI_FILTERS == TRUE)
    else if (p_bcb->con_flags & BNEP_FLAGS_MULTI_RESP_PEND)
    {
        if (p_bcb->re_transmits++ != BNEP_MAX_RETRANSMITS)
        {
            bnepu_send_peer_our_multi_filters (p_bcb);
            wiced_start_timer(&p_bcb->conn_tle, BNEP_FILTER_SET_TIMEOUT);
        }
        else
        {
            wiced_bt_l2cap_disconnect_req(p_bcb->l2cap_cid);
            /* Tell the user if he has a callback */
            if (bnep_cb.p_conn_state_cb)
                (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda, BNEP_SET_FILTER_FAIL, FALSE);

            bnepu_release_bcb (p_bcb);
            return;
        }
    }
#endif
}


/*******************************************************************************
**
** Function         bnep_connected
**
** Description      This function is called when a connection is established
**                  (after config).
**
** Returns          void
**
*******************************************************************************/
void bnep_connected (tBNEP_CONN *p_bcb)
{
    BOOLEAN     is_role_change;

    WICED_BT_TRACE ("bnep_connected\n");

    if (p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)
        is_role_change = TRUE;
    else
        is_role_change = FALSE;

    p_bcb->con_state = BNEP_STATE_CONNECTED;
    p_bcb->con_flags |= BNEP_FLAGS_CONN_COMPLETED;
    p_bcb->con_flags &= (~BNEP_FLAGS_SETUP_RCVD);

    wiced_stop_timer(&p_bcb->conn_tle);
    p_bcb->re_transmits = 0;
    p_bcb->xmit_pending_num = 0;

    /* Tell the upper layer, if he has a callback */
    if (bnep_cb.p_conn_state_cb)
        (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda, BNEP_SUCCESS,
                                    is_role_change);
}

#endif
