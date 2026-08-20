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

#ifndef PAN_API_H
#define PAN_API_H

#if defined(WICED_APP_PANU_INCLUDED) || defined(WICED_APP_PANNAP_INCLUDED)

#include "bnep_api.h"

#define PAN_API


/*****************************************************************************
**  Constants
*****************************************************************************/

/* Define the minimum offset needed in a GKI buffer for
** sending PAN packets. Note, we are currently not sending
** extension headers, but may in the future, so allow
** space for them
*/
#define PAN_MINIMUM_OFFSET          BNEP_MINIMUM_OFFSET


/*
** The handle is passed from BNEP to PAN. The same handle is used
** between PAN and application as well
*/
#define PAN_INVALID_HANDLE          BNEP_INVALID_HANDLE

/* Bit map for PAN roles */
#define PAN_ROLE_CLIENT         0x01     /* PANU role */
#define PAN_ROLE_GN_SERVER      0x02     /* GN role */
#define PAN_ROLE_NAP_SERVER     0x04     /* NAP role */

/* Bitmap to indicate the usage of the Data */
#define PAN_DATA_TO_HOST        0x01
#define PAN_DATA_TO_LAN         0x02


/*****************************************************************************
**  Type Definitions
*****************************************************************************/

/* Define the result codes from PAN */
enum
{
    PAN_SUCCESS,                                                /* Success                           */
    PAN_DISCONNECTED            = BNEP_CONN_DISCONNECTED,       /* Connection terminated   */
    PAN_CONN_FAILED             = BNEP_CONN_FAILED,             /* Connection failed                 */
    PAN_NO_RESOURCES            = BNEP_NO_RESOURCES,            /* No resources                      */
    PAN_MTU_EXCEDED             = BNEP_MTU_EXCEDED,             /* Attempt to write long data        */
    PAN_INVALID_OFFSET          = BNEP_INVALID_OFFSET,          /* Insufficient offset in GKI buffer */
    PAN_CONN_FAILED_CFG         = BNEP_CONN_FAILED_CFG,         /* Connection failed cos of config   */
    PAN_INVALID_SRC_ROLE        = BNEP_CONN_FAILED_SRC_UUID,    /* Connection failed wrong source UUID   */
    PAN_INVALID_DST_ROLE        = BNEP_CONN_FAILED_DST_UUID,    /* Connection failed wrong destination UUID   */
    PAN_CONN_FAILED_UUID_SIZE   = BNEP_CONN_FAILED_UUID_SIZE,   /* Connection failed wrong size UUID   */
    PAN_Q_SIZE_EXCEEDED         = BNEP_Q_SIZE_EXCEEDED,         /* Too many buffers to dest          */
    PAN_TOO_MANY_FILTERS        = BNEP_TOO_MANY_FILTERS,        /* Too many local filters specified  */
    PAN_SET_FILTER_FAIL         = BNEP_SET_FILTER_FAIL,         /* Set Filter failed  */
    PAN_WRONG_HANDLE            = BNEP_WRONG_HANDLE,            /* Wrong handle for the connection  */
    PAN_WRONG_STATE             = BNEP_WRONG_STATE,             /* Connection is in wrong state */
    PAN_SECURITY_FAIL           = BNEP_SECURITY_FAIL,           /* Failed because of security */
    PAN_IGNORE_CMD              = BNEP_IGNORE_CMD,              /* To ignore the rcvd command */
    PAN_TX_FLOW_ON              = BNEP_TX_FLOW_ON,              /* tx data flow enabled */
    PAN_TX_FLOW_OFF	            = BNEP_TX_FLOW_OFF,             /* tx data flow disabled */
    PAN_FAILURE                                                 /* Failure                      */

};
typedef uint8_t tPAN_RESULT;


/*****************************************************************
**       Callback Function Prototypes
*****************************************************************/

/* This is call back function used to report connection status
**      to the application. The second parameter TRUE means
**      to create the bridge and FALSE means to remove it.
*/
typedef void (tPAN_CONN_STATE_CB) (uint16_t handle, BD_ADDR bd_addr, tPAN_RESULT state, BOOLEAN is_role_change,
                                        uint8_t src_role, uint8_t dst_role);


/* This is call back function used to create bridge for the
**      Connected device. The parameter "state" indicates
**      whether to create the bridge or remove it. TRUE means
**      to create the bridge and FALSE means to remove it.
*/
typedef void (tPAN_BRIDGE_REQ_CB) (BD_ADDR bd_addr, BOOLEAN state);


/* Data received indication callback prototype. Parameters are
**              Source BD/Ethernet Address
**              Dest BD/Ethernet address
**              Protocol
**              Address of buffer (or data if non-GKI)
**              Length of data (non-GKI)
**              ext is flag to indicate whether it has aby extension headers
**              Flag used to indicate to forward on LAN
**                      FALSE - Use it for internal stack
**                      TRUE  - Send it across the ethernet as well
*/
typedef void (tPAN_DATA_IND_CB) (uint16_t handle,
                                 BD_ADDR src,
                                 BD_ADDR dst,
                                 uint16_t protocol,
                                 uint8_t *p_data,
                                 uint16_t len,
                                 BOOLEAN ext,
                                 BOOLEAN forward);


/* Data buffer received indication callback prototype. Parameters are
**              Source BD/Ethernet Address
**              Dest BD/Ethernet address
**              Protocol
**              pointer to the data buffer
**              ext is flag to indicate whether it has aby extension headers
**              Flag used to indicate to forward on LAN
**                      FALSE - Use it for internal stack
**                      TRUE  - Send it across the ethernet as well
*/
typedef void (tPAN_DATA_BUF_IND_CB) (uint16_t handle,
                                     BD_ADDR src,
                                     BD_ADDR dst,
                                     uint16_t protocol,
                                     uint8_t *data_buf,
                                     uint16_t data_len,
                                     BOOLEAN ext,
                                     BOOLEAN forward);

/* Flow control callback for TX data. Parameters are
**              Handle to the connection
**              Event  flow status
*/
typedef void (tPAN_TX_DATA_FLOW_CB) (uint16_t handle,
                                     tPAN_RESULT  event);

/* Filters received indication callback prototype. Parameters are
**              Handle to the connection
**              TRUE if the cb is called for indication
**              Ignore this if it is indication, otherwise it is the result
**                      for the filter set operation performed by the local
**                      device
**              Number of protocol filters present
**              Pointer to the filters start. Filters are present in pairs
**                      of start of the range and end of the range.
**                      They will be present in big endian order. First
**                      two bytes will be starting of the first range and
**                      next two bytes will be ending of the range.
*/
typedef void (tPAN_FILTER_IND_CB) (uint16_t handle,
                                   BOOLEAN indication,
                                   tBNEP_RESULT result,
                                   uint16_t num_filters,
                                   uint8_t *p_filters);



/* Multicast Filters received indication callback prototype. Parameters are
**              Handle to the connection
**              TRUE if the cb is called for indication
**              Ignore this if it is indication, otherwise it is the result
**                      for the filter set operation performed by the local
**                      device
**              Number of multicast filters present
**              Pointer to the filters start. Filters are present in pairs
**                      of start of the range and end of the range.
**                      First six bytes will be starting of the first range and
**                      next six bytes will be ending of the range.
*/
typedef void (tPAN_MFILTER_IND_CB) (uint16_t handle,
                                    BOOLEAN indication,
                                    tBNEP_RESULT result,
                                    uint16_t num_mfilters,
                                    uint8_t *p_mfilters);




/* This structure is used to register with PAN profile
** It is passed as a parameter to wiced_bt_pan_register call.
*/
typedef struct
{
    tPAN_CONN_STATE_CB          *pan_conn_state_cb;     /* Connection state callback */
    tPAN_BRIDGE_REQ_CB          *pan_bridge_req_cb;     /* Bridge request callback */
    tPAN_DATA_IND_CB            *pan_data_ind_cb;       /* Data indication callback */
    tPAN_DATA_BUF_IND_CB        *pan_data_buf_ind_cb;   /* Data buffer indication callback */
    tPAN_FILTER_IND_CB          *pan_pfilt_ind_cb;      /* protocol filter indication callback */
    tPAN_MFILTER_IND_CB         *pan_mfilt_ind_cb;      /* multicast filter indication callback */
    tPAN_TX_DATA_FLOW_CB        *pan_tx_data_flow_cb;   /* data flow callback */
    char                        *user_service_name;     /* Service name for PANU role */
    char                        *gn_service_name;       /* Service name for GN role */
    char                        *nap_service_name;      /* Service name for NAP role */

} tPAN_REGISTER;


/*****************************************************************************
**  External Function Declarations
*****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif

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
PAN_API extern tPAN_RESULT wiced_bt_pan_setrole(uint8_t role);

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
PAN_API extern tPAN_RESULT wiced_bt_pan_connect (BD_ADDR rem_bda, uint8_t src_role, uint8_t dst_role, uint16_t *handle);

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
PAN_API extern tPAN_RESULT wiced_bt_pan_disconnect(uint16_t handle);

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
** Parameters:      dst      - MAC or BD Addr of the destination device
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
PAN_API extern tPAN_RESULT pan_write (uint16_t handle,
                                     BD_ADDR dst,
                                     BD_ADDR src,
                                     uint16_t protocol,
                                     uint8_t *p_data,
                                     uint16_t len,
                                     BOOLEAN ext);

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
PAN_API extern uint8_t pan_set_trace_level (uint8_t new_level);


#ifdef __cplusplus
}
#endif

#endif

#endif  /* PAN_API_H */
