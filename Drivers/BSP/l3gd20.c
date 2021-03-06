
#include "main.h"
#include "l3gd20.h"
#include "bsp.h"

L3GD20_StatusTypedef GyroStatus = GYRO_ERROR;
int32_t AngRate[3] = {0, 0, 0};

void L3GD20_Handler(void)
{
  uint8_t dataStatus = L3GD20_GetDataStatus();

  if (dataStatus & 0x08) {
    L3GD20_ReadXYZAngRate();
  }
}

uint8_t L3GD20_Configure(void)
{
  uint8_t retVal = GYRO_ERROR;

  L3GD20_InitTypedef L3GD20_InitStructure;
  L3GD20_FilterConfigTypedef L3GD20_FilterStructure;


  // Configure Mems L3GD20
  L3GD20_InitStructure.Power_Mode = L3GD20_MODE_ACTIVE;
  L3GD20_InitStructure.Output_DataRate = L3GD20_OUTPUT_DATARATE_4;
  L3GD20_InitStructure.Axes_Enable = L3GD20_AXES_ENABLE;
  L3GD20_InitStructure.Band_Width = L3GD20_BANDWIDTH_1;
  L3GD20_InitStructure.BlockData_Update = L3GD20_BlockDataUpdate_Continous;
  L3GD20_InitStructure.Endianness = L3GD20_BLE_LSB;
  L3GD20_InitStructure.Full_Scale = L3GD20_FULLSCALE_500;
  L3GD20_InitStructure.SpiWireMode = L3GD20_SIM_4_WIRE;
  L3GD20_Init(&L3GD20_InitStructure);

  L3GD20_FilterStructure.HighPassFilter_Mode_Selection = L3GD20_HPM_NORMAL_MODE_RES;
  L3GD20_FilterStructure.HighPassFilter_CutOff_Frequency = L3GD20_HPFCF_0;
  L3GD20_FilterConfig(&L3GD20_FilterStructure);

  L3GD20_FilterCmd(L3GD20_HIGHPASSFILTER_DISABLE);

  //L3GD20_INT2InterruptCmd(L3GD20_INT2INTERRUPT_ENABLE);

  if(L3GD20_ReadID() == I_AM_L3GD20) {
    retVal = GYRO_OK;
  }

  return retVal;
}

void L3GD20_Init(L3GD20_InitTypedef *L3GD20_InitStruct)
{
  uint8_t ctrl1 = 0x00, ctrl4 = 0x00;

  BSP_GyroInit();

  // Configure MEMS: data rate, power mode, full scale and axes
  ctrl1 |= (uint8_t) (L3GD20_InitStruct->Power_Mode | L3GD20_InitStruct->Output_DataRate | \
                      L3GD20_InitStruct->Axes_Enable | L3GD20_InitStruct->Band_Width);

  ctrl4 |= (uint8_t) (L3GD20_InitStruct->BlockData_Update | L3GD20_InitStruct->Endianness | \
                      L3GD20_InitStruct->Full_Scale | L3GD20_InitStruct->SpiWireMode);

  // Write value to MEMS CTRL_REG1 regsister
  L3GD20_Write(&ctrl1, L3GD20_CTRL_REG1_ADDR, 1);

  // Write value to MEMS CTRL_REG4 regsister
  L3GD20_Write(&ctrl4, L3GD20_CTRL_REG4_ADDR, 1);
}

uint8_t L3GD20_ReadID(void)
{
  uint8_t tmp = 0;

  // Read WHO I AM register
  L3GD20_Read(&tmp, L3GD20_WHO_AM_I_ADDR, 1);

  // Return the ID
  return (uint8_t)tmp;
}

/**
  * @brief  Get status for L3GD20 data
  * @param  None
  * @retval Data status in a L3GD20 Data
  */
uint8_t L3GD20_GetDataStatus(void)
{
  uint8_t tmpreg;

  // Read STATUS_REG register
  L3GD20_Read(&tmpreg, L3GD20_STATUS_REG_ADDR, 1);

  return tmpreg;
}

/**
* @brief  Calculate the L3GD20 angular data in mdps.
* @param  pfData : Data out pointer
* @retval None
*/
void L3GD20_ReadXYZAngRate()
{
  uint8_t tmpbuffer[6] = {0};
  int16_t RawData[3] = {0};
  uint8_t tmpreg = 0;
  int32_t sensitivity = 0;
  int i =0;

  L3GD20_Read(&tmpreg, L3GD20_CTRL_REG4_ADDR, 1);

  L3GD20_Read(tmpbuffer, L3GD20_OUT_X_L_ADDR, 6);

  // check in the control register 4 the data alignment (Big Endian or Little Endian)
  if(!(tmpreg & L3GD20_BLE_MSB)) {
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

  // Switch the sensitivity value set in the CRTL4
  switch(tmpreg & L3GD20_FULLSCALE_SELECTION) {
    case L3GD20_FULLSCALE_250 :
      sensitivity = L3GD20_SENSITIVITY_250DPS;
      break;
    case L3GD20_FULLSCALE_500 :
      sensitivity = L3GD20_SENSITIVITY_500DPS;
      break;
    case L3GD20_FULLSCALE_2000 :
      sensitivity = L3GD20_SENSITIVITY_2000DPS;
      break;
  }

  // divide by sensitivity
  for(i = 0; i < 3; i++)
  {
    AngRate[i] = (uint32_t)RawData[i] * sensitivity;
  }
}

/**
  * @brief  Set High Pass Filter Modality
  * @param  FilterStruct: contains the configuration setting for the L3GD20.
  * @retval None
  */
void L3GD20_FilterConfig(L3GD20_FilterConfigTypedef *L3GD20_FilterStruct)
{
  uint8_t tmpreg;

  // Read CTRL_REG2 register
  L3GD20_Read(&tmpreg, L3GD20_CTRL_REG2_ADDR, 1);

  tmpreg &= 0xC0;

  // Configure MEMS: mode and cutoff frquency
  tmpreg |= (uint8_t)(L3GD20_FilterStruct->HighPassFilter_Mode_Selection | \
                      L3GD20_FilterStruct->HighPassFilter_CutOff_Frequency);

  // Write value to MEMS CTRL_REG2 regsister
  L3GD20_Write(&tmpreg, L3GD20_CTRL_REG2_ADDR, 1);
}

/**
  * @brief  Enable or Disable High Pass Filter
  * @param  HighPassFilterState: new state of the High Pass Filter feature.
  *      This parameter can be:
  *         @arg: L3GD20_HIGHPASSFILTER_DISABLE
  *         @arg: L3GD20_HIGHPASSFILTER_ENABLE
  * @retval None
  */
void L3GD20_FilterCmd(uint8_t HighPassFilterState)
{
  uint8_t tmpreg;

  // Read CTRL_REG5 register
  L3GD20_Read(&tmpreg, L3GD20_CTRL_REG5_ADDR, 1);

  tmpreg &= 0xEF;

  tmpreg |= HighPassFilterState;

  // Write value to MEMS CTRL_REG5 regsister
  L3GD20_Write(&tmpreg, L3GD20_CTRL_REG5_ADDR, 1);
}

/**
  * @brief Set L3GD20 Interrupt configuration
  * @param  L3GD20_InterruptConfig_TypeDef: pointer to a L3GD20_InterruptConfig_TypeDef
  *         structure that contains the configuration setting for the L3GD20 Interrupt.
  * @retval None
  */
void L3GD20_INT1InterruptConfig(L3GD20_InterruptConfigTypedef *L3GD20_IntConfigStruct)
{
  uint8_t ctrl_cfr = 0x00, ctrl3 = 0x00;

  // Read INT1_CFG register
  L3GD20_Read(&ctrl_cfr, L3GD20_INT1_CFG_ADDR, 1);

  // Read CTRL_REG3 register
  L3GD20_Read(&ctrl3, L3GD20_CTRL_REG3_ADDR, 1);

  ctrl_cfr &= 0x80;

  ctrl3 &= 0xDF;

  // Configure latch Interrupt request and axe interrupts
  ctrl_cfr |= (uint8_t)(L3GD20_IntConfigStruct->Latch_Request| \
                   L3GD20_IntConfigStruct->Interrupt_Axes);

  ctrl3 |= (uint8_t)(L3GD20_IntConfigStruct->Interrupt_ActiveEdge);

  // Write value to MEMS INT1_CFG register
  L3GD20_Write(&ctrl_cfr, L3GD20_INT1_CFG_ADDR, 1);

  // Write value to MEMS CTRL_REG3 register
  L3GD20_Write(&ctrl3, L3GD20_CTRL_REG3_ADDR, 1);
}

/**
  * @brief  Enable or disable INT1 interrupt
  * @param  InterruptState: State of INT1 Interrupt
  *      This parameter can be:
  *        @arg L3GD20_INT1INTERRUPT_DISABLE
  *        @arg L3GD20_INT1INTERRUPT_ENABLE
  * @retval None
  */
void L3GD20_INT1InterruptCmd(uint8_t InterruptState)
{
  uint8_t tmpreg;

  /* Read CTRL_REG3 register */
  L3GD20_Read(&tmpreg, L3GD20_CTRL_REG3_ADDR, 1);

  tmpreg &= 0x7F;
  tmpreg |= InterruptState;

  /* Write value to MEMS CTRL_REG3 regsister */
  L3GD20_Write(&tmpreg, L3GD20_CTRL_REG3_ADDR, 1);
}

/**
  * @brief  Enable or disable INT2 interrupt
  * @param  InterruptState: State of INT1 Interrupt
  *      This parameter can be:
  *        @arg L3GD20_INT2INTERRUPT_DISABLE
  *        @arg L3GD20_INT2INTERRUPT_ENABLE
  * @retval None
  */
void L3GD20_INT2InterruptCmd(uint8_t InterruptState)
{
  uint8_t tmpreg;

  /* Read CTRL_REG3 register */
  L3GD20_Read(&tmpreg, L3GD20_CTRL_REG3_ADDR, 1);

  tmpreg &= 0xF7;
  tmpreg |= InterruptState;

  /* Write value to MEMS CTRL_REG3 regsister */
  L3GD20_Write(&tmpreg, L3GD20_CTRL_REG3_ADDR, 1);
}

/**
  * @brief  Writes one byte to the GYRO.
  * @param  pBuffer : pointer to the buffer  containing the data to be written to the GYRO.
  * @param  WriteAddr : GYRO's internal address to write to.
  * @param  NumByteToWrite: Number of bytes to write.
  * @retval None
  */
void L3GD20_Write(uint8_t* pBuffer, uint8_t WriteAddr, uint16_t NumByteToWrite)
{
  // Configure the MS bit:
  //  - When 0, the address will remain unchanged in multiple read/write commands.
  //  - When 1, the address will be auto incremented in multiple read/write commands.

  if(NumByteToWrite > 0x01) {
    WriteAddr |= (uint8_t)L3GD20_MULTIPLEBYTE_CMD;
  }

  // Set chip select Low at the start of the transmission
  BSP_GYRO_CS_LOW();

  // Send the Address of the indexed register
  HAL_SPI_Transmit(&hspi1, &WriteAddr, 1, SpiTimeout);

  // Send the data that will be written into the device (MSB First)
  HAL_SPI_Transmit(&hspi1, pBuffer, NumByteToWrite, SpiTimeout);

  // Set chip select High at the end of the transmission
  BSP_GYRO_CS_HIGH();
}

/**
  * @brief  Reads a block of data from the GYRO.
  * @param  pBuffer : pointer to the buffer that receives the data read from the GYRO.
  * @param  ReadAddr : GYRO's internal address to read from.
  * @param  NumByteToRead : number of bytes to read from the GYRO.
  * @retval None
  */
void L3GD20_Read(uint8_t* pBuffer, uint8_t ReadAddr, uint16_t NumByteToRead)
{
  if(NumByteToRead > 0x01) {
    ReadAddr |= (uint8_t)(L3GD20_READWRITE_CMD | L3GD20_MULTIPLEBYTE_CMD);
  } else {
    ReadAddr |= (uint8_t)L3GD20_READWRITE_CMD;
  }

  // Set chip select Low at the start of the transmission
  BSP_GYRO_CS_LOW();

  // Send the Address of the indexed register
  HAL_SPI_Transmit(&hspi1, &ReadAddr, 1, SpiTimeout);

  // Receive the data that will be read from the device (MSB First)
  HAL_SPI_Receive(&hspi1, pBuffer, NumByteToRead, SpiTimeout);

  // Set chip select High at the end of the transmission
  BSP_GYRO_CS_HIGH();
}
