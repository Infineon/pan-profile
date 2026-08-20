/*
 * $ Copyright 2016-YEAR Cypress Semiconductor $
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
