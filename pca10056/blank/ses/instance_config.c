/*
 * instance_config.c
 *
 *  Created on: Oct 14, 2019
 *      Author: milad
 */

#include "instance_config.h"
#include "identity.h"
#include "instance.h"
#include "port.h"

instance_info_t instance_info;

dwt_config_t radio_config = {
    5,               /* Channel number. */
    DWT_PLEN_1024,    /* Preamble length. Used in TX only. */
    DWT_PAC8,        /* Preamble acquisition chunk size. Used in RX only. */
    9,               /* TX preamble code. Used in TX only. */
    10,               /* RX preamble code. Used in RX only. */
    1,               /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,      /* Data rate. */
    DWT_PHRMODE_EXT, /* PHY header mode. */
    DWT_PHRRATE_DTA, /* PHY header rate. */
    (1024 + 8 - 8),   /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    DWT_STS_MODE_OFF,
    DWT_STS_LEN_64,  /* STS length, see allowed values in Enum dwt_sts_lengths_e */
    DWT_PDOA_M0      /* PDOA mode off */
};

dwt_config_t TS_config = {
    5,               /* Channel number. */
    DWT_PLEN_1024,    /* Preamble length. Used in TX only. */
    DWT_PAC8,        /* Preamble acquisition chunk size. Used in RX only. */
    9,               /* TX preamble code. Used in TX only. */
    9,               /* RX preamble code. Used in RX only. */
    1,               /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,      /* Data rate. */
    DWT_PHRMODE_EXT, /* PHY header mode. */
    DWT_PHRRATE_DTA, /* PHY header rate. */
    (1024 + 8 - 8),   /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    DWT_STS_MODE_OFF,
    DWT_STS_LEN_64,  /* STS length, see allowed values in Enum dwt_sts_lengths_e */
    DWT_PDOA_M0      /* PDOA mode off */
};

dwt_txconfig_t txconfig =
{
    0x34,           /* PG delay. */
    0xfdfdfdfd,      /* TX power. */
    0x0             /*PG count*/
};


packet_std_t ack_pkt;


void instance_config_identity_init() {
  uint32_t dev_id = NRF_FICR->DEVICEADDR[0];
  switch(dev_id){
    case 0xf5330684:
      identity_set_role(ROLE_TS);
      radio_config.txCode = 9;
      identity_set_address(0x0000);
    break;
    case 0xf965bd2e:
      identity_set_role(ROLE_TX);
      radio_config.txCode = 10;
      radio_config.rxCode = 9;
      identity_set_address(0x0001);
    break;
    case 0x29ec415d:
      identity_set_role(ROLE_TX);
      radio_config.txCode = 9;
      radio_config.rxCode = 9;
      identity_set_address(0x0002);
    break;
  }
  configure_node();
}

void identity_set_role(Role role) {
	identity_set_operations(IDENTITY_OPERATIONS_NONE);
	switch (role) {
	case ROLE_TX:
		identity_append_operations(IDENTITY_OPERATIONS_DATA_TX);
		identity_append_operations(IDENTITY_OPERATIONS_CONSTANT_TX);

		break;
	case ROLE_RX:
		identity_append_operations(IDENTITY_OPERATIONS_DATA_RX);
		break;

	case ROLE_TS:
		identity_append_operations(IDENTITY_OPERATIONS_TIMESYNC);
		//instance_info.events.ts_tx.enabled = 1;
		break;

	}
}

void config_tx() {
  instance_info.config.radio_config = radio_config;
  instance_info.config.packet_size = 100;
  instance_info.config.tx_number = TX_PKT_CNT;
  instance_info.config.tx_config = txconfig;
  instance_info.config.sequence_number = 0;
  instance_info.config.IPI_wait = DELAY_TX_IPI;
  instance_info.config.initial_wait = DELAY_TX;
  instance_info.events.tx_enable = 0;
}

void configure_node() {
  if(dwt_configure(&radio_config)) {
    while (1)
    { };
  }
  dwt_configuretxrf(&txconfig);
  if ((identity_get_operations() & (IDENTITY_OPERATIONS_DATA_TX | IDENTITY_OPERATIONS_DATA_RX) )) {
    dwt_rxenable(DWT_START_RX_IMMEDIATE);
    config_tx();
  }
}

