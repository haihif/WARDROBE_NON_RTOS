/**
 * @file main.c
 * @author Nguyen Huy Hai 20203898 (hai.nh203898@sis.hust.edu.vn)
 * @brief Main file of Smart-Wardrobe project
 * @version 0.2
 * @date 2023-03-03
 * 
 * @copyright Copyright (c) 2023
 * 
 */

/*------------------------------------ INCLUDE LIBRARY ------------------------------------ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_attr.h"
#include "esp_spi_flash.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
#include "protocol_examples_common.h"

#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "driver/spi_common.h"
#include "esp_http_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "freertos/event_groups.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "bme280.h"

#include <cJSON.h>


/*------------------------------------ DEFINE ------------------------------------ */

static const char *TAG = "SmartWardrobe";

#define SDA_PIN                                     CONFIG_SDA_PIN
#define SCL_PIN                                     CONFIG_SCL_PIN

#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

#define OPENAI_API_ENDPOINT                         CONFIG_OPENAI_API_ENDPOINT
#define OPENAI_API_KEY                              CONFIG_OPENAI_API_KEY
#define OPEN_WEATHER_API_KEY                        CONFIG_OPEN_WEATHER_API_KEY
#define YOUR_CITY                                   CONFIG_YOUR_CITY
#define YOUR_COUNTRY                                CONFIG_YOUR_COUNTRY

#define HTTP_RESPONSE_BUFFER_SIZE                   CONFIG_HTTP_RESPONSE_BUFFER_SIZE
#define HTTP_REQUEST_LENGTH                         CONFIG_HTTP_REQUEST_LENGTH
#define TEXT_RESPONSE_LENGTH                        CONFIG_TEXT_RESPONSE_LENGTH
#define USER_REQUEST_LENGTH                         CONFIG_USER_REQUEST_LENGTH
#define WEATHER_DESCRIPTION_LENGTH                  CONFIG_WEATHER_DESCRIPTION_LENGTH

// Data template structure
struct data_sensor_st
{
    double temperature ;
    double humidity ;
    double pressure ;
    double outside_temp;
    char text_response[TEXT_RESPONSE_LENGTH];
    char text_user_request[USER_REQUEST_LENGTH];
    char weather_des[WEATHER_DESCRIPTION_LENGTH];
};
struct data_sensor_st s_data_sensor_temp;

char *prompt = NULL;    
size_t prompt_len = 0;

char *p_text_generated_by_chatgpt = NULL;
size_t text_generated_by_chatgpt_length = 0;

char REQUEST[HTTP_REQUEST_LENGTH];

char open_weather_api_key[] = OPEN_WEATHER_API_KEY;

char city[] = YOUR_CITY;
char country_code[] = YOUR_COUNTRY;

char *p_text_generated_by_openweather = NULL;
size_t p_text_generated_by_openweather_length = 0;

int devide_status = 0; 

/*_______________________________________Blynk request______________________________________*/

// Change v3 status value
void change_devide_status_info_on_blynk(){
    char V3_blynk[200];
    sprintf(V3_blynk,"https://blynk.cloud/external/api/update?token={YOUR_BLYNK_AUTH_TOKEN}&v3=%d", devide_status);
    esp_http_client_config_t config_get = {  
        .url = V3_blynk,
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200)
        {
            ESP_LOGI(TAG, "Change V3 Status successfully");
        }
        else
        {
            ESP_LOGI(TAG, "Change V3 Status Failed");
        }
    }
    else
    {
        ESP_LOGI(TAG, "Change V3 Status Failed");
    }
    esp_http_client_cleanup(client);
}

// Read prompt from V6 Virtual Terminal 
esp_err_t read_devide_status_client_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        devide_status = atoi(evt->data);
         printf("%d\n",devide_status);
        break;
    case HTTP_EVENT_ON_FINISH:
        break;
    default:
        break;
    }
    
    return ESP_OK;
}

void read_devide_status()
{
    // Read V6 Terminal push to prompt
    esp_http_client_config_t config_get = {  
        .url = "https://blynk.cloud/external/api/get?token={YOUR_BLYNK_AUTH_TOKEN}&v3",
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .event_handler = read_devide_status_client_handler
    };

    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200)
        {
            ESP_LOGI(TAG, "Read terminal successfully");
        }
        else
        {
            ESP_LOGI(TAG, "Read terminal Failed");
        }
    }
    else
    {
        ESP_LOGI(TAG, "Read terminal Failed");
    }
    esp_http_client_cleanup(client);
}

/*_______________________________________Blynk Terminal_____________________________________*/ 

/*
* @brief Replace ' ' to '%20'.
 * When you send a URL in an HTTP request, you need to encode any spaces in the URL with %20 instead 
 * of using a space character. This is because spaces are not allowed in URLs and can cause errors 
 * if they are not encoded properly. This is called URL encoding and it’s used to convert special 
 * characters into a format that can be transmitted over the internet safely.
 * 
 * @param char* str : pointer to the string 
 * 
*/
void replace_spaces(char *str) {
    int spaceCount = 0, i;
    for (i = 0; str[i]; i++) {
        if (str[i] == ' ') {
            spaceCount++;
        }
    }
    int newLength = strlen(str) + 2 * spaceCount;
    char newStr[newLength];
    int j = 0;
    for (i = 0; i < strlen(str); i++) {
        if (str[i] == ' ') {
            newStr[j++] = '%';
            newStr[j++] = '2';
            newStr[j++] = '0';
        } else {
            newStr[j++] = str[i];
        }
    }
    newStr[j] = '\0';
    strcpy(str, newStr);
}

/*
* @brief Remove special characters of strings.
*This code is a function that removes special characters from a string. The function takes a pointer to a character array 
*as an argument and modifies the array in place by removing any characters that are not letters, numbers, or spaces.
*The function first initializes a pointer to the beginning of the string and then iterates over each character in the string
*using a while loop. For each character, it checks if it is one of several special characters (such as newline, tab, or backslash) 
*that should be removed. If the character is not a special character, it checks if it is a letter, number, or space and if so, 
*it copies the character to the output string and increments the output string pointer.Finally, it adds a null terminator to the end 
*of the output string to terminate it properly. 
*
* @param char* str: pointer to the string
*
* @note
*       Input string: "Hello, World!\n" 
*       Output string: "Hello World"
*/
void remove_special_characters(char *str) {
    char *ptr = str;
    while (*ptr != '\0') {
        if ((*ptr != '\n') && (*ptr != '\f') && (*ptr != '\r') && (*ptr != '\t') && (*ptr != '\v') && (*ptr != '\\') && (*ptr != '\?')) {
            if((*ptr >= 65 && *ptr <= 90) || (*ptr >= 97 && *ptr <= 122) || (*ptr == 32) || (*ptr >= 48 && *ptr <= 57))
            *str++ = *ptr;
        }
        ptr++;
    }
    *str = '\0';
}


/**
 * @brief Writes a prompt to the Blynk terminal.
 *
 * This function checks if the prompt and weather description are NULL. 
 * If they are, it sets them to "error". Otherwise, it removes special 
 * characters and replaces spaces. It then constructs a request string 
 * and sends it to the Blynk server using an HTTP GET method.
 * 
 */
void write_prompt_to_blynk_terminal()
{   
    if (prompt == NULL)
    {
        prompt = "error";
    }
    else
    {
        remove_special_characters(prompt);
        replace_spaces(prompt);
    }
    
    if (s_data_sensor_temp.weather_des == NULL)
    {
        strcpy(s_data_sensor_temp.weather_des, "error");
    }
    else
    {
        remove_special_characters(s_data_sensor_temp.weather_des);
        replace_spaces(s_data_sensor_temp.weather_des);
    }
    
    char newline[] = ".%0A";
    sprintf(REQUEST,"https://blynk.cloud/external/api/batch/update?token={YOUR_BLYNK_AUTH_TOKEN}&v6=%s%s&v5=%s%s", prompt, newline, s_data_sensor_temp.weather_des, newline);
    
    printf("\n%s\n", REQUEST);
    esp_http_client_config_t config_get = {
        .url = REQUEST,
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config_get);printf("Checked");
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    
}

/*________________________ChatGPT API - Prompting and Repsonding_________________________*/

/**
 * @brief Gets text generated by ChatGPT.
 *
 * This function parses a JSON string to extract the text generated by ChatGPT. 
 * It then copies the text into the s_data_sensor_temp.text_response variable 
 * and prints it. The root cJSON object is deleted and the 
 * p_text_generated_by_chatgpt pointer is freed and set to NULL.
 * 
 */
void get_text_generated_by_chatgpt(const char *json_string)
{
    cJSON *root = cJSON_Parse(json_string);
    cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    cJSON *choice = cJSON_GetArrayItem(choices, 0);
    cJSON *text = cJSON_GetObjectItem(choice, "text");

    strcpy(s_data_sensor_temp.text_response, text->valuestring);
    printf("%s", s_data_sensor_temp.text_response); 
    
    cJSON_Delete(root);

    free(p_text_generated_by_chatgpt);

    p_text_generated_by_chatgpt = NULL;
    text_generated_by_chatgpt_length = 0;
}

esp_err_t chatgpt_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Resize the buffer to fit the new chunk of data
            p_text_generated_by_chatgpt = realloc(p_text_generated_by_chatgpt, text_generated_by_chatgpt_length + evt->data_len);
            memcpy(p_text_generated_by_chatgpt + text_generated_by_chatgpt_length, evt->data, evt->data_len);
            text_generated_by_chatgpt_length += evt->data_len;
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG,"Text generated by ChatGPT: %s", p_text_generated_by_chatgpt);
            get_text_generated_by_chatgpt(p_text_generated_by_chatgpt);
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief Generates text using ChatGPT.
 *
 * This function sends a POST request to the OpenAI API endpoint to generate text 
 * using ChatGPT. It sets the request headers and constructs the request body using 
 * the current outside temperature and weather description. The response is handled 
 * by the chatgpt_http_event_handler function. If the request fails, an error message
 *  is printed.
 * 
 */
void chatgpt_generate_text()
{
    esp_http_client_config_t config = {
        .url = OPENAI_API_ENDPOINT,
        .method = HTTP_METHOD_POST,
        .event_handler = chatgpt_http_event_handler
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", OPENAI_API_KEY);

    char request_body[1024];

    snprintf(request_body, sizeof(request_body), "{\"prompt\":\"The temperature now is: %.2f and the weather forecast is %s. What should i wear today?\",\"max_tokens\":50, \"model\": \"text-davinci-003\"}", s_data_sensor_temp.outside_temp, s_data_sensor_temp.weather_des);
    printf("%s", request_body);
    esp_http_client_set_post_field(client, request_body, strlen(request_body));
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        printf("HTTP POST request failed: %s\n", esp_err_to_name(err));
    }

    int response_code = esp_http_client_get_status_code(client);
    if (response_code != 200) {
        printf("HTTP POST request failed: %d\n", response_code);
    }

    esp_http_client_cleanup(client);
}

/*________________________________Sending Data to Blynk___________________________________*/

esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("Data sent to Blynk: %.*s\n", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }
    return ESP_OK;
}

/**
 * @brief Sends sensor data to Blynk.
 *
 * This function constructs a request string to send temperature, humidity, and outside
 *  temperature data to the Blynk server using an HTTP GET method. The response is handled
 *  by the client_event_get_handler function.
 */
void send_data_sensor_to_blynk()
{   
    sprintf(REQUEST,"https://blynk.cloud/external/api/batch/update?token={YOUR_BLYNK_AUTH_TOKEN}&v0=%.2f&v1=%.2f&v2=%.2f",s_data_sensor_temp.temperature, 
                                                                                                    s_data_sensor_temp.humidity, s_data_sensor_temp.outside_temp);
    
    printf("%s\n", REQUEST);
    esp_http_client_config_t config_get = {
        
        .url = REQUEST,
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .event_handler = client_event_get_handler
    };
        
    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

/*_________________________________OpenWeather API ______________________________________*/

/**
 * @brief Gets weather's data from a JSON string.
 *
 * This function parses a JSON string to extract temperature and weather's description data. 
 * It converts the temperature from Kelvin to Celsius and copies the weather description 
 * into the s_data_sensor_temp.weather_des variable. The data is printed and the root cJSON 
 * object is deleted. The p_text_generated_by_openweather pointer is freed and set to NULL.
 * 
 * 
 */
void temp_outdoor_from_openweather(const char *json_string)
{
   
    cJSON *root = cJSON_Parse(json_string);
    cJSON *obj = cJSON_GetObjectItemCaseSensitive(root, "main");
    cJSON *weather = cJSON_GetObjectItemCaseSensitive(root, "weather");
    cJSON *description = cJSON_GetObjectItemCaseSensitive(weather->child, "description");

    s_data_sensor_temp.outside_temp = cJSON_GetObjectItemCaseSensitive(obj, "temp")->valuedouble;
    
    s_data_sensor_temp.outside_temp = s_data_sensor_temp.outside_temp-273.15;
    
    strcpy(s_data_sensor_temp.weather_des, description->valuestring);

    cJSON_Delete(root);
    free(p_text_generated_by_openweather);
    p_text_generated_by_openweather = NULL;
    p_text_generated_by_openweather_length = 0;
}

esp_err_t openweather_http_event_handler(esp_http_client_event_t *evt)
{
    
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Resize the buffer to fit the new chunk of data
            p_text_generated_by_openweather = realloc(p_text_generated_by_openweather, p_text_generated_by_openweather_length + evt->data_len);
            memcpy(p_text_generated_by_openweather + p_text_generated_by_openweather_length, evt->data, evt->data_len);
            p_text_generated_by_openweather_length += evt->data_len;
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI("OpenWeatherAPI", "Received data: %s", p_text_generated_by_openweather);
            temp_outdoor_from_openweather(p_text_generated_by_openweather);
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief Gets data from OpenWeather.
 *
 * This function constructs a URL to send a GET request to the OpenWeather API. The response
 * is handled by the openweather_http_event_handler function. If the request is successful 
 * and the status code is 200, a success message is logged. Otherwise, a failure message is 
 * logged.
 * 
 * 
 */
void get_data_from_openweather()
{

    char open_weather_map_url[200];
    snprintf(open_weather_map_url,
             sizeof(open_weather_map_url),
             "%s%s%s%s%s%s",
             "http://api.openweathermap.org/data/2.5/weather?q=",
             city,
             ",",
             country_code,
             "&APPID=",
             open_weather_api_key);

    esp_http_client_config_t config = {
        .url = open_weather_map_url,
        .method = HTTP_METHOD_GET,
        .event_handler = openweather_http_event_handler,
    };


    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200)
        {
            ESP_LOGI(TAG, "Message sent Successfully");
        }
        else
        {
            ESP_LOGI(TAG, "Message sent Failed");
        }
    }
    else
    {
        ESP_LOGI(TAG, "Message sent Failed");
    }
    esp_http_client_cleanup(client);
}

/*_________________________________Initialize I2C___________________________________*/

/**
 * @brief Initializes the I2C master.
 *
 * This function configures and installs the I2C driver in master mode. The SDA and SCL pins
 * are set and pull-up resistors are enabled. The clock speed is set to 400kHz.
 */
void i2c_master_init()
{
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = SDA_PIN,
		.scl_io_num = SCL_PIN,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 400000};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/*________________________________BME280 Sensor_____________________________________*/
// BME280 I2C write function
s8 bme280_i2c_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 iError = BME280_INIT_VALUE;

	esp_err_t espRc;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, reg_addr, true);
	i2c_master_write(cmd, reg_data, cnt, true);
	i2c_master_stop(cmd);

	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
	if (espRc == ESP_OK)
	{
		iError = SUCCESS;
	}
	else
	{
		iError = FAIL;
	}
	i2c_cmd_link_delete(cmd);

	return (s8)iError;
}

// BME280 I2C read function
s8 bme280_i2c_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 iError = BME280_INIT_VALUE;
	esp_err_t espRc;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, reg_addr, true);

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

	if (cnt > 1)
	{
		i2c_master_read(cmd, reg_data, cnt - 1, I2C_MASTER_ACK);
	}
	i2c_master_read_byte(cmd, reg_data + cnt - 1, I2C_MASTER_NACK);
	i2c_master_stop(cmd);

	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
	if (espRc == ESP_OK)
	{
		iError = SUCCESS;
	}
	else
	{
		iError = FAIL;
	}

	i2c_cmd_link_delete(cmd);

	return (s8)iError;
}

// BME280 I2C delay function
void bme280_delay_msek(u32 msek)
{
	vTaskDelay(msek / portTICK_PERIOD_MS);
}

// Read data from BME280 sensor 
/**
 * @brief Reads data from a BME280 sensor.
 *
 * This function initializes a BME280 sensor and sets its internal parameters. It then
 * reads uncompensated pressure, temperature, and humidity data from the sensor and 
 * uses the BME280 compensation functions to calculate the actual values. The temperature
 * and humidity data is stored in the s_data_sensor_temp structure and printed. If an 
 * error occurs during initialization or reading, an error message is logged.
 */
void read_data_from_bme280()
{
    // BME280 I2C communication structure
	struct bme280_t bme280 = {
		.bus_write = bme280_i2c_bus_write,
		.bus_read = bme280_i2c_bus_read,
		.dev_addr = BME280_I2C_ADDRESS1,
		.delay_msec = bme280_delay_msek};

	s32 com_rslt;
	s32 v_uncomp_pressure_s32;
	s32 v_uncomp_temperature_s32;
	s32 v_uncomp_humidity_s32;

	// Initialize BME280 sensor and set internal parameters
	com_rslt = bme280_init(&bme280);
	printf("com_rslt %d\n", com_rslt);

	com_rslt += bme280_set_oversamp_pressure(BME280_OVERSAMP_16X);
	com_rslt += bme280_set_oversamp_temperature(BME280_OVERSAMP_2X);
	com_rslt += bme280_set_oversamp_humidity(BME280_OVERSAMP_1X);

	com_rslt += bme280_set_standby_durn(BME280_STANDBY_TIME_1_MS);
	com_rslt += bme280_set_filter(BME280_FILTER_COEFF_16);

	com_rslt += bme280_set_power_mode(BME280_NORMAL_MODE);
	if (com_rslt == SUCCESS)
	{
			// Read BME280 data
			com_rslt = bme280_read_uncomp_pressure_temperature_humidity(
				&v_uncomp_pressure_s32, &v_uncomp_temperature_s32, &v_uncomp_humidity_s32);

			double temp = bme280_compensate_temperature_double(v_uncomp_temperature_s32);

			double hum = bme280_compensate_humidity_double(v_uncomp_humidity_s32);

			if (com_rslt == SUCCESS)
			{
				s_data_sensor_temp.temperature = temp;
                s_data_sensor_temp.humidity = hum;
                
			}
			else
			{
				ESP_LOGE(TAG, "measure error. code: %d", com_rslt);
			}
		
	}
	else
	{
		ESP_LOGE(TAG, "init or setting error. code: %d", com_rslt);
	}
}

/*________________________________Initialize Wi Fi__________________________________*/
void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(example_connect());
}

/*_______________________________Initialize memory__________________________________*/
static void initialize_nvs(void)
{
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
        error = nvs_flash_init();
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(error);
}


/*___________________________________MAIN_APP ______________________________________*/
/*
 * @brief This device has 2 state
 * State 1: Read data from sensor, then upload to datastream of Blynk platform.
 * State 2: After recieve user's request from Blynk, the program get advice generated by ChatGPT,
 * upload its to terminal on Blynk platform.
 * 
*/
void app_main(void)
{
// Initialize memory
    initialize_nvs();

// Initialize I2C master
    i2c_master_init();

// Wifi
    wifi_init();

// Loop 
    while(true){

        read_data_from_bme280();

        get_data_from_openweather();

        send_data_sensor_to_blynk();
        
        read_devide_status();
        
        if (devide_status == 1) 
        {
            chatgpt_generate_text();
            prompt = s_data_sensor_temp.text_response;
            write_prompt_to_blynk_terminal();
            devide_status = 0;
            change_devide_status_info_on_blynk();
        }
    } 
}

