#include "instance.h"
#include <stdio.h>
#include <stdlib.h>
#include <sdk_config.h>
#include "boards.h"
#include "port.h"
#include "deca_spi.h"
#include <math.h>
#include "deca_device_api.h"
#include "deca_regs.h"
#include "port.h"
#include "identity.h"
#include "util.h"
#include "instance_config.h"


#define ADDRESS 0xFFFF
#define SET_OUPUT_GPIOs 0xFFFF & ~(GPIO_DIR_GDP1_BIT_MASK | GPIO_DIR_GDP2_BIT_MASK | GPIO_DIR_GDP3_BIT_MASK)
#define ENABLE_ALL_GPIOS_MASK 0x200000
#define SWITCH_CONF_INDEX 10


uint32_t seq_num = 0;
uint32_t conf_switch_SN = SWITCH_CONF_INDEX;
uint32_t tx_num = 0;
packet_std_t tx_packet;
packet_std_t rx_packet;
uint32_t rx_timestamp;

void check_uart();

uint32_t swap_uint32( uint32_t num )
{
    return((num>>24)&0xff) | // move byte 3 to byte 0
          ((num<<8)&0xff0000) | // move byte 1 to byte 2
          ((num>>8)&0xff00) | // move byte 2 to byte 1
          ((num<<24)&0xff000000); // byte 0 to byte 3;
}

uint16_t swap_uint16( uint16_t num )
{
    return (num>>8) | (num<<8);
}


void init_LEDs(){
	dwt_enablegpioclocks();
	dwt_write32bitoffsetreg(GPIO_MODE_ID, 0, ENABLE_ALL_GPIOS_MASK);
	dwt_write16bitoffsetreg(GPIO_OUT_ID, 0, 0x0);
	dwt_write16bitoffsetreg(GPIO_DIR_ID, 0, SET_OUPUT_GPIOs);

}

void init_tx_packet(){
  if(identity_get_operations() & IDENTITY_OPERATIONS_TIMESYNC){
    tx_packet.packet_id = (MSG_TIME_SYNC);
    tx_packet.sequence_number = (0X00000000);
  }
  if(identity_get_operations() & IDENTITY_OPERATIONS_DATA_TX){
    tx_packet.packet_id = (MSG_DATA);
    tx_packet.sequence_number = (0X00000000);
  }
  tx_packet.src = identity_get_address();
  tx_packet.dst = (0xFFFF);
  memset(tx_packet.payload,'.', sizeof(tx_packet.payload));
  int p = 0;
  int chunk = 100;
  while (p < sizeof(tx_packet.payload)){
    chunk = MIN(100, sizeof(tx_packet.payload) - p);
    dwt_writetxdata(chunk, (uint8_t *) &tx_packet + p, p);
    //printf("chunk: %d\n", chunk);
    p += chunk;
  }
  memset(tx_packet.payload,'p', sizeof(tx_packet.payload));
  dwt_writetxdata(sizeof(tx_packet), (uint8_t *) &tx_packet, 0);
  dwt_writetxfctrl(sizeof(tx_packet), 0, 0);

}

void gpio_set(uint16_t port){
	dwt_or16bitoffsetreg(GPIO_OUT_ID, 0, (port));
}

void gpio_reset(uint16_t port){
	dwt_and16bitoffsetreg(GPIO_OUT_ID, 0, (uint16_t)(~(port)));
}


void init_NRF(){
  bsp_board_init(BSP_INIT_BUTTONS);
  /* Initialise the SPI for nRF52840-DK */
  nrf52840_dk_spi_init();
  /* Configuring interrupt*/
  dw_irq_init();
  /* Small pause before startup */
  nrf_delay_ms(2);
}






uint8_t set_radio_params(){
  //uint32_t rx_timestamp = dwt_readsystimestamphi32();
  uint32_t rx_timestamp = 10;
  uint32_t wait_time = 0;
  if (identity_get_operations() & IDENTITY_OPERATIONS_DATA_RX){
    rx_timestamp = dwt_readrxtimestamphi32();
    if (rx_packet.packet_id == MSG_TIME_SYNC)
      //wait_time = instance_info.config.initial_wait;
      wait_time = 0;
    else
      wait_time = 0;

  }
  if (identity_get_operations() & IDENTITY_OPERATIONS_DATA_TX){
    if (tx_num == 0){
      rx_timestamp = dwt_readrxtimestamphi32();
      wait_time = instance_info.config.initial_wait;
    }else{
      rx_timestamp = dwt_readtxtimestamphi32();
      wait_time = instance_info.config.IPI_wait;
    }
  }
  //printf("timestamp: %d\n", rx_timestamp);
  if(wait_time != 0){
    uint32_t tx_timestamp = rx_timestamp + wait_time;
    dwt_setdelayedtrxtime(tx_timestamp);
    return DWT_START_TX_DELAYED;
  }
  return DWT_START_TX_IMMEDIATE;
}




void instance_tx(void){
  uint8_t mode;
  uint8_t txPRECode = 0;
  mode = set_radio_params();
  instance_info.events.tx_enable = 1;
  dwt_writetxdata(50, (uint8_t *) &tx_packet, 0);
  if(seq_num >= conf_switch_SN){
    conf_switch_SN += SWITCH_CONF_INDEX;
    dwt_writetxfctrl(900, 0, 0);
    dwt_configure(&TS_config);
  }else{
    seq_num++;
    dwt_writetxfctrl(900, 0, 0);
    dwt_configure(&radio_config);
  }
  tx_packet.sequence_number = (seq_num);

  if (dwt_starttx(mode) == DWT_SUCCESS ){
    //printf("Message sent\n");
  }else{
    //printf("message failed\n");
    instance_info.diagnostics.uwb.tx.failed_count ++;
  }
  
}

void instance_rx(void){
  uint8_t mode;
  mode = set_radio_params();
  if (dwt_rxenable(mode) != DWT_SUCCESS ){
          instance_info.diagnostics.uwb.rx.enable_error++;
  }
}

void process_message(void){
  if(identity_get_operations() & IDENTITY_OPERATIONS_DATA_TX){
    instance_tx();  
  }
  if(identity_get_operations() & IDENTITY_OPERATIONS_DATA_RX){
    instance_rx();
  }

}


void rx_ok_cb(const dwt_cb_data_t *cb_data){
  instance_info.diagnostics.uwb.rx.received_count++;
  //dwt_rxenable(DWT_START_RX_IMMEDIATE);
  dwt_readrxdata((uint8_t *) &rx_packet, 100, 0);
  process_message();
}




void sfd_ok_cb(const dwt_cb_data_t *cb_data){
  //process_message();
}


/*! ------------------------------------------------------------------------------------------------------------------
 * @fn rx_to_cb()
 *
 * @brief Callback to process RX timeout events
 *
 * @param  cb_data  callback data
 *
 * @return  none
 */
void rx_to_cb(const dwt_cb_data_t *cb_data){
	//instance_info.diagnostics.uwb.rx.to_cb_count++;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn rx_err_cb()
 *
 * @brief Callback to process RX error events
 *
 * @param  cb_data  callback data
 *
 * @return  none
 */
void rx_err_cb(const dwt_cb_data_t *cb_data){
  //process_message();
  instance_info.diagnostics.uwb.rx.err_cb_count++;
  dwt_rxenable(DWT_START_RX_IMMEDIATE);
  //uint32_t rx_timestamp = dwt_readsystimestamphi32();
  //uint32_t wait_time = DELAY_TX_100US(2000);
  //uint32_t tx_timestamp = rx_timestamp + wait_time;
  //dwt_setdelayedtrxtime(tx_timestamp);
  //if (dwt_rxenable(DWT_START_RX_DELAYED) != DWT_SUCCESS ){
  //        //instance_info.diagnostics.uwb.tx.failed_count ++;
  //}
}


void tx_conf_cb(const dwt_cb_data_t *cb_data){
  //printf("tx done");
  //instance_info.diagnostics.uwb.tx.sent_count++;
  if (identity_get_operations() & IDENTITY_OPERATIONS_DATA_TX){
    tx_num ++;
    if (tx_num < instance_info.config.tx_number && instance_info.events.tx_enable){
        instance_tx();
    }else{
      char* msg = "TX_DONE";
      
      //send_UART_msg((uint8_t*)msg, sizeof(msg));
      dwt_configure(&TS_config);
      dwt_rxenable(DWT_START_RX_IMMEDIATE);
      tx_num = 0;
    }
  }  
}

void instance_init(){
  // NRF start
  init_NRF();
  // DW3000 start
  port_set_dw_ic_spi_fastrate();

  /* Reset DW IC */
  reset_DWIC(); /* Target specific drive of RSTn line into DW IC low for a period. */

  Sleep(2); // Time needed for DW3000 to start up (transition from INIT_RC to IDLE_RC, or could wait for SPIRDY event)

  while (!dwt_checkidlerc()) /* Need to make sure DW IC is in IDLE_RC before proceeding */
  { };

  if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
  {
      while (1)
      { };
  }
  //dwt_configuretxrf(&txconfig);
  instance_config_identity_init();
  dwt_setcallbacks(&tx_conf_cb, &rx_ok_cb, &rx_to_cb, &rx_err_cb, NULL, NULL);
  ali_setcallbacks(&sfd_ok_cb);
  

  dwt_setinterrupt(
    SYS_ENABLE_LO_RXPRD_ENABLE_BIT_MASK|
    SYS_ENABLE_LO_RXSFDD_ENABLE_BIT_MASK|
    SYS_ENABLE_LO_RXPHE_ENABLE_BIT_MASK|
    SYS_ENABLE_LO_RXFCE_ENABLE_BIT_MASK|
    SYS_ENABLE_LO_RXFSL_ENABLE_BIT_MASK|
    SYS_ENABLE_LO_RXFCG_ENABLE_BIT_MASK|
    SYS_ENABLE_LO_TXFRB_ENABLE_BIT_MASK|
    SYS_ENABLE_LO_TXPRS_ENABLE_BIT_MASK|
    SYS_ENABLE_LO_TXPHS_ENABLE_BIT_MASK|
    SYS_ENABLE_LO_TXFRS_ENABLE_BIT_MASK
    , 0, DWT_ENABLE_INT);
	    //dwt_setinterrupt(SYS_ENABLE_LO_TXFRS_ENABLE_BIT_MASK | SYS_ENABLE_LO_RXFCG_ENABLE_BIT_MASK | SYS_ENABLE_LO_RXFTO_ENABLE_BIT_MASK  |
	    //        SYS_ENABLE_LO_RXPTO_ENABLE_BIT_MASK | SYS_ENABLE_LO_RXPHE_ENABLE_BIT_MASK | SYS_ENABLE_LO_RXFCE_ENABLE_BIT_MASK |
	    //        SYS_ENABLE_LO_RXFSL_ENABLE_BIT_MASK | SYS_ENABLE_LO_RXSTO_ENABLE_BIT_MASK, 0, DWT_ENABLE_INT);

  
  /* Configure the TX spectrum parameters (power PG delay and PG Count) */
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RCINIT_BIT_MASK | SYS_STATUS_SPIRDY_BIT_MASK);
  init_LEDs();
  port_set_dwic_isr(ali_isr);
  //port_set_dwic_isr(dwt_isr);
  init_tx_packet();
}



uint8_t cnt = 0;
void instance_loop(){
  
  if (identity_get_operations() & IDENTITY_OPERATIONS_TIMESYNC){
    dwt_writetxdata(30, (uint8_t *) &tx_packet, 0);
    dwt_writetxfctrl(120, 0, 0);
    uint32_t ret = dwt_starttx(DWT_START_TX_IMMEDIATE);
    tx_packet.sequence_number++;
    nrf_delay_ms(TX_DELAY_MS);
  }
  if(identity_get_operations() & IDENTITY_OPERATIONS_DATA_TX){
    check_uart();
  }
}

void instance_uart_handler(uint8_t* command, uint16_t len){
  instance_info.events.tx_enable = 0;

}

void check_uart(){
  uint8_t input[5] = {0,0,0,0,0};
  uint8_t resp = 'a';
  uint32_t ret = 0;
  int cnt = 0;
  ret = app_uart_get(&input[0]);
  while (ret != NRF_ERROR_NOT_FOUND){
    cnt++; 
    ret = app_uart_get(&input[cnt]);
  }
  if(cnt != 0){
    instance_uart_handler(input, cnt);
    app_uart_put(resp);
  }

}