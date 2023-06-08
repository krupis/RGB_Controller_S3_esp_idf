#include "UART0.h"

// hello
static const char* TAG = "UART0";
static const char* TEST_TAG = "TEST";






void UART0_setup() {
    uart_config_t uart_config = {
        .baud_rate           = 115200,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
        .source_clk          = UART_SCLK_DEFAULT,
        //.rx_flow_ctrl_thresh = 122,
        //.use_ref_tick        = false,
    };

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    // Set UART pins(TX: IO4, RX: IO5, RTS: IO18, CTS: IO19)
    //ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 13, 26, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, UART0_COMMAND_LINE_MAX_SIZE, 0, 0, NULL, 0));
}



void UART0_task(void *argument)
{
	UART0_setup();
  	unsigned int char_received=EOF;
  	unsigned int char_counter=0;
  	char command_line[UART0_COMMAND_LINE_MAX_SIZE];
  	for (;;)
	{	
		int len = uart_read_bytes(UART_NUM_0, command_line, (UART0_COMMAND_LINE_MAX_SIZE - 1), 20 / portTICK_PERIOD_MS);
        if (len) {
            command_line[len] = 0;
			ParseSystemCmd(command_line, len); // Line is complete. Execute it!
			memset(&command_line, 0, sizeof(command_line));
        }
		vTaskDelay(10/portTICK_PERIOD_MS);

  }

}








// this semaphore will ensure that the SPI (external eeprom) cannot be interrupted via another task

//esp_log_level_set("*",ESP_LOG_WARN);
bool ParseSystemCmd(char *line, uint16_t cmd_size)
{

    if (!strncmp("ping", line,4))
    {	
            printf("pong\n");
            return true;
    }

    if (!strncmp("red:", line,4))
    {	
        if (!isdigit( (unsigned char)(line[4]) ))
		{
			ESP_LOGW(TAG,"Entered input is not a number");
			return 0;
		}
		char temp_buf[4];
		uint16_t data_to_write=0;
		for(int i = 0;i <=(strlen(line)-4);i++){
			temp_buf[i] = line[4+i];
		}
		data_to_write = atoi(temp_buf);
        RGB_set_red(data_to_write);
        return true;
    }

        if (!strncmp("green:", line,6))
    {	
        if (!isdigit( (unsigned char)(line[6]) ))
		{
			ESP_LOGW(TAG,"Entered input is not a number");
			return 0;
		}
		char temp_buf[4];
		uint16_t data_to_write=0;
		for(int i = 0;i <=(strlen(line)-6);i++){
			temp_buf[i] = line[6+i];
		}
		data_to_write = atoi(temp_buf);
        RGB_set_green(data_to_write);
        return true;
    }

        if (!strncmp("blue:", line,5))
    {	
        if (!isdigit( (unsigned char)(line[5]) ))
		{
			ESP_LOGW(TAG,"Entered input is not a number");
			return 0;
		}
		char temp_buf[4];
		uint16_t data_to_write=0;
		for(int i = 0;i <=(strlen(line)-5);i++){
			temp_buf[i] = line[5+i];
		}
		data_to_write = atoi(temp_buf);
        RGB_set_blue(data_to_write);
        return true;
    }

    return 0;

}
