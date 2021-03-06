
#include "main.h"
#include "bsp.h"
#include "config.h"
#include "gsm.h"
#include "cmsis_os.h"
#include "at.h"
#include "gps.h"
#include "l3gd20.h"
#include "lis3dh.h"
#include "lsm303c.h"

#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#define GPS_FLAG 0x3FFFFFFF;

USBD_HandleTypeDef USBD_Device;

volatile uint32_t poicnt = 0, poi_period = 0, acccnt = 0, logcnt = 0, log_speed = 0, log_period = 0, log_repeat = 0, log_timeout = 0, totalRecordingTime = 0;

char txline[LINE_BUFFER_SIZE];
char rxline[LINE_BUFFER_SIZE];

volatile char AccStat;
volatile char InsTick;
volatile char Tunnel;
volatile char UsbStat;

extern char SDPath[4];     // SD card logical drive path
extern FATFS SDFatFs;      // File system object for SD card logical drive
extern TIM_HandleTypeDef t1;

static InsSample currentSample, gpsSample;

void dputc(char ch)
{
    if (Tunnel) return;
    if (Pwr.LowVbat) return;
    BSP_USART_SendData(USART1, ch);
}

void InertialSampleTask()
{
    if ((GyroStatus != GYRO_OK) || (AccStatus != ACC_OK)|| (MagStatus != MAG_OK)) return;
    L3GD20_Handler();
    LIS3DH_Handler();
    LSM303C_Handler();
    currentSample.AccX = Acc[0]; currentSample.AccY = Acc[1]; currentSample.AccZ = Acc[2];
    currentSample.AngX = AngRate[0]; currentSample.AngY = AngRate[1]; currentSample.AngZ = AngRate[2];
    currentSample.MagX = MagInt[0]; currentSample.MagY = MagInt[1]; currentSample.MagZ = MagInt[2];
    InsTick = 1;
}

void IncCntFile()
{
    FIL file;
    uint32_t bw;

    if (FR_OK != f_open(&file, "cnt.txt", FA_WRITE | FA_READ)) {
        acccnt = 0;
        f_open(&file, "cnt.txt", FA_CREATE_NEW | FA_WRITE);
        f_lseek(&file, 0);
        f_write(&file, (void*)&acccnt, sizeof(acccnt), &bw);
    }
    else {
        f_lseek(&file, 0);
        f_read(&file, (void *)&acccnt, sizeof(acccnt), &bw);
        acccnt++;
        f_lseek(&file, 0);
        f_write(&file, (void *)&acccnt, sizeof(acccnt), &bw);
    }
    f_close(&file);
}


int main()
{
    BSP_Config();

    poicnt = 0;
    acccnt = 0;
    AccStat = 0;
    UsbStat = 0;
    Tunnel = 0;

    // Init file system on SD-card
    FATFS_LinkDriver(&SD_Driver, SDPath);
    f_mount(&SDFatFs, (TCHAR const*)SDPath, 0);

    // Start tasks
    osThreadDef(MainThread, MainThread, osPriorityNormal, 1, 1 * 16 * configMINIMAL_STACK_SIZE);
    osThreadCreate(osThread(MainThread), NULL);

    // Start scheduler
    osKernelStart(NULL, NULL);
    // We should never get here as control is now taken by the scheduler
    while (1);
}

#define ONE_SECOND_US    1000000
static void MainThread(void const *argument)
{
    uint32_t bw, recordingTime = 0, log_speed_file = 0;
    char LogFilePath[16], NmeaFilePath[16];
    FIL accfile, nmeafile;

    IncCntFile();
    sprintf(LogFilePath, "a%07u.ivs\0", acccnt);
    sprintf(NmeaFilePath, "n%07u.ivs\0", acccnt);

    // Init INS log file
    if (FR_OK != f_open(&accfile, LogFilePath, FA_WRITE))
    f_open(&accfile, LogFilePath, FA_CREATE_NEW | FA_WRITE);
    f_close(&accfile);

    // Init GPS log file
    if (FR_OK != f_open(&nmeafile, NmeaFilePath, FA_WRITE))
    f_open(&nmeafile, NmeaFilePath, FA_CREATE_NEW | FA_WRITE);
    f_close(&nmeafile);

    // Load configuration from SD card
    CFG_LoadConfigFile();
    log_repeat = atoi(CFG_GlobVarsStruct.logRepeat);
    log_period = atoi(CFG_GlobVarsStruct.logPeriod);
    log_speed = atoi(CFG_GlobVarsStruct.logSpeed) * 1000;
    log_timeout = atoi(CFG_GlobVarsStruct.logTimeout) * 1000;
    t1.Init.Period = log_speed;
    HAL_TIM_Base_Init(&t1);

    // Init MSC Application
    USBD_Init(&USBD_Device, &MSC_Desc, 0);
    USBD_RegisterClass(&USBD_Device, &USBD_MSC);
    USBD_MSC_RegisterStorage(&USBD_Device, &USBD_DISK_fops);
    USBD_Start(&USBD_Device);

    BSP_SPI1_Init_1_Line();
    MagStatus = (LSM303C_StatusTypedef)LSM303C_Configure();
    BSP_SPI1_Init_2_Lines();
    AccStatus = (LIS3DH_StatusTypedef)LIS3DH_Configure();
    GyroStatus = (L3GD20_StatusTypedef)L3GD20_Configure();
    if ((GyroStatus == GYRO_OK) && (AccStatus == ACC_OK) && (MagStatus == MAG_OK)) AccStat = 1;
    GSM_Init();

    // We use the gpsSample.AccX as a flag if a gps flag sample should be written in the file
    // We use other memebers only as gps flag values tobe written to the file
    gpsSample.AccX = 0;
    gpsSample.AngX = GPS_FLAG;
    gpsSample.MagX = GPS_FLAG;
    gpsSample.AccY = GPS_FLAG;
    gpsSample.AngY = GPS_FLAG;
    gpsSample.MagY = GPS_FLAG;
    gpsSample.AccZ = GPS_FLAG;
    gpsSample.AngZ = GPS_FLAG;
    gpsSample.MagZ = GPS_FLAG;

    // This variable hold the current recording time. When it reaches the total recording time
    // As set in the config file the cycle will break
    recordingTime = 0;
    totalRecordingTime = log_period*(ONE_SECOND_US / log_speed);

    // Open INS log file
    f_open(&accfile, LogFilePath, FA_WRITE);
    f_lseek(&accfile, f_size(&accfile));
    log_speed_file = log_speed / 1000;
    f_write(&accfile, &log_speed_file, sizeof(uint32_t), &bw);

    // Open GPS log file
    f_open(&nmeafile, NmeaFilePath, FA_WRITE);
    f_lseek(&nmeafile, f_size(&nmeafile));

    HAL_Delay(log_timeout);

//     //Wait for GPS to get fix
//    while(!GpsStat.Fix)
//    {
//        if (DetectPPS())
//        {
//            GpsStat.Req = true;
//        }
//        GPS_Handler();
//        GSM_Handler();
//    }

    nmea[0] = '\0';
    HAL_TIM_Base_Start_IT(&t1);

    while (1)
    {
        if (InsTick)
        {
            InsTick = 0;
            recordingTime++;

            // This is the value of GPS_FLAG. For some reason we cannot use it directly
            if (gpsSample.AccX == 0x3FFFFFFF)
            {
                f_write(&accfile, &gpsSample, sizeof(InsSample), &bw);
                gpsSample.AccX = 0;
            }

            f_write(&accfile, &currentSample, sizeof(InsSample), &bw);

            if(recordingTime >= totalRecordingTime)
            {
              break;
            }
        }

        if (DetectPPS())
        {
            GpsStat.Req = true;
            GpsStat.Rdy = false;
            gpsSample.AccX = GPS_FLAG;
        }

        GPS_Handler();
        GSM_Handler();

        if (GpsStat.Rdy == true)
        {
            f_write(&nmeafile, &nmea, strlen(nmea), &bw);
            GpsStat.Rdy = false;
        }
    }

    f_close(&accfile);
    f_close(&nmeafile);
    HAL_TIM_Base_Stop(&t1);

    while(1)
    {
      USB_Handler();
    }
}

void USB_Handler()
{
    if (UsbStat == 0) {
        if ((USBD_Device.pClassData != NULL) &&
                ((USBD_Device.dev_state != USBD_STATE_DEFAULT) && (USBD_Device.dev_state != USBD_STATE_SUSPENDED))) {
            UsbStat = 1;
            HAL_Delay(100);
        }
    }
    else { // USB plugged out
        if ((USBD_Device.dev_state == USBD_STATE_DEFAULT) || (USBD_Device.dev_state == USBD_STATE_SUSPENDED)) {
            UsbStat = 0;
            HAL_Delay(100);
            log_repeat = atoi(CFG_GlobVarsStruct.logRepeat);
            log_period = atoi(CFG_GlobVarsStruct.logPeriod);
            log_speed = atoi(CFG_GlobVarsStruct.logSpeed) * 1000;
            log_timeout = atoi(CFG_GlobVarsStruct.logTimeout) * 1000;
            t1.Init.Period = log_speed;
            HAL_TIM_Base_Init(&t1);
            HAL_TIM_Base_Start_IT(&t1);
            totalRecordingTime = log_period*(ONE_SECOND_US / log_speed);
        }
    }
}


/**
* @brief  Retargets the C library printf function to the USART.
* @param  None
* @retval None
*/
PUTCHAR_PROTOTYPE
{
    // Place your implementation of fputc here
    // e.g. write a character to the GSM_COM1 and Loop until the end of transmission
    //osDelay(1);
    //HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 100);

    BSP_USART_SendData(USART2, (uint16_t)ch);                 // GSM port
    //./BSP_USART_SendData(USART1, (uint16_t)ch);                 // PC port

    return ch;
}

/**
* @brief  Debug print function to the USART.
* @param  None
* @retval None
*/
void DebugPrint(char *pMsg, uint16_t msgSize)
{
    uint16_t i = 0;

    if (!msgSize) {
        while (*pMsg) dputc((uint16_t)*pMsg++);
    }
    else {
        for (i = 0; i < msgSize; i++) dputc((uint16_t)pMsg[i]);
    }
}

uint32_t GetPhrase(char *dst, char eop) {
    uint32_t c, i, j, l;

    __disable_interrupt();
    if(rxBuffIdx) {
        for(i = 0; i < rxBuffIdx; i++) if(AtRxBuffer[i] >= ' ') break;
        for(l = 0; i < rxBuffIdx; l++) {
            dst[l] = AtRxBuffer[i];
            i++;
            if((dst[l] == eop) || (dst[l] == '\r')) break;
        }
        dst[l] = 0;
        c = rxBuffIdx - i;
        if(c != 0) {
            for(j = 0; i < rxBuffIdx; j++, i++) AtRxBuffer[j] = AtRxBuffer[i];
            rxBuffIdx = c;
        }
        else l = 0;
    }
    else l = 0;
    __enable_interrupt();

    return l;
}
