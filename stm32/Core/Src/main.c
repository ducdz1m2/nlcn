/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "fonts.h"
#include "gui.h"
#include "st7789_adapter.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "qrcodegen.h"
#include "st7789.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
const int32_t DEADZONE = 300;
uint8_t bufr[500];
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart3;

osThreadId defaultTaskHandle;
osThreadId StartInputTaskHandle;
osMessageQId InputEventHandle;
/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_USART3_UART_Init(void);
void StartDefaultTask(void const * argument);
void InputTask(void const * argument);

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
extern DuckEngineDriver duckDriver;
void draw_rectangle(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
	duckDriver.DrawFilledRectangle(x, y, w, h, color);
}
void drawQRCode(const uint8_t *qrcode, int scale, int offset_x, int offset_y) {
	int size = qrcodegen_getSize(qrcode);
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			if (qrcodegen_getModule(qrcode, x, y)) {
				draw_rectangle(offset_x + x * scale, offset_y + y * scale,
						scale, scale, BLACK);
			} else {
				draw_rectangle(offset_x + x * scale, offset_y + y * scale,
						scale, scale, WHITE);
			}
		}
	}
}
int8_t my_filter_adc(uint16_t raw_dir) {
	int32_t centered = (int32_t) raw_dir - 1967;
	int32_t scaled = (centered * 128) / 2048;
	if (scaled > 127)
		scaled = 127;
	if (scaled < -128)
		scaled = -128;
	return (int8_t) scaled;
}
void DuckEngineHanleInput() {
	static uint8_t isLeftHeld = 0, isRightHeld = 0, isUpHeld = 0,
			isDownHeld = 0;
	static uint8_t isAHeld = 0, isBHeld = 0;
	static uint32_t lastInputTime = 0;
	if (HAL_GetTick() - lastInputTime < 100)
		return;
	lastInputTime = HAL_GetTick();
	int sumX = 0, sumY = 0;
	for (int i = 0; i < 5; i++) {
		sumX += HAL_ADC_GetValue(&hadc2);
		sumY += HAL_ADC_GetValue(&hadc1);
	}
	int avgX = sumX / 5;
	int avgY = sumY / 5;
	int8_t joystick_x = my_filter_adc(avgX);
	int8_t joystick_y = my_filter_adc(avgY);
	InputEvent ipev;
	if (joystick_x <= -100) {
		if (!isLeftHeld) {
			ipev = J_LEFT;
			xQueueSend(InputEventHandle, &ipev, portMAX_DELAY);
			isLeftHeld = 1;
		}
	} else {
		isLeftHeld = 0;
	}
	if (joystick_x >= 90) {
		if (!isRightHeld) {
			ipev = J_RIGHT;
			xQueueSend(InputEventHandle, &ipev, portMAX_DELAY);
			isRightHeld = 1;
		}
	} else {
		isRightHeld = 0;
	}
	if (joystick_y >= 100) {
		if (!isUpHeld) {
			ipev = J_UP;
			xQueueSend(InputEventHandle, &ipev, portMAX_DELAY);
			isUpHeld = 1;
		}
	} else {
		isUpHeld = 0;
	}
	if (joystick_y <= -100) {
		if (!isDownHeld) {
			ipev = J_DOWN;
			xQueueSend(InputEventHandle, &ipev, portMAX_DELAY);
			isDownHeld = 1;
		}
	} else {
		isDownHeld = 0;
	}
	if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == 1) {
		if (!isAHeld) {
			ipev = BUTTON_A_CLICK;
			xQueueSend(InputEventHandle, &ipev, portMAX_DELAY);
			isAHeld = 1;
		}
	} else {
		isAHeld = 0;
	}
	if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == 1) {
		if (!isBHeld) {
			ipev = BUTTON_B_CLICK;
			xQueueSend(InputEventHandle, &ipev, portMAX_DELAY);
			isBHeld = 1;
		}
	} else {
		isBHeld = 0;
	}
}
DuckPage usr_home;
DuckPage WifiModal;
DuckPage ConfigModal;
void DuckEngine_Draw_Header() {
	duckDriver.DrawLine(0, 26, 280, 26, WHITE);
}
void DuckEngine_Draw_Footer() {
	duckDriver.DrawFilledRectangle(0, 210, 280, 30, DEEPPURPLE);
}
int _write(int fd, unsigned char *buf, int len) {
	if (fd == 1 || fd == 2) {
		HAL_UART_Transmit(&huart3, buf, len, 999);
	}
	return len;
}

void sendUARTCommand(const char *command, char *response,
		uint16_t response_size) {
	memset(response, '\0', response_size);

	// Clear any pending data in the UART receive buffer before transmitting a new command
	// This is crucial to prevent reading leftover bytes from previous communications.
	uint8_t dummy_read;
	while (HAL_UART_Receive(&huart3, &dummy_read, 1, 10) == HAL_OK)
		;

	// Send the command to the ESP32
	HAL_UART_Transmit(&huart3, (uint8_t*) command, strlen(command),
	HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart3, (uint8_t*) "\n", 1, HAL_MAX_DELAY);

	// Receive the response
	uint16_t index = 0;
	uint32_t startTime = HAL_GetTick();
	const uint32_t timeout = 1000;
	while (index < response_size - 1 && (HAL_GetTick() - startTime) < timeout) {
		uint8_t c;
		if (HAL_UART_Receive(&huart3, &c, 1, 10) == HAL_OK) {
			if (c == '\n' || c == '\r') {
				break; // Stop on newline or carriage return
			}
			response[index++] = c;
		}
	}
	response[index] = '\0';

	// For debugging: Print the received response to see if it's now correct
//	printf("Received QR string: %s\n", response);
}
void getStorageInfo(char *response, uint16_t response_size) {
	sendUARTCommand("GET_STORAGE", response, response_size);
	if (strncmp(response, "ERROR", 5) == 0) {
		snprintf(response, response_size, "Error: GET_STORAGE");
	}
}
void getFilesInfo(char *response, uint16_t response_size) {
	sendUARTCommand("GET_FILES", response, response_size);
	if (strncmp(response, "ERROR", 5) == 0
			|| strncmp(response, "Files: ", 7) != 0) {
		snprintf(response, response_size, "Files: 0");
	}
}
void getUptime(char *response, uint16_t response_size) {
	sendUARTCommand("GET_UPTIME", response, response_size);
	if (strncmp(response, "ERROR", 5) == 0) {
		snprintf(response, response_size, "Error: GET_UPTIME");
	}
}
void getMemoryInfo(char *response, uint16_t response_size) {
	sendUARTCommand("GET_MEMORY", response, response_size);
	if (strncmp(response, "ERROR", 5) == 0) {
		snprintf(response, response_size, "Error: GET_MEMORY");
	}
}
void getWiFiStatus(char *response, uint16_t response_size) {
	sendUARTCommand("GET_WIFI", response, response_size);
	if (strncmp(response, "ERROR", 5) == 0) {
		snprintf(response, response_size, "Error: GET_WIFI");
	}
}
void showLoadingModal() {
	duckDriver.DrawFilledRectangle(0, 0, 280, 240, BLACK);
	duckDriver.WriteString(40, 100, "Generating QR code...", Font_11x18, WHITE,
	BLACK);
	DuckEngine_Render();
}
void BackToHome(DuckObject *obj) {
	DuckEngine_Set_CurrentPage(DuckEngine_Get_CurrentPage()->type, PAGE_HOME);
}
// Trạng thái QR
typedef struct {
    bool qr_generated;
    char wifi_data[128];
} WifiModalState;

static WifiModalState wifiModalState;

void OpenWifiModal(DuckObject *obj) {
    // Lưu trạng thái
    memset(&wifiModalState, 0, sizeof(wifiModalState));

    DuckEngine_Set_CurrentPage(DuckEngine_Get_CurrentPage()->type, PAGE_WIFI_MODAL);

    // Xóa màn hình
    duckDriver.DrawFilledRectangle(0, 0, 280, 240, WHITE);

    // UI cơ bản
    DuckEngine_Create_Label(80, 10, "Scan to connect WiFi", &Font_7x10, BLACK, WHITE, &WifiModal, 0);
    DuckEngine_Create_Button(80, 200, 100, 30, "Back", LABEL_CENTER, &Font_7x10,
                             WHITE, 0, BLACK, &WifiModal, 1, BackToHome);
    DuckEngine_Render();
}

// Hàm update QR (gọi mỗi frame trong main loop)
void UpdateWifiModal() {
    // Nếu không ở trang này thì bỏ qua
    if (DuckEngine_Get_CurrentPage()->type != PAGE_WIFI_MODAL) {
        return;
    }

    // Nếu chưa tạo QR
    if (!wifiModalState.qr_generated) {
        memset(wifiModalState.wifi_data, 0, sizeof(wifiModalState.wifi_data));
        sendUARTCommand("GET_WIFI_QR", wifiModalState.wifi_data, sizeof(wifiModalState.wifi_data) - 1);
        wifiModalState.wifi_data[sizeof(wifiModalState.wifi_data) - 1] = '\0';

        if (strlen(wifiModalState.wifi_data) == 0) {
            duckDriver.WriteString(60, 100, "No WiFi data received", Font_7x10, RED, WHITE);
            wifiModalState.qr_generated = true; // Không cần vẽ nữa
            return;
        }

        // Tạo QR
        uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
        uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

        bool ok = qrcodegen_encodeText(wifiModalState.wifi_data, tempBuffer, qrcode,
                                       qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN,
                                       qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);

        if (!ok) {
            duckDriver.WriteString(40, 100, "QR encode failed", Font_7x10, RED, WHITE);
            wifiModalState.qr_generated = true;
            return;
        }

        int size = qrcodegen_getSize(qrcode);
        int scale = 4;
        int offset_x = (280 - size * scale) / 2;
        int offset_y = (240 - size * scale) / 2;

        // Vẽ QR
        for (int y = 0; y < size; y++) {
            // Nếu người dùng bấm Back giữa chừng thì thoát luôn
            if (DuckEngine_Get_CurrentPage()->type != PAGE_WIFI_MODAL) {
                return;
            }
            for (int x = 0; x < size; x++) {
                uint16_t color = qrcodegen_getModule(qrcode, x, y) ? BLACK : WHITE;
                draw_rectangle(offset_x + x * scale, offset_y + y * scale, scale, scale, color);
            }
        }
        DuckEngine_Render();
        wifiModalState.qr_generated = true;
    }
}

void OpenConfigModal(DuckObject *obj) {
    // Save state
    DuckEngine_Set_CurrentPage(DuckEngine_Get_CurrentPage()->type, PAGE_CONFIG_MODAL);

    // Clear the screen with a black background


    // Add a title to the settings window
    DuckEngine_Create_Label(80, 5, "MANUALS", &Font_11x18, SOFTWHITE, BLACK, &ConfigModal, -1);

    // General instructions label
    DuckEngine_Create_Label(20, 40, "Use manual to manage your device settings.", &Font_7x10, PASTELGREEN, BLACK, &ConfigModal, 0);

    // Labels for the WiFi Configuration section
    DuckEngine_Create_Label(20, 60, "WIFI SETUP:", &Font_7x10, PASTELPINK, BLACK, &ConfigModal, 1);
    DuckEngine_Create_Label(20, 80, "Method 1: Enter SSID/password.", &Font_7x10, SOFTWHITE, BLACK, &ConfigModal, 2);
    DuckEngine_Create_Label(20, 100, "Method 2: Scan QR code.", &Font_7x10, SOFTWHITE, BLACK, &ConfigModal, 3);
    DuckEngine_Create_Label(20, 120, "For advanced settings, go to the Admin panel.", &Font_7x10, CLOUDBLUE, BLACK, &ConfigModal, 4);

    // Labels for the general Configuration section
    DuckEngine_Create_Label(20, 150, "ADMIN SETTINGS:", &Font_7x10, PASTELYELLOW, BLACK, &ConfigModal, 5);
    DuckEngine_Create_Label(20, 170, "Change admin password and network options.", &Font_7x10, SOFTWHITE, BLACK, &ConfigModal, 6);

    // "Back" button to return to the home page
    DuckEngine_Create_Button(80, 200, 100, 30, "Back", LABEL_CENTER, &Font_7x10, WHITE, 0, PASTELBLUE, &ConfigModal, 7, BackToHome);

    DuckEngine_Render();
}
void HomePage(DuckPage *page) {
	char response[64];
	DuckEngine_Create_Label(80, 5, "NAS MANAGER", &Font_11x18, SOFTWHITE, BLACK,
			page, 0);
	getStorageInfo(response, sizeof(response));
	page->storage_label = DuckEngine_Create_Label(10, 35, response, &Font_7x10,
	PASTELGREEN, BLACK, page, 0);
	getFilesInfo(response, sizeof(response));
	page->files_label = DuckEngine_Create_Label(10, 50, response, &Font_7x10,
	PASTELGREEN, BLACK, page, 0);
	getUptime(response, sizeof(response));
	page->uptime_label = DuckEngine_Create_Label(10, 70, response, &Font_7x10,
	CLOUDBLUE, BLACK, page, 0);
	getMemoryInfo(response, sizeof(response));
	page->memory_label = DuckEngine_Create_Label(10, 85, response, &Font_7x10,
	CLOUDBLUE, BLACK, page, 0);
	DuckEngine_Create_Label(10, 110, "WiFi:", &Font_7x10, PASTELPINK, BLACK,
			page, 0);
	getWiFiStatus(response, sizeof(response));
	page->wifi_label = DuckEngine_Create_Label(60, 110, response, &Font_7x10,
	PASTELPINK, BLACK, page, 0);
	DuckEngine_Create_Button(30, 140, 100, 30, "Connect to WiFi", LABEL_CENTER,
			&Font_7x10, BLACK, 0, PASTELBLUE, page, 1, OpenWifiModal);
	DuckEngine_Create_Button(150, 140, 100, 30, "Change Config", LABEL_CENTER,
			&Font_7x10, BLACK, 0, PASTELYELLOW, page, 1, OpenConfigModal);
	DuckEngine_Create_Label(100, 200, "Version: 1.0", &Font_7x10, SOFTWHITE,
	BLACK, page, 0);
}
void UpdateHomePageLabels(DuckPage *page) {
	char response[128];
	getStorageInfo(response, sizeof(response));
	if (page->storage_label) {
		DuckLabel *label = (DuckLabel*) page->storage_label;
		memset(label->text, 0, sizeof(label->text));
		strncpy(label->text, response, sizeof(label->text) - 1);
		label->text[sizeof(label->text) - 1] = '\0';
		label->base.isNeedUpdate = 1;
	}
	getFilesInfo(response, sizeof(response));
	if (page->files_label) {
		DuckLabel *label = (DuckLabel*) page->files_label;
		memset(label->text, 0, sizeof(label->text));
		strncpy(label->text, response, sizeof(label->text) - 1);
		label->text[sizeof(label->text) - 1] = '\0';
		label->base.isNeedUpdate = 1;
	}
	getUptime(response, sizeof(response));
	if (page->uptime_label) {
		DuckLabel *label = (DuckLabel*) page->uptime_label;
		memset(label->text, 0, sizeof(label->text));
		strncpy(label->text, response, sizeof(label->text) - 1);
		label->text[sizeof(label->text) - 1] = '\0';
		label->base.isNeedUpdate = 1;
	}
	getMemoryInfo(response, sizeof(response));
	if (page->memory_label) {
		DuckLabel *label = (DuckLabel*) page->memory_label;
		memset(label->text, 0, sizeof(label->text));
		strncpy(label->text, response, sizeof(label->text) - 1);
		label->text[sizeof(label->text) - 1] = '\0';
		label->base.isNeedUpdate = 1;
	}
	getWiFiStatus(response, sizeof(response));
	if (page->wifi_label) {
		DuckLabel *label = (DuckLabel*) page->wifi_label;
		memset(label->text, 0, sizeof(label->text));
		strncpy(label->text, response, sizeof(label->text) - 1);
		label->text[sizeof(label->text) - 1] = '\0';
		label->base.isNeedUpdate = 1;
	}
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of InputEvent */
  osMessageQDef(InputEvent, 16, InputEvent);
  InputEventHandle = osMessageCreate(osMessageQ(InputEvent), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 1000);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of StartInputTask */
  osThreadDef(StartInputTask, InputTask, osPriorityAboveNormal, 0, 512);
  StartInputTaskHandle = osThreadCreate(osThread(StartInputTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.ContinuousConvMode = ENABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_1LINE;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 921600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, ST7789_DC_Pin|ST7789_RST_Pin|ST7789_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pins : SWA_Pin SWB_Pin */
  GPIO_InitStruct.Pin = SWA_Pin|SWB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : ST7789_DC_Pin ST7789_RST_Pin ST7789_CS_Pin */
  GPIO_InitStruct.Pin = ST7789_DC_Pin|ST7789_RST_Pin|ST7789_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_InputTask */
/**
 * @brief Function implementing the StartInputTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_InputTask */
void InputTask(void const * argument)
{
  /* USER CODE BEGIN InputTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END InputTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM2 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM2)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
