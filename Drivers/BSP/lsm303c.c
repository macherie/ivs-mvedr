
#include "main.h"
#include "lsm303c.h"
#include "bsp.h"

LSM303C_StatusTypedef MagStatus = MAG_ERROR;
int32_t MagInt[3] = {0, 0, 0};

void LSM303C_Handler(void)
{
  if (LSM303C_GetDataStatus() & 0x08) {
      LSM303C_ReadXYZMag();
  }
}


void LSM303C_ReadXYZMag(void)
{
  uint8_t tmpbuffer[6] = {0};
  int16_t RawData[3] = {0};
  uint8_t tmpreg = 0;
  int i =0;

  LSM303C_Read(&tmpreg, LSM303C_CTRL_REG4_M, 1);

  LSM303C_Read(tmpbuffer, LSM303C_OUT_X_L_M, 6);

  // check in the control register 4 the data alignment (Big Endian or Little Endian)
  if(!(tmpreg & LSM303C_BLE_MSB)) {
    for(i = 0; i < 3; i++)
    {
      RawData[i]=(int16_t)(((uint16_t)tmpbuffer[2*i+1] << 8) + tmpbuffer[2*i]);
    }
  } else {
    for(i = 0; i < 3; i++)
    {
      RawData[i]=(int16_t)(((uint16_t)tmpbuffer[2*i] << 8) + tmpbuffer[2*i+1]);
    }
  }

  for (i = 0; i < 3; i++) MagInt[i] = RawData[i];
}


uint8_t LSM303C_GetDataStatus(void)
{
  uint8_t tmpreg;

  // Read STATUS_REG register
  LSM303C_Read(&tmpreg, LSM303C_STATUS_REG_M, 1);

  return tmpreg;
}

uint8_t LSM303C_Configure(void)
{
  uint8_t retVal = MAG_ERROR;
  uint8_t reg;

  BSP_MagInit();

  // Enable SPI read/write
  reg = 0x84; // I2C disabled , SPI -R/W, Cont.conv
  LSM303C_Write(&reg, LSM303C_CTRL_REG3_M, 1);

  if(LSM303C_ReadID() == I_AM_LSM303C_M) {
    // Configure Mems LSM303C
    // Write value to MAG CTRL_REG1 regsister
    reg = 0x50;
    LSM303C_Write(&reg, LSM303C_CTRL_REG1_M, 1);

    // Write value to MAG CTRL_REG2 regsister
    reg = 0x60;
    LSM303C_Write(&reg, LSM303C_CTRL_REG2_M, 1);

     // Write value to MAG CTRL_REG4 regsister
    reg = 0x08;
    LSM303C_Write(&reg, LSM303C_CTRL_REG4_M, 1);

    // Write value to MAG CTRL_REG5 regsister
    reg = 0x00;
    LSM303C_Write(&reg, LSM303C_CTRL_REG5_M, 1);

    retVal = MAG_OK;
  }

  return retVal;
}


uint8_t LSM303C_ReadID(void)
{
  uint8_t tmp = 0;

  // Read WHO I AM register
  LSM303C_Read(&tmp, LSM303C_WHO_AM_I_M, 1);

  // Return the ID
  return (uint8_t)tmp;
}


void LSM303C_Write(uint8_t* pBuffer, uint8_t WriteAddr, uint16_t NumByteToWrite)
{
  // Configure the MS bit:
  //  - When 0, the address will remain unchanged in multiple read/write commands.
  //  - When 1, the address will be auto incremented in multiple read/write commands.

  if(NumByteToWrite > 0x01) {
    WriteAddr |= (uint8_t)LSM303C_MULTIPLEBYTE_CMD;
  }

  hspi1.Init.Direction = SPI_DIRECTION_1LINE;
  HAL_SPI_Init(&hspi1);

  // Set chip select Low at the start of the transmission
  __HAL_SPI_1LINE_TX(&hspi1);
  BSP_MAG_CS_LOW();

  // Send the Address of the indexed register
  HAL_SPI_Transmit(&hspi1, &WriteAddr, 1, SpiTimeout);

  // Send the data that will be written into the device (MSB First)
  HAL_SPI_Transmit(&hspi1, pBuffer, NumByteToWrite, SpiTimeout);

  // Set chip select High at the end of the transmission
  BSP_MAG_CS_HIGH();

  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  HAL_SPI_Init(&hspi1);
}

void LSM303C_Read(uint8_t* pBuffer, uint8_t ReadAddr, uint16_t NumByteToRead)
{
  if(NumByteToRead > 0x01) {
    ReadAddr |= (uint8_t)(LSM303C_READWRITE_CMD | LSM303C_MULTIPLEBYTE_CMD);
  } else {
    ReadAddr |= (uint8_t)LSM303C_READWRITE_CMD;
  }

  hspi1.Init.Direction = SPI_DIRECTION_1LINE;
  HAL_SPI_Init(&hspi1);

  // Set chip select Low at the start of the transmission
  __HAL_SPI_1LINE_TX(&hspi1);
  BSP_MAG_CS_LOW();

  // Send the Address of the indexed register
  HAL_SPI_Transmit(&hspi1, &ReadAddr, 1, SpiTimeout);

  // Receive the data that will be read from the device (MSB First)
  __HAL_SPI_1LINE_RX(&hspi1);
  HAL_SPI_Receive(&hspi1, pBuffer, NumByteToRead, SpiTimeout);

  // Set chip select High at the end of the transmission
  BSP_MAG_CS_HIGH();

  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  HAL_SPI_Init(&hspi1);
}