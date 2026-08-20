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

#include "bnep_api.h"
#include "pan_api.h"
#include "pan_int.h"
#include "wiced_bt_l2c.h"
#include "hcidefs.h"
#include "wiced_bt_trace.h"

/*******************************************************************************
**
** Function         pan_allocate_pcb
**
** Description
**
** Returns
**
*******************************************************************************/
tPAN_CONN *pan_allocate_pcb (BD_ADDR p_bda, uint16_t handle)
{
    uint16_t      i;

    for (i=0; i<MAX_PAN_CONNS; i++)
    {
        if (pan_cb.pcb[i].con_state != PAN_STATE_IDLE &&
            pan_cb.pcb[i].handle == handle)
            return NULL;
    }

    for (i=0; i<MAX_PAN_CONNS; i++)
    {
        if (pan_cb.pcb[i].con_state != PAN_STATE_IDLE &&
            memcmp (pan_cb.pcb[i].rem_bda, p_bda, BD_ADDR_LEN) == 0)
            return NULL;
    }

    for (i=0; i<MAX_PAN_CONNS; i++)
    {
        if (pan_cb.pcb[i].con_state == PAN_STATE_IDLE)
        {
            memset (&(pan_cb.pcb[i]), 0, sizeof (tPAN_CONN));
            memcpy (pan_cb.pcb[i].rem_bda, p_bda, BD_ADDR_LEN);
            pan_cb.pcb[i].handle = handle;
            return &(pan_cb.pcb[i]);
        }
    }
    return NULL;
}


/*******************************************************************************
**
** Function         pan_get_pcb_by_handle
**
** Description
**
** Returns
**
*******************************************************************************/
tPAN_CONN *pan_get_pcb_by_handle (uint16_t handle)
{
    uint16_t      i;

    for (i=0; i<MAX_PAN_CONNS; i++)
    {
        if (pan_cb.pcb[i].con_state != PAN_STATE_IDLE &&
            pan_cb.pcb[i].handle == handle)
            return &(pan_cb.pcb[i]);
    }

    return NULL;
}


/*******************************************************************************
**
** Function         pan_get_pcb_by_addr
**
** Description
**
** Returns
**
*******************************************************************************/
tPAN_CONN *pan_get_pcb_by_addr (BD_ADDR p_bda)
{
    uint16_t      i;

    for (i=0; i<MAX_PAN_CONNS; i++)
    {
        if (pan_cb.pcb[i].con_state == PAN_STATE_IDLE)
            continue;

        if (memcmp (pan_cb.pcb[i].rem_bda, p_bda, BD_ADDR_LEN) == 0)
            return &(pan_cb.pcb[i]);

        /*
        if (pan_cb.pcb[i].mfilter_present &&
            (memcmp (p_bda, pan_cb.pcb[i].multi_cast_bridge, BD_ADDR_LEN) == 0))
            return &(pan_cb.pcb[i]);
        */
    }

    return NULL;
}




/*******************************************************************************
**
** Function         pan_close_all_connections
**
** Description
**
** Returns          void
**
*******************************************************************************/
void pan_close_all_connections (void)
{
    uint16_t      i;

    for (i=0; i<MAX_PAN_CONNS; i++)
    {
        if (pan_cb.pcb[i].con_state != PAN_STATE_IDLE)
        {
            bnep_disconnect (pan_cb.pcb[i].handle);
            pan_cb.pcb[i].con_state = PAN_STATE_IDLE;
        }
    }

    pan_cb.active_role = PAN_ROLE_INACTIVE;
    pan_cb.num_conns   = 0;
    return;
}


/*******************************************************************************
**
** Function         pan_release_pcb
**
** Description      This function releases a PCB.
**
** Returns          void
**
*******************************************************************************/
void pan_release_pcb (tPAN_CONN *p_pcb)
{
    /* Drop any response pointer we may be holding */
    memset (p_pcb, 0, sizeof (tPAN_CONN));
    p_pcb->con_state = PAN_STATE_IDLE;
}

#endif
