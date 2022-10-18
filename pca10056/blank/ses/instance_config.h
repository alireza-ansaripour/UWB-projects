#include "deca_device_api.h"
//#include "instance.h"
#include "util.h"

typedef struct
{
	struct configuration_t{
		uint16_t packet_size;
		dwt_config_t radio_config;
		dwt_txconfig_t tx_config;
		uint16_t tx_number;
		uint32_t sequence_number;
		uint32_t initial_wait;
		uint32_t IPI_wait;
	}config;
        struct events_t
	{
		uint8_t tx_enable;
                uint8_t rx_enable;
	} events;
	struct blink_t
	{
		uint8_t count;
	} blink;

	struct diagnostics_t
	{
		struct uwb_diag_t
		{
			struct uwb_diag_rx_t
			{
				uint16_t received_count;
				uint16_t enable_error;
				uint16_t packets_kept_count;
				uint16_t to_cb_count;
				uint16_t err_cb_count;
				uint16_t swing_buffer_min_free_count;
				uint16_t swing_buffer_full_count;
				uint16_t swing_buffer_wrong_fingerprint_read_count;
				uint16_t swing_buffer_wrong_fingerprint_write_count;
				uint16_t swing_buffer_oversize_write_count; // memory corruption
			} rx;

			struct uwb_diag_tx_t
			{
				uint16_t sent_count;
				uint16_t failed_count;
				uint16_t conf_cb_count;
				uint16_t swing_buffer_min_free_count;
				uint16_t swing_buffer_full_count;
				uint16_t swing_buffer_wrong_fingerprint_read_count;
				uint16_t swing_buffer_wrong_fingerprint_write_count;
				uint16_t swing_buffer_oversize_write_count; // memory corruption
			} tx;
		} uwb;

		struct usart_diag_t
		{
			struct usart_diag_tx_t
			{
				uint16_t write_count;
				uint16_t ring_buffer_max_length;
				uint16_t ring_buffer_full_count;
			} tx;
		} usart;

		struct twr_diag_t
		{
			uint16_t poll_tx_count;
			uint16_t poll_rx_count;
			uint16_t resp_tx_count;
			uint16_t resp_rx_count;
			uint16_t final_tx_count;
			uint16_t final_rx_count;
			uint16_t report_tx_count;
		} twr;

		struct blink_diag_t
		{
			uint16_t tx_count;
			uint16_t rx_count;
			uint16_t report_tx_count;
		} blink;

	} diagnostics;
} instance_info_t;

extern instance_info_t instance_info;
extern dwt_config_t radio_config;
extern dwt_config_t TS_config;

#define DELAY_TX DELAY_TX_100US(45)
#define DELAY_TX_IPI DELAY_TX_100US(45)
#define TX_PKT_CNT      5000
#define TX_DELAY_MS     1000

//#define DELAY_TX DELAY_TX_100US(0)
//#define DELAY_TX_IPI DELAY_TX_100US(0)
