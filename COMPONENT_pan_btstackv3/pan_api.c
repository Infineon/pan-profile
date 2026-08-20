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

#include "bt_types.h"
#include "bnep_api.h"
#include "pan_api.h"
#include "pan_int.h"
#include "wiced_bt_sdp_defs.h"
#include "wiced_bt_l2c.h"
#include "hcidefs.h"
#include "wiced_bt_trace.h"
#include "wiced_memory.h"

/*******************************************************************************
**
** Function         wiced_bt_pan_register
**
** Description      This function is called by the application to register
**                  its callbacks with PAN profile. The application then
**                  should set the PAN role explicitly.
**
** Parameters:      p_register - contains all callback function pointers
**
**
** Returns          none
**
*******************************************************************************/
void wiced_bt_pan_register(tPAN_REGISTER *p_register)
{
    pan_register_with_bnep ();

    if (!p_register)
        return;

    pan_cb.pan_conn_state_cb    = p_register->pan_conn_state_cb;
    pan_cb.pan_bridge_req_cb    = p_register->pan_bridge_req_cb;
    pan_cb.pan_data_buf_ind_cb  = p_register->pan_data_buf_ind_cb;
    pan_cb.pan_data_ind_cb      = p_register->pan_data_ind_cb;
    pan_cb.pan_pfilt_ind_cb     = p_register->pan_pfilt_ind_cb;
    pan_cb.pan_mfilt_ind_cb     = p_register->pan_mfilt_ind_cb;
    pan_cb.pan_tx_data_flow_cb  = p_register->pan_tx_data_flow_cb;

    return;
}



/*******************************************************************************
**
** Function         wiced_bt_pan_deregister
**
** Description      This function is called by the application to de-register
**                  its callbacks with PAN profile. This will make the PAN to
**                  become inactive. This will deregister PAN services from SDP
**                  and close all active connections
**
** Parameters:      none
**
**
** Returns          none
**
*******************************************************************************/
void wiced_bt_pan_deregister(void)
{
    pan_cb.pan_bridge_req_cb    = NULL;
    pan_cb.pan_data_buf_ind_cb  = NULL;
    pan_cb.pan_data_ind_cb      = NULL;
    pan_cb.pan_conn_state_cb    = NULL;
    pan_cb.pan_pfilt_ind_cb     = NULL;
    pan_cb.pan_mfilt_ind_cb     = NULL;

    wiced_bt_pan_setrole (PAN_ROLE_INACTIVE);
    bnep_deregister ();

    return;
}


/*******************************************************************************
**
** Function         wiced_bt_pan_setrole
**
** Description      This function is called by the application to set the PAN
**                  profile role. This should be called after wiced_bt_pan_register.
**                  This can be called any time to change the PAN role
**
** Parameters:      role        - is bit map of roles to be active
**                                      PAN_ROLE_CLIENT is for PANU role
**                                      PAN_ROLE_GN_SERVER is for GN role
**                                      PAN_ROLE_NAP_SERVER is for NAP role
**                  sec_mask    - Security mask for different roles
**                                      It is array of UINT8. The byte represent the
**                                      security for roles PANU, GN and NAP in order
**                  p_user_name - Service name for PANU role
**                  p_gn_name   - Service name for GN role
**                  p_nap_name  - Service name for NAP role
**                                      Can be NULL if user wants it to be default
**
** Returns          PAN_SUCCESS     - if the role is set successfully
**                  PAN_FAILURE     - if the role is not valid
**
*******************************************************************************/
tPAN_RESULT wiced_bt_pan_setrole (uint8_t role)
{
    /* If the role is not a valid combination reject it */
    if ((!(role & (PAN_ROLE_CLIENT | PAN_ROLE_GN_SERVER | PAN_ROLE_NAP_SERVER))) &&
        role != PAN_ROLE_INACTIVE)
    {
        WICED_BT_TRACE ("PAN role %d is invalid", role);
        return PAN_FAILURE;
    }

    /* If the current active role is same as the role being set do nothing */
    if (pan_cb.role == role)
    {
        WICED_BT_TRACE ("PAN role already was set to: %d", role);
        return PAN_SUCCESS;
    }

    /* Register all the roles with SDP */
    WICED_BT_TRACE ("wiced_bt_pan_setrole() called with role 0x%x\n", role);
    /* Check if it is a shutdown request */
    if (role == PAN_ROLE_INACTIVE)
        pan_close_all_connections ();

    pan_cb.role = role;
    WICED_BT_TRACE ("PAN role set to: %d\n", role);
    return PAN_SUCCESS;
}



/*******************************************************************************
**
** Function         wiced_bt_pan_connect
**
** Description      This function is called by the application to initiate a
**                  connection to the remote device
**
** Parameters:      rem_bda     - BD Addr of the remote device
**                  src_role    - Role of the local device for the connection
**                  dst_role    - Role of the remote device for the connection
**                                      PAN_ROLE_CLIENT is for PANU role
**                                      PAN_ROLE_GN_SERVER is for GN role
**                                      PAN_ROLE_NAP_SERVER is for NAP role
**                  *handle     - Pointer for returning Handle to the connection
**
** Returns          PAN_SUCCESS      - if the connection is initiated successfully
**                  PAN_NO_RESOURCES - resources are not sufficent
**                  PAN_FAILURE      - if the connection cannot be initiated
**                                           this can be because of the combination of
**                                           src and dst roles may not be valid or
**                                           allowed at that point of time
**
*******************************************************************************/
tPAN_RESULT wiced_bt_pan_connect(BD_ADDR rem_bda, uint8_t src_role, uint8_t dst_role, uint16_t *handle)
{
    tPAN_CONN       *pcb;
    tBNEP_RESULT    result;
    tBT_UUID        src_uuid, dst_uuid;

    /*
    ** Initialize the handle so that in case of failure return values
    ** the profile will not get confused
    */
    *handle = BNEP_INVALID_HANDLE;

    /* Check if PAN is active or not */
    if (!(pan_cb.role & src_role))
    {
        WICED_BT_TRACE ("PAN is not active for the role %d\n", src_role);
        return PAN_FAILURE;
    }

    /* Validate the parameters before proceeding */
    if ((src_role != PAN_ROLE_CLIENT && src_role != PAN_ROLE_GN_SERVER
         && src_role != PAN_ROLE_NAP_SERVER) ||
        (dst_role != PAN_ROLE_CLIENT && dst_role != PAN_ROLE_GN_SERVER
         && dst_role != PAN_ROLE_NAP_SERVER))
    {
        WICED_BT_TRACE ("Either source %d or destination role %d is invalid\n",
                          src_role, dst_role);
        return PAN_FAILURE;
    }

    /* Check if connection exists for this remote device */
    pcb = pan_get_pcb_by_addr (rem_bda);

    /* If we are PANU for this role validate destination role */
    if (src_role == PAN_ROLE_CLIENT)
    {
        if ((pan_cb.num_conns > 1) || (pan_cb.num_conns && (!pcb)))
        {
            /*
            ** If the request is not for existing connection reject it
            ** because if there is already a connection we cannot accept
            ** another connection in PANU role
            */
            WICED_BT_TRACE ("Can't make PANU conns when more than one connection\n");
            return PAN_INVALID_SRC_ROLE;
        }

        src_uuid.uu.uuid16 = UUID_SERVCLASS_PANU;
        if (dst_role == PAN_ROLE_CLIENT)
        {  /* BSA_SPECIFIC */
            dst_uuid.uu.uuid16 = UUID_SERVCLASS_PANU;
        }
        else if (dst_role == PAN_ROLE_GN_SERVER)
        {
            dst_uuid.uu.uuid16 = UUID_SERVCLASS_GN;
        }
        else
        {
            dst_uuid.uu.uuid16 = UUID_SERVCLASS_NAP;
        }
    }
    /* If destination is PANU role validate source role */
    else if (dst_role == PAN_ROLE_CLIENT)
    {
        if (pan_cb.num_conns && pan_cb.active_role == PAN_ROLE_CLIENT && !pcb)
        {
            WICED_BT_TRACE ("Device already have a connection in PANU role\n");
            return PAN_INVALID_SRC_ROLE;
        }

        dst_uuid.uu.uuid16 = UUID_SERVCLASS_PANU;
        if (src_role == PAN_ROLE_GN_SERVER)
        {
            src_uuid.uu.uuid16 = UUID_SERVCLASS_GN;
        }
        else
        {
            src_uuid.uu.uuid16 = UUID_SERVCLASS_NAP;
        }
    }
    /* The role combination is not valid */
    else
    {
        WICED_BT_TRACE("Source %d and Destination roles %d are not valid combination\n",
            src_role, dst_role);
        return PAN_FAILURE;
    }

    /* Allocate control block and initiate connection */
    if (!pcb)
        pcb = pan_allocate_pcb (rem_bda, BNEP_INVALID_HANDLE);
    if (!pcb)
    {
        WICED_BT_TRACE ("PAN Connection failed because of no resources");
        return PAN_NO_RESOURCES;
    }
    WICED_BT_TRACE ("wiced_bt_pan_connect() for BD Addr %x.%x.%x.%x.%x.%x\n",
        rem_bda[0], rem_bda[1], rem_bda[2], rem_bda[3], rem_bda[4], rem_bda[5]);
    if (pcb->con_state == PAN_STATE_IDLE)
    {
        pan_cb.num_conns++;
    }
    else if (pcb->con_state == PAN_STATE_CONNECTED)
    {
        pcb->con_flags |= PAN_FLAGS_CONN_COMPLETED;
    }
    else
        /* PAN connection is still in progress */
        return PAN_WRONG_STATE;

    pcb->con_state = PAN_STATE_CONN_START;
    pcb->prv_src_uuid = pcb->src_uuid;
    pcb->prv_dst_uuid = pcb->dst_uuid;

    pcb->src_uuid     = src_uuid.uu.uuid16;
    pcb->dst_uuid     = dst_uuid.uu.uuid16;

    src_uuid.len      = 2;
    dst_uuid.len      = 2;

    result = bnep_connect (rem_bda, &src_uuid, &dst_uuid, &(pcb->handle));
    if (result != BNEP_SUCCESS)
    {
        pan_release_pcb (pcb);
        return result;
    }

    WICED_BT_TRACE ("wiced_bt_pan_connect() current active role set to %d\n", src_role);
    pan_cb.prv_active_role = pan_cb.active_role;
    pan_cb.active_role = src_role;
    *handle = pcb->handle;
    return PAN_SUCCESS;
}




/*******************************************************************************
**
** Function         wiced_bt_pan_disconnect
**
** Description      This is used to disconnect the connection
**
** Parameters:      handle           - handle for the connection
**
** Returns          PAN_SUCCESS      - if the connection is closed successfully
**                  PAN_FAILURE      - if the connection is not found or
**                                           there is an error in disconnecting
**
*******************************************************************************/
tPAN_RESULT wiced_bt_pan_disconnect (uint16_t handle)
{
    tPAN_CONN       *pcb;
    tBNEP_RESULT    result;

    /* Check if the connection exists */
    pcb = pan_get_pcb_by_handle (handle);
    if(!pcb)
    {
        WICED_BT_TRACE ("PAN connection not found for the handle %d", handle);
        return PAN_FAILURE;
    }

    result = bnep_disconnect (pcb->handle);
    if (pcb->con_state == PAN_STATE_CONNECTED)
        pan_cb.num_conns--;

    if (pan_cb.pan_bridge_req_cb && pcb->src_uuid == UUID_SERVCLASS_NAP)
        (*pan_cb.pan_bridge_req_cb) (pcb->rem_bda, FALSE);

    pan_release_pcb (pcb);

    if (result != BNEP_SUCCESS)
    {
        WICED_BT_TRACE ("Error in closing PAN connection");
        return PAN_FAILURE;
    }

    WICED_BT_TRACE ("PAN connection closed");
    return PAN_SUCCESS;
}


/*******************************************************************************
**
** Function         pan_write
**
** Description      This sends data over the PAN connections. If this is called
**                  on GN or NAP side and the packet is multicast or broadcast
**                  it will be sent on all the links. Otherwise the correct link
**                  is found based on the destination address and forwarded on it
**                  If the return value is not PAN_SUCCESS the application should
**                  take care of releasing the message buffer
**
** Parameters:      handle   - handle for the connection
**                  dst      - MAC or BD Addr of the destination device
**                  src      - MAC or BD Addr of the source who sent this packet
**                  protocol - protocol of the ethernet packet like IP or ARP
**                  p_data   - pointer to the data
**                  len      - length of the data
**                  ext      - to indicate that extension headers present
**
** Returns          PAN_SUCCESS       - if the data is sent successfully
**                  PAN_FAILURE       - if the connection is not found or
**                                           there is an error in sending data
**
*******************************************************************************/
tPAN_RESULT pan_write (uint16_t handle, BD_ADDR dst, BD_ADDR src, uint16_t protocol,
                       uint8_t *p_data, uint16_t len, BOOLEAN ext)
{
    tPAN_CONN       *pcb;
    uint16_t          i;
    tBNEP_RESULT    result;

    if (pan_cb.role == PAN_ROLE_INACTIVE || (!(pan_cb.num_conns)))
    {
        WICED_BT_TRACE ("PAN is not active Data write failed");
        return PAN_FAILURE;
    }

    /* Check if it is broadcast or multicast packet */
    if (dst[0] & 0x01)
    {
        for (i=0; i<MAX_PAN_CONNS; i++)
        {
            if (pan_cb.pcb[i].con_state == PAN_STATE_CONNECTED)
                bnep_write (pan_cb.pcb[i].handle, dst, p_data, len, protocol, src, ext);
        }

        return PAN_SUCCESS;
    }

    if (pan_cb.active_role == PAN_ROLE_CLIENT)
    {
        /* Data write is on PANU connection */
        for (i=0; i<MAX_PAN_CONNS; i++)
        {
            if (pan_cb.pcb[i].con_state == PAN_STATE_CONNECTED &&
                pan_cb.pcb[i].src_uuid == UUID_SERVCLASS_PANU)
                break;
        }

        if (i == MAX_PAN_CONNS)
        {
            WICED_BT_TRACE ("PAN Don't have any user connections");
            return PAN_FAILURE;
        }

        result = bnep_write(pan_cb.pcb[i].handle, dst, p_data, len, protocol, src, ext);
        if (result == BNEP_IGNORE_CMD)
        {
            WICED_BT_TRACE ("PAN ignored data for PANU connection");
            return result;
        }
        else if (result != BNEP_SUCCESS)
        {
            WICED_BT_TRACE ("PAN failed to write data for the PANU connection");
            return result;
        }

        WICED_BT_TRACE ("PAN successfully wrote data for the PANU connection");
        return PAN_SUCCESS;
    }

    pcb = pan_get_pcb_by_handle (handle);
    if (!pcb)
    {
        WICED_BT_TRACE ("PAN Data write for wrong addr");
        return PAN_FAILURE;
    }

    if (pcb->con_state != PAN_STATE_CONNECTED)
    {
        WICED_BT_TRACE ("PAN Data write when conn is not active");
        return PAN_FAILURE;
    }

    result = bnep_write (pcb->handle, dst, p_data, len, protocol, src, ext);
    if (result == BNEP_IGNORE_CMD)
    {
        WICED_BT_TRACE ("PAN ignored data write to PANU");
        return result;
    }
    else if (result != BNEP_SUCCESS)
    {
        WICED_BT_TRACE ("PAN failed to send data to the PANU");
        return result;
    }

    WICED_BT_TRACE ("PAN successfully sent data to the PANU");
    return PAN_SUCCESS;
}


/*******************************************************************************
**
** Function         wiced_bt_pan_writebuf
**
** Description      This sends data over the PAN connections. If this is called
**                  on GN or NAP side and the packet is multicast or broadcast
**                  it will be sent on all the links. Otherwise the correct link
**                  is found based on the destination address and forwarded on it
**                  If the return value is not PAN_SUCCESS the application should
**                  take care of releasing the message buffer
**
** Parameters:      handle   - handle for the connection
**                  dst      - MAC or BD Addr of the destination device
**                  src      - MAC or BD Addr of the source who sent this packet
**                  protocol - protocol of the ethernet packet like IP or ARP
**                  p_buf    - pointer to the data buffer
**                  ext      - to indicate that extension headers present
**
** Returns          PAN_SUCCESS       - if the data is sent successfully
**                  PAN_FAILURE       - if the connection is not found or
**                                           there is an error in sending data
**
*******************************************************************************/
tPAN_RESULT wiced_bt_pan_writebuf (uint16_t handle, BD_ADDR dst, BD_ADDR src, uint16_t protocol,
                          BT_HDR *p_buf, BOOLEAN ext)
{
    tPAN_CONN       *pcb;
    uint16_t          i;
    tBNEP_RESULT    result;

    /* Check if it is broadcast or multicast packet */
    if (dst[0] & 0x01)
    {
        uint8_t       *p_data;
        uint16_t      len;

        p_data  = (uint8_t *)(p_buf + 1) + p_buf->offset;
        len     = p_buf->len;
        pan_write (handle, dst, src, protocol, p_data, len, ext);
        wiced_bt_free_buffer (p_buf);
        return PAN_SUCCESS;
    }

    if (pan_cb.role == PAN_ROLE_INACTIVE || (!(pan_cb.num_conns)))
    {
        WICED_BT_TRACE ("PAN is not active Data write failed");
        wiced_bt_free_buffer (p_buf);
        return PAN_FAILURE;
    }

    /* Check if the data write is on PANU side */
    if (pan_cb.active_role == PAN_ROLE_CLIENT)
    {
        /* Data write is on PANU connection */
        for (i=0; i<MAX_PAN_CONNS; i++)
        {
            if (pan_cb.pcb[i].con_state == PAN_STATE_CONNECTED &&
                pan_cb.pcb[i].src_uuid == UUID_SERVCLASS_PANU)
                break;
        }

        if (i == MAX_PAN_CONNS)
        {
            WICED_BT_TRACE ("PAN Don't have any user connections");
            wiced_bt_free_buffer (p_buf);
            return PAN_FAILURE;
        }

        result = bnep_writebuf (pan_cb.pcb[i].handle, dst, p_buf, protocol, src, ext);
        if (result == BNEP_IGNORE_CMD)
        {
            WICED_BT_TRACE ("PAN ignored data write for PANU connection");
            return result;
        }
        else if (result != BNEP_SUCCESS)
        {
            WICED_BT_TRACE ("PAN failed to write data for the PANU connection");
            return result;
        }

        WICED_BT_TRACE ("PAN successfully wrote data for the PANU connection");
        return PAN_SUCCESS;
    }

    /* findout to which connection the data is meant for */
    pcb = pan_get_pcb_by_handle (handle);
    if (!pcb)
    {
        WICED_BT_TRACE ("PAN Buf write for wrong handle");
        wiced_bt_free_buffer (p_buf);
        return PAN_FAILURE;
    }

    if (pcb->con_state != PAN_STATE_CONNECTED)
    {
        WICED_BT_TRACE ("PAN Buf write when conn is not active");
        wiced_bt_free_buffer (p_buf);
        return PAN_FAILURE;
    }

    result = bnep_writebuf (pcb->handle, dst, p_buf, protocol, src, ext);
    if (result == BNEP_IGNORE_CMD)
    {
        WICED_BT_TRACE ("PAN ignored data buf write to PANU");
        return result;
    }
    else if (result != BNEP_SUCCESS)
    {
        WICED_BT_TRACE ("PAN failed to send data buf to the PANU");
        return result;
    }

    WICED_BT_TRACE ("PAN successfully sent data buf to the PANU");
    return PAN_SUCCESS;
}


/*******************************************************************************
**
** Function         wiced_bt_pan_set_protocol_filters
**
** Description      This function is used to set protocol filters on the peer
**
** Parameters:      handle      - handle for the connection
**                  num_filters - number of protocol filter ranges
**                  start       - array of starting protocol numbers
**                  end         - array of ending protocol numbers
**
**
** Returns          PAN_SUCCESS        if protocol filters are set successfully
**                  PAN_FAILURE        if connection not found or error in setting
**
*******************************************************************************/
tPAN_RESULT wiced_bt_pan_set_protocol_filters(uint16_t handle,
                                    uint16_t num_filters,
                                    uint16_t *p_start_array,
                                    uint16_t *p_end_array)
{
#if (defined (BNEP_SUPPORTS_PROT_FILTERS) && BNEP_SUPPORTS_PROT_FILTERS == TRUE)
    tPAN_CONN       *pcb;
    tPAN_RESULT     result;

    /* Check if the connection exists */
    pcb = pan_get_pcb_by_handle (handle);
    if(!pcb)
    {
        WICED_BT_TRACE ("PAN connection not found for the handle %d", handle);
        return PAN_FAILURE;
    }

    result = bnep_set_protocol_filters (pcb->handle, num_filters,
                                      p_start_array, p_end_array);
    if (result != BNEP_SUCCESS)
    {
        WICED_BT_TRACE ("PAN failed to set protocol filters for handle %d", handle);
        return result;
    }

    WICED_BT_TRACE ("PAN successfully sent protocol filters for handle %d", handle);
    return PAN_SUCCESS;
#else
    return PAN_FAILURE;
#endif
}



/*******************************************************************************
**
** Function         wiced_bt_pan_set_multicast_filters
**
** Description      This function is used to set multicast filters on the peer
**
** Parameters:      handle      - handle for the connection
**                  num_filters - number of multicast filter ranges
**                  start       - array of starting multicast filter addresses
**                  end         - array of ending multicast filter addresses
**
**
** Returns          PAN_SUCCESS        if multicast filters are set successfully
**                  PAN_FAILURE        if connection not found or error in setting
**
*******************************************************************************/
tBNEP_RESULT wiced_bt_pan_set_multicast_filters(uint16_t handle,
                                                              uint16_t num_mcast_filters,
                                                              uint8_t *p_start_array,
                                                              uint8_t *p_end_array)
{
#if (defined (BNEP_SUPPORTS_MULTI_FILTERS) && BNEP_SUPPORTS_MULTI_FILTERS == TRUE)
    tPAN_CONN       *pcb;
    tPAN_RESULT     result;

    /* Check if the connection exists */
    pcb = pan_get_pcb_by_handle (handle);
    if(!pcb)
    {
        WICED_BT_TRACE ("PAN connection not found for the handle %d", handle);
        return PAN_FAILURE;
    }

    result = bnep_set_multicast_filters (pcb->handle,
                            num_mcast_filters, p_start_array, p_end_array);
    if (result != BNEP_SUCCESS)
    {
        WICED_BT_TRACE ("PAN failed to set multicast filters for handle %d", handle);
        return result;
    }

    WICED_BT_TRACE ("PAN successfully sent multicast filters for handle %d", handle);
    return PAN_SUCCESS;
#else
    return PAN_FAILURE;
#endif
}


/*******************************************************************************
**
** Function         pan_set_trace_level
**
** Description      This function sets the trace level for PAN. If called with
**                  a value of 0xFF, it simply reads the current trace level.
**
** Returns          the new (current) trace level
**
*******************************************************************************/
uint8_t pan_set_trace_level (uint8_t new_level)
{
    if (new_level != 0xFF)
        pan_cb.trace_level = new_level;

    return (pan_cb.trace_level);
}

/*******************************************************************************
**
** Function         wiced_bt_pan_init
**
** Description      This function initializes the PAN module variables
**
** Parameters:      none
**
** Returns          none
**
*******************************************************************************/
void wiced_bt_pan_init (void)
{
    memset (&pan_cb, 0, sizeof (tPAN_CB));

#if defined(PAN_INITIAL_TRACE_LEVEL)
    pan_cb.trace_level = PAN_INITIAL_TRACE_LEVEL;
#else
    pan_cb.trace_level = BT_TRACE_LEVEL_NONE;    /* No traces */
#endif
}

#endif
