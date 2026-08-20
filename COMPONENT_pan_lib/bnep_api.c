/*
 * $ Copyright 2016-YEAR Cypress Semiconductor $
 */

#if defined(WICED_APP_PANU_INCLUDED) || defined(WICED_APP_PANNAP_INCLUDED)

#include "bnep_api.h"
#include "bnep_int.h"
#include "wiced_bt_trace.h"
#include "wiced_memory.h"

/*******************************************************************************
**
** Function         wiced_bt_bnep_init
**
** Description      This function initializes the BNEP unit. It should be called
**                  before accessing any other APIs to initialize the control block
**
** Returns          void
**
*******************************************************************************/
void wiced_bt_bnep_init(void)
{
    WICED_BT_TRACE("wiced_bt_bnep_init\n");

    memset (&bnep_cb, 0, sizeof (tBNEP_CB));

#if defined(BNEP_INITIAL_TRACE_LEVEL)
    bnep_cb.trace_level = BNEP_INITIAL_TRACE_LEVEL;
#else
    bnep_cb.trace_level = BT_TRACE_LEVEL_NONE;    /* No traces */
#endif
}

/*******************************************************************************
**
** Function         bnep_register
**
** Description      This function is called by the upper layer to register
**                  its callbacks with BNEP
**
** Parameters:      p_reg_info - contains all callback function pointers
**
**
** Returns          BNEP_SUCCESS        if registered successfully
**                  BNEP_FAILURE        if connection state callback is missing
**
*******************************************************************************/
tBNEP_RESULT bnep_register(tBNEP_REGISTER *p_reg_info)
{

    WICED_BT_TRACE("BNEP_Register  \n");

    /* There should be connection state call back registered */
    if ((!p_reg_info) || (!(p_reg_info->p_conn_state_cb)))
        return BNEP_SECURITY_FAIL;

    bnep_cb.p_conn_ind_cb       = p_reg_info->p_conn_ind_cb;
    bnep_cb.p_conn_state_cb     = p_reg_info->p_conn_state_cb;
    bnep_cb.p_data_ind_cb       = p_reg_info->p_data_ind_cb;
    bnep_cb.p_data_buf_cb       = p_reg_info->p_data_buf_cb;
    bnep_cb.p_filter_ind_cb     = p_reg_info->p_filter_ind_cb;
    bnep_cb.p_mfilter_ind_cb    = p_reg_info->p_mfilter_ind_cb;
    bnep_cb.p_tx_data_flow_cb   = p_reg_info->p_tx_data_flow_cb;

    if (bnep_register_with_l2cap ())
        return BNEP_SECURITY_FAIL;

    bnep_cb.profile_registered  = TRUE;

    /* Read our Bd address */
    if (!bnep_cb.got_my_bd_addr)
    {
        wiced_bt_dev_read_local_addr(bnep_cb.my_bda);

        if ((bnep_cb.my_bda[0] | bnep_cb.my_bda[1] | bnep_cb.my_bda[2] |
            bnep_cb.my_bda[3] | bnep_cb.my_bda[4] | bnep_cb.my_bda[5]) != 0)
        {
            bnep_cb.got_my_bd_addr = TRUE;
            WICED_BT_TRACE ("wiced_bt_dev_read_local_addr success\n");
        }
        else
        {
            WICED_BT_TRACE ("wiced_bt_dev_read_local_addr: Not possible to read the Bd Address now\n");
        }
    }

    return BNEP_SUCCESS;
}


/*******************************************************************************
**
** Function         bnep_deregister
**
** Description      This function is called by the upper layer to de-register
**                  its callbacks.
**
** Parameters:      void
**
**
** Returns          void
**
*******************************************************************************/
void bnep_deregister (void)
{
    WICED_BT_TRACE("bnep_deregister  \n");

    /* Clear all the call backs registered */
    bnep_cb.p_conn_ind_cb       = NULL;
    bnep_cb.p_conn_state_cb     = NULL;
    bnep_cb.p_data_ind_cb       = NULL;
    bnep_cb.p_data_buf_cb       = NULL;
    bnep_cb.p_filter_ind_cb     = NULL;
    bnep_cb.p_mfilter_ind_cb    = NULL;

    bnep_cb.profile_registered  = FALSE;
    wiced_bt_l2cap_deregister(BT_PSM_BNEP);
}


/*******************************************************************************
**
** Function         bnep_connect
**
** Description      This function creates a BNEP connection to a remote
**                  device.
**
** Parameters:      p_rem_addr  - BD_ADDR of the peer
**                  src_uuid    - source uuid for the connection
**                  dst_uuid    - destination uuid for the connection
**                  p_handle    - pointer to return the handle for the connection
**
** Returns          BNEP_SUCCESS                if connection started
**                  BNEP_NO_RESOURCES           if no resources
**
*******************************************************************************/
tBNEP_RESULT bnep_connect (BD_ADDR p_rem_bda,
                           tBT_UUID *src_uuid,
                           tBT_UUID *dst_uuid,
                           uint16_t *p_handle)
{
    uint16_t          cid;
    tBNEP_CONN      *p_bcb = bnepu_find_bcb_by_bd_addr (p_rem_bda);

    WICED_BT_TRACE ("bnep_connect()  BDA: %02x-%02x-%02x-%02x-%02x-%02x\n",
                     p_rem_bda[0], p_rem_bda[1], p_rem_bda[2],
                     p_rem_bda[3], p_rem_bda[4], p_rem_bda[5]);

    if (!bnep_cb.profile_registered)
        return BNEP_WRONG_STATE;

    /* Both source and destination UUID lengths should be same */
    if (src_uuid->len != dst_uuid->len)
        return BNEP_CONN_FAILED_UUID_SIZE;

#if (!defined (BNEP_SUPPORTS_ALL_UUID_LENGTHS) || BNEP_SUPPORTS_ALL_UUID_LENGTHS == FALSE)
    if (src_uuid->len != 2)
        return BNEP_CONN_FAILED_UUID_SIZE;
#endif

    if (!p_bcb)
    {
        if ((p_bcb = bnepu_allocate_bcb (p_rem_bda)) == NULL)
            return (BNEP_NO_RESOURCES);
    }
    else if (p_bcb->con_state != BNEP_STATE_CONNECTED)
            return BNEP_WRONG_STATE;
    else
    {
        /* Backup current UUID values to restore if role change fails */
        memcpy ((uint8_t *)&(p_bcb->prv_src_uuid), (uint8_t *)&(p_bcb->src_uuid), sizeof (tBT_UUID));
        memcpy ((uint8_t *)&(p_bcb->prv_dst_uuid), (uint8_t *)&(p_bcb->dst_uuid), sizeof (tBT_UUID));
    }

    /* We are the originator of this connection */
    p_bcb->con_flags |= BNEP_FLAGS_IS_ORIG;

    memcpy ((uint8_t *)&(p_bcb->src_uuid), (uint8_t *)src_uuid, sizeof (tBT_UUID));
    memcpy ((uint8_t *)&(p_bcb->dst_uuid), (uint8_t *)dst_uuid, sizeof (tBT_UUID));

    if (p_bcb->con_state == BNEP_STATE_CONNECTED)
    {
        /* Transition to the next appropriate state, waiting for connection confirm. */
        p_bcb->con_state = BNEP_STATE_SEC_CHECKING;

        WICED_BT_TRACE ("BNEP initiating security procedures for src uuid 0x%x\n",
            p_bcb->src_uuid.uu.uuid16);

        bnep_sec_check_complete (p_bcb->rem_bda, FALSE, p_bcb, WICED_BT_SUCCESS);
    }
    else
    {
        /* Transition to the next appropriate state, waiting for connection confirm. */
        p_bcb->con_state = BNEP_STATE_CONN_START;

        if ((cid = wiced_bt_l2cap_connect_req (BT_PSM_BNEP, p_bcb->rem_bda, NULL)) != 0)
        {
            p_bcb->l2cap_cid = cid;

        }
        else
        {
            WICED_BT_TRACE ("BNEP - Originate failed\n");
            if (bnep_cb.p_conn_state_cb)
                (*bnep_cb.p_conn_state_cb) (p_bcb->handle, p_bcb->rem_bda, BNEP_CONN_FAILED, FALSE);
            bnepu_release_bcb (p_bcb);
            return BNEP_CONN_FAILED;
        }

        /* Start timer waiting for connect */
        wiced_start_timer(&p_bcb->conn_tle, BNEP_CONN_TIMEOUT);
    }

    *p_handle = p_bcb->handle;
    return (BNEP_SUCCESS);
}


/*******************************************************************************
**
** Function         bnep_connect_resp
**
** Description      This function is called in responce to connection indication
**
**
** Parameters:      handle  - handle given in the connection indication
**                  resp    - responce for the connection indication
**
** Returns          BNEP_SUCCESS                if connection started
**                  BNEP_WRONG_HANDLE           if the connection is not found
**                  BNEP_WRONG_STATE            if the responce is not expected
**
*******************************************************************************/
tBNEP_RESULT bnep_connect_resp (uint16_t handle, tBNEP_RESULT resp)
{
    tBNEP_CONN      *p_bcb;
    uint16_t          resp_code = BNEP_SETUP_CONN_OK;

    if ((!handle) || (handle > BNEP_MAX_CONNECTIONS))
        return (BNEP_WRONG_HANDLE);

    p_bcb = &(bnep_cb.bcb[handle - 1]);

    if (p_bcb->con_state != BNEP_STATE_CONN_SETUP ||
        (!(p_bcb->con_flags & BNEP_FLAGS_SETUP_RCVD)))
        return (BNEP_WRONG_STATE);

    WICED_BT_TRACE ("bnep_connect_resp()  for handle %d, responce %d", handle, resp);

    /* Form appropriate responce based on profile responce */
    if      (resp == BNEP_CONN_FAILED_SRC_UUID)   resp_code = BNEP_SETUP_INVALID_SRC_UUID;
    else if (resp == BNEP_CONN_FAILED_DST_UUID)   resp_code = BNEP_SETUP_INVALID_DEST_UUID;
    else if (resp == BNEP_CONN_FAILED_UUID_SIZE)  resp_code = BNEP_SETUP_INVALID_UUID_SIZE;
    else if (resp == BNEP_SUCCESS)                resp_code = BNEP_SETUP_CONN_OK;
    else                                          resp_code = BNEP_SETUP_CONN_NOT_ALLOWED;

    bnep_send_conn_responce (p_bcb, resp_code);
    p_bcb->con_flags &= (~BNEP_FLAGS_SETUP_RCVD);

    if (resp == BNEP_SUCCESS)
        bnep_connected (p_bcb);
    else if (p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)
    {
        /* Restore the original parameters */
        p_bcb->con_state = BNEP_STATE_CONNECTED;
        p_bcb->con_flags &= (~BNEP_FLAGS_SETUP_RCVD);

        memcpy ((uint8_t *)&(p_bcb->src_uuid), (uint8_t *)&(p_bcb->prv_src_uuid), sizeof (tBT_UUID));
        memcpy ((uint8_t *)&(p_bcb->dst_uuid), (uint8_t *)&(p_bcb->prv_dst_uuid), sizeof (tBT_UUID));
    }

    /* Process remaining part of the setup message (extension headers) */
    if (p_bcb->p_pending_data)
    {
        uint8_t   extension_present = TRUE, *p, ext_type;
        uint16_t  rem_len;

        rem_len = p_bcb->p_pending_data->len;
        p       = (uint8_t *)(p_bcb->p_pending_data + 1) + p_bcb->p_pending_data->offset;
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
        wiced_bt_free_buffer (p_bcb->p_pending_data);
        p_bcb->p_pending_data = NULL;
    }
    return (BNEP_SUCCESS);
}


/*******************************************************************************
**
** Function         bnep_disconnect
**
** Description      This function is called to close the specified connection.
**
** Parameters:      handle   - handle of the connection
**
** Returns          BNEP_SUCCESS                if connection is disconnected
**                  BNEP_WRONG_HANDLE           if no connection is not found
**
*******************************************************************************/
tBNEP_RESULT bnep_disconnect (uint16_t handle)
{
    tBNEP_CONN      *p_bcb;

    if ((!handle) || (handle > BNEP_MAX_CONNECTIONS))
        return (BNEP_WRONG_HANDLE);

    p_bcb = &(bnep_cb.bcb[handle - 1]);

    if (p_bcb->con_state == BNEP_STATE_IDLE)
        return (BNEP_WRONG_HANDLE);

    WICED_BT_TRACE ("bnep_disconnect()  for handle %d", handle);
    wiced_bt_l2cap_disconnect_req(p_bcb->l2cap_cid);
    bnepu_release_bcb (p_bcb);

    return (BNEP_SUCCESS);
}


/*******************************************************************************
**
** Function         bnep_writebuf
**
** Description      This function sends data in a GKI buffer on BNEP connection
**
** Parameters:      handle       - handle of the connection to write
**                  p_dest_addr  - BD_ADDR/Ethernet addr of the destination
**                  p_buf        - pointer to address of buffer with data
**                  protocol     - protocol type of the packet
**                  p_src_addr   - (optional) BD_ADDR/ethernet address of the source
**                                 (should be NULL if it is local BD Addr)
**                  fw_ext_present - forwarded extensions present
**
** Returns:         BNEP_WRONG_HANDLE       - if passed handle is not valid
**                  BNEP_MTU_EXCEDED        - If the data length is greater than MTU
**                  BNEP_IGNORE_CMD         - If the packet is filtered out
**                  BNEP_Q_SIZE_EXCEEDED    - If the Tx Q is full
**                  BNEP_SUCCESS            - If written successfully
**
*******************************************************************************/
tBNEP_RESULT bnep_writebuf (uint16_t handle,
                            uint8_t *p_dest_addr,
                            BT_HDR *p_buf,
                            uint16_t protocol,
                            uint8_t *p_src_addr,
                            BOOLEAN fw_ext_present)
{
    tBNEP_CONN      *p_bcb;
    uint8_t           *p_data;

    if ((!handle) || (handle > BNEP_MAX_CONNECTIONS))
    {
        wiced_bt_free_buffer (p_buf);
        return (BNEP_WRONG_HANDLE);
    }

    WICED_BT_TRACE ("bnep_writebuf\n");

    p_bcb = &(bnep_cb.bcb[handle - 1]);
    /* Check MTU size */
    if (p_buf->len > BNEP_MTU_SIZE)
    {
        WICED_BT_TRACE ("bnep_write() length %d exceeded MTU %d", p_buf->len, BNEP_MTU_SIZE);
        wiced_bt_free_buffer (p_buf);
        return (BNEP_MTU_EXCEDED);
    }

    /* Check if the packet should be filtered out */
    p_data = (uint8_t *)(p_buf + 1) + p_buf->offset;
    if (bnep_is_packet_allowed (p_bcb, p_dest_addr, protocol, fw_ext_present, p_data) != BNEP_SUCCESS)
    {
        /*
        ** If packet is filtered and ext headers are present
        ** drop the data and forward the ext headers
        */
        if (fw_ext_present)
        {
            uint8_t       ext, length;
            uint16_t      org_len, new_len;
            /* parse the extension headers and findout the new packet len */
            org_len = p_buf->len;
            new_len = 0;
            do {

                ext     = *p_data++;
                length  = *p_data++;
                p_data += length;

                new_len += (length + 2);

                if (new_len > org_len)
                {
                    wiced_bt_free_buffer (p_buf);
                    return BNEP_IGNORE_CMD;
                }

            } while (ext & 0x80);

            if (protocol != BNEP_802_1_P_PROTOCOL)
                protocol = 0;
            else
            {
                new_len += 4;
                p_data[2] = 0;
                p_data[3] = 0;
            }
            p_buf->len  = new_len;
        }
        else
        {
            wiced_bt_free_buffer (p_buf);
            return BNEP_IGNORE_CMD;
        }
    }

    /* Check transmit queue */
    if (p_bcb->xmit_q.count >= BNEP_MAX_XMITQ_DEPTH)
    {
        wiced_bt_free_buffer (p_buf);
        return (BNEP_Q_SIZE_EXCEEDED);
    }

    /* Build the BNEP header */
    bnepu_build_bnep_hdr (p_bcb, p_buf, protocol, p_src_addr, p_dest_addr, fw_ext_present);

    /* Send the data or queue it up */
    bnepu_check_send_packet (p_bcb, p_buf);

    return (BNEP_SUCCESS);
}


/*******************************************************************************
**
** Function         bnep_write
**
** Description      This function sends data over a BNEP connection
**
** Parameters:      handle       - handle of the connection to write
**                  p_dest_addr  - BD_ADDR/Ethernet addr of the destination
**                  p_data       - pointer to data start
**                  protocol     - protocol type of the packet
**                  p_src_addr   - (optional) BD_ADDR/ethernet address of the source
**                                 (should be NULL if it is local BD Addr)
**                  fw_ext_present - forwarded extensions present
**
** Returns:         BNEP_WRONG_HANDLE       - if passed handle is not valid
**                  BNEP_MTU_EXCEDED        - If the data length is greater than MTU
**                  BNEP_IGNORE_CMD         - If the packet is filtered out
**                  BNEP_Q_SIZE_EXCEEDED    - If the Tx Q is full
**                  BNEP_NO_RESOURCES       - If not able to allocate a buffer
**                  BNEP_SUCCESS            - If written successfully
**
*******************************************************************************/
tBNEP_RESULT bnep_write (uint16_t handle,
                          uint8_t *p_dest_addr,
                          uint8_t *p_data,
                          uint16_t len,
                          uint16_t protocol,
                          uint8_t *p_src_addr,
                          BOOLEAN fw_ext_present)
{
    BT_HDR       *p_buf;
    tBNEP_CONN   *p_bcb;
    uint8_t        *p;

    WICED_BT_TRACE ("bnep_write\n");

    /* Check MTU size. Consider the possibility of having extension headers */
    if (len > BNEP_MTU_SIZE)
    {
        WICED_BT_TRACE ("bnep_write() length %d exceeded MTU %d", len, BNEP_MTU_SIZE);
        return (BNEP_MTU_EXCEDED);
    }

    if ((!handle) || (handle > BNEP_MAX_CONNECTIONS))
        return (BNEP_WRONG_HANDLE);

    p_bcb = &(bnep_cb.bcb[handle - 1]);

    /* Check if the packet should be filtered out */
    if (bnep_is_packet_allowed (p_bcb, p_dest_addr, protocol, fw_ext_present, p_data) != BNEP_SUCCESS)
    {
        /*
        ** If packet is filtered and ext headers are present
        ** drop the data and forward the ext headers
        */
        if (fw_ext_present)
        {
            uint8_t       ext, length;
            uint16_t      org_len, new_len;
            /* parse the extension headers and findout the new packet len */
            org_len = len;
            new_len = 0;
            p       = p_data;
            do {

                ext     = *p_data++;
                length  = *p_data++;
                p_data += length;

                new_len += (length + 2);

                if (new_len > org_len)
                    return BNEP_IGNORE_CMD;

            } while (ext & 0x80);

            if (protocol != BNEP_802_1_P_PROTOCOL)
                protocol = 0;
            else
            {
                new_len += 4;
                p_data[2] = 0;
                p_data[3] = 0;
            }
            len         = new_len;
            p_data      = p;
        }
        else
            return BNEP_IGNORE_CMD;
    }

    /* Check transmit queue */
    if (p_bcb->xmit_q.count >= BNEP_MAX_XMITQ_DEPTH)
        return (BNEP_Q_SIZE_EXCEEDED);

    /* Get a buffer to copy teh data into */
    if ((p_buf = (BT_HDR *)GKI_getpoolbuf (BNEP_POOL_ID)) == NULL)
    {
        WICED_BT_TRACE ("bnep_write() not able to get buffer");
        return (BNEP_NO_RESOURCES);
    }

    p_buf->len = len;
    p_buf->offset = BNEP_MINIMUM_OFFSET;
    p = (uint8_t *)(p_buf + 1) + BNEP_MINIMUM_OFFSET;

    memcpy (p, p_data, len);

    /* Build the BNEP header */
    bnepu_build_bnep_hdr (p_bcb, p_buf, protocol, p_src_addr, p_dest_addr, fw_ext_present);

    /* Send the data or queue it up */
    bnepu_check_send_packet (p_bcb, p_buf);

    return (BNEP_SUCCESS);
}


/*******************************************************************************
**
** Function         bnep_set_protocol_filters
**
** Description      This function sets the protocol filters on peer device
**
** Parameters:      handle        - Handle for the connection
**                  num_filters   - total number of filter ranges
**                  p_start_array - Array of beginings of all protocol ranges
**                  p_end_array   - Array of ends of all protocol ranges
**
** Returns          BNEP_WRONG_HANDLE           - if the connection handle is not valid
**                  BNEP_SET_FILTER_FAIL        - if the connection is in wrong state
**                  BNEP_TOO_MANY_FILTERS       - if too many filters
**                  BNEP_SUCCESS                - if request sent successfully
**
*******************************************************************************/
tBNEP_RESULT bnep_set_protocol_filters (uint16_t handle,
                                      uint16_t num_filters,
                                      uint16_t *p_start_array,
                                      uint16_t *p_end_array)
{
#if (defined (BNEP_SUPPORTS_PROT_FILTERS) && BNEP_SUPPORTS_PROT_FILTERS == TRUE)
    uint16_t          xx;
    tBNEP_CONN     *p_bcb;

    WICED_BT_TRACE ("bnep_set_protocol_filters\n");

    if ((!handle) || (handle > BNEP_MAX_CONNECTIONS))
        return (BNEP_WRONG_HANDLE);

    p_bcb = &(bnep_cb.bcb[handle - 1]);

    /* Check the connection state */
    if ((p_bcb->con_state != BNEP_STATE_CONNECTED) &&
        (!(p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)))
        return (BNEP_WRONG_STATE);

    /* Validate the parameters */
    if (num_filters && (!p_start_array || !p_end_array))
        return (BNEP_SET_FILTER_FAIL);

    if (num_filters > BNEP_MAX_PROT_FILTERS)
        return (BNEP_TOO_MANY_FILTERS);

    /* Fill the filter values in connnection block */
    for (xx = 0; xx < num_filters; xx++)
    {
        p_bcb->sent_prot_filter_start[xx] = *p_start_array++;
        p_bcb->sent_prot_filter_end[xx]   = *p_end_array++;
    }

    p_bcb->sent_num_filters = num_filters;

    bnepu_send_peer_our_filters (p_bcb);

    return (BNEP_SUCCESS);
#else
    return (BNEP_SET_FILTER_FAIL);
#endif
}


/*******************************************************************************
**
** Function         bnep_set_multicast_filters
**
** Description      This function sets the filters for multicast addresses for BNEP.
**
** Parameters:      handle        - Handle for the connection
**                  num_filters   - total number of filter ranges
**                  p_start_array - Pointer to sequence of beginings of all
**                                         multicast address ranges
**                  p_end_array   - Pointer to sequence of ends of all
**                                         multicast address ranges
**
** Returns          BNEP_WRONG_HANDLE           - if the connection handle is not valid
**                  BNEP_SET_FILTER_FAIL        - if the connection is in wrong state
**                  BNEP_TOO_MANY_FILTERS       - if too many filters
**                  BNEP_SUCCESS                - if request sent successfully
**
*******************************************************************************/
tBNEP_RESULT bnep_set_multicast_filters (uint16_t handle,
                                       uint16_t num_filters,
                                       uint8_t *p_start_array,
                                       uint8_t *p_end_array)
{
#if (defined (BNEP_SUPPORTS_MULTI_FILTERS) && BNEP_SUPPORTS_MULTI_FILTERS == TRUE)
    uint16_t          xx;
    tBNEP_CONN     *p_bcb;

    WICED_BT_TRACE ("bnep_set_multicast_filters\n");

    if ((!handle) || (handle > BNEP_MAX_CONNECTIONS))
        return (BNEP_WRONG_HANDLE);

    p_bcb = &(bnep_cb.bcb[handle - 1]);

    /* Check the connection state */
    if ((p_bcb->con_state != BNEP_STATE_CONNECTED) &&
        (!(p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)))
        return (BNEP_WRONG_STATE);

    /* Validate the parameters */
    if (num_filters && (!p_start_array || !p_end_array))
        return (BNEP_SET_FILTER_FAIL);

    if (num_filters > BNEP_MAX_MULTI_FILTERS)
        return (BNEP_TOO_MANY_FILTERS);

    /* Fill the multicast filter values in connnection block */
    for (xx = 0; xx < num_filters; xx++)
    {
        memcpy (p_bcb->sent_mcast_filter_start[xx], p_start_array, BD_ADDR_LEN);
        memcpy (p_bcb->sent_mcast_filter_end[xx], p_end_array, BD_ADDR_LEN);

        p_start_array += BD_ADDR_LEN;
        p_end_array   += BD_ADDR_LEN;
    }

    p_bcb->sent_mcast_filters = num_filters;

    bnepu_send_peer_our_multi_filters (p_bcb);

    return (BNEP_SUCCESS);
#else
    return (BNEP_SET_FILTER_FAIL);
#endif
}


/*******************************************************************************
**
** Function         bnep_get_mybdaddr
**
** Description      This function returns a pointer to the local device BD address.
**                  If the BD address has not been read yet, it returns NULL.
**
** Returns          the BD address
**
*******************************************************************************/
uint8_t *bnep_get_mybdaddr (void)
{
    if (bnep_cb.got_my_bd_addr)
        return (bnep_cb.my_bda);
    else
        return (NULL);
}

/*******************************************************************************
**
** Function         bnep_set_trace_level
**
** Description      This function sets the trace level for BNEP. If called with
**                  a value of 0xFF, it simply reads the current trace level.
**
** Returns          the new (current) trace level
**
*******************************************************************************/
uint8_t bnep_set_trace_level (uint8_t new_level)
{
    if (new_level != 0xFF)
        bnep_cb.trace_level = new_level;

    return (bnep_cb.trace_level);
}


/*******************************************************************************
**
** Function         bnep_get_status
**
** Description      This function gets the status information for BNEP connection
**
** Returns          BNEP_SUCCESS            - if the status is available
**                  BNEP_NO_RESOURCES       - if no structure is passed for output
**                  BNEP_WRONG_HANDLE       - if the handle is invalid
**                  BNEP_WRONG_STATE        - if not in connected state
**
*******************************************************************************/
tBNEP_RESULT bnep_get_status (uint16_t handle, tBNEP_STATUS *p_status)
{
#if (defined (BNEP_SUPPORTS_STATUS_API) && BNEP_SUPPORTS_STATUS_API == TRUE)
    tBNEP_CONN     *p_bcb;

    WICED_BT_TRACE("bnep_get_status\n");

    if (!p_status)
        return BNEP_NO_RESOURCES;

    if ((!handle) || (handle > BNEP_MAX_CONNECTIONS))
        return (BNEP_WRONG_HANDLE);

    p_bcb = &(bnep_cb.bcb[handle - 1]);

    memset (p_status, 0, sizeof (tBNEP_STATUS));
    if ((p_bcb->con_state != BNEP_STATE_CONNECTED) &&
        (!(p_bcb->con_flags & BNEP_FLAGS_CONN_COMPLETED)))
        return BNEP_WRONG_STATE;

    /* Read the status parameters from the connection control block */
    p_status->con_status            = BNEP_STATUS_CONNECTED;
    p_status->l2cap_cid             = p_bcb->l2cap_cid;
    p_status->rem_mtu_size          = p_bcb->rem_mtu_size;
    p_status->xmit_q_depth          = p_bcb->xmit_q.count;
    p_status->sent_num_filters      = p_bcb->sent_num_filters;
    p_status->sent_mcast_filters    = p_bcb->sent_mcast_filters;
    p_status->rcvd_num_filters      = p_bcb->rcvd_num_filters;
    p_status->rcvd_mcast_filters    = p_bcb->rcvd_mcast_filters;

    memcpy (p_status->rem_bda, p_bcb->rem_bda, BD_ADDR_LEN);
    memcpy (&(p_status->src_uuid), &(p_bcb->src_uuid), sizeof (tBT_UUID));
    memcpy (&(p_status->dst_uuid), &(p_bcb->dst_uuid), sizeof (tBT_UUID));

    return BNEP_SUCCESS;
#else
    return (BNEP_IGNORE_CMD);
#endif
}

#endif
