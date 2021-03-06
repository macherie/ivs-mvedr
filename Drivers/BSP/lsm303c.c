
#include "main.h"
#include "lsm303c.h"
#include "bsp.h"

LSM303C_StatusTypedef MagStatus = MAG_ERROR;
int32_t MagInt[3] = {0, 0, 0};

void LSM303C_Handler(void)
{
  //Set the SPI in 3 wire mode for the get data communication
  BSP_SPI1_Init_1_Line();

  if (LSM303C_GetDataStatus() & 0x08) {
      LSM303C_ReadXYZMag();
  }

  //Reset the SPI in the default 4 wire mode
  BSP_SPI1_Init_2_Lines();
}

void LSM303C_ReadXYZMag()
{
  uint8_t tmpbuffer[6] = {0};
  int16_t RawData[3] = {0};
  uint8_t tmpreg = 0;
  int32_t sensitivity = 58;
  int i =0;

  LSM303C_Read(&tmpreg, LSM303C_CTRL_REG4_M, 1);

  LSM303C_Read(tmpbuffer, LSM303C_OUT_X_L_M, 6);

  // check in the control register 4 the data alignment (Big Endian or Little Endian)
  if(!(tmpreg & LSM303C_M_BLE_MSB)) {
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

  // divide by sensitivity
  for(i = 0; i < 3; i++)
  {
    MagInt[i] = (uint32_t)RawData[i] * sensitivity;
  }
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
  //Configure only the Magnetometer
  uint8_t retVal = MAG_ERROR;

  LSM303C_InitTypedef LSM303C_InitStruct;

  // Configure Mems LSM303C initialisation structure
  //Confgiure values for LSM303C_CTRL_REG1_M
  LSM303C_InitStruct.TempMode = LSM303C_M_TEMP_DISABLE; //Reg1
  LSM303C_InitStruct.XYOperativeMode = LSM303C_M_XY_ULTRAHIGH_PERF; //Reg1
  LSM303C_InitStruct.OutputDataRate = LSM303C_M_ODR_80_HZ; //Reg1

  //Confgiure values for LSM303C_CTRL_REG2_M
  LSM303C_InitStruct.FullScale = LSM303C_M_FULL_SCALE; //Reg2

  //Confgiure values for LSM303C_CTRL_REG3_M
  LSM303C_InitStruct.PowerMode = LSM303C_M_LOW_POWER_DISABLE; //Reg3
  LSM303C_InitStruct.InterfaceModeI2C = LSM303C_M_I2C_DISABLE; //Reg3
  LSM303C_InitStruct.InterfaceModeSPI = LSM303C_M_SPI_READ_WRITE; //Reg3
  LSM303C_InitStruct.SystemOperatingMode = LSM303C_M_SOM_CONT_CONV; //Reg3

  //Confgiure values for LSM303C_CTRL_REG4_M
  LSM303C_InitStruct.ZOperativeMode = LSM303C_M_Z_ULTRAHIGH_PERF; //Reg4
  LSM303C_InitStruct.Endianness = LSM303C_M_BLE_LSB; //Reg4

  //Confgiure values for LSM303C_CTRL_REG5_M
  LSM303C_InitStruct.BlockDataUpdate = LSM303C_M_BDU_CONTINUOUS; //Reg5

  //Init before readID, beacuse the init operation sets the proper I2C and SPI
  //communication paramters
  LSM303C_Init(&LSM303C_InitStruct);

  if(LSM303C_ReadID() == I_AM_LSM303C_M) {
    retVal = MAG_OK;
  }

  return retVal;
}

void LSM303C_Init(LSM303C_InitTypedef *LSM303C_InitStruct)
{
    uint8_t ctrl1 = 0x00, ctrl2 = 0x00, ctrl3 = 0x00, ctrl4 = 0x00, ctrl5 = 0x00;

    BSP_MagInit();

    // Configure MEMS: Temp sensor mode, xy operative mode, output data rate
    ctrl1 |= (uint8_t) (LSM303C_InitStruct->TempMode |
                        LSM303C_InitStruct->XYOperativeMode |
                        LSM303C_InitStruct->OutputDataRate);

    // Configure MEMS: Full scale +-16 Gauss
    ctrl2 |= (uint8_t) (LSM303C_InitStruct->FullScale);

    // Configure MEMS: low power mode, I2C, SPI modes, System operating mode
    ctrl3 |= (uint8_t) (LSM303C_InitStruct->PowerMode |
                        LSM303C_InitStruct->InterfaceModeI2C |
                        LSM303C_InitStruct->InterfaceModeSPI |
                        LSM303C_InitStruct->SystemOperatingMode);

    // Configure MEMS: Z axis operative mode, Output endianness
    ctrl4 |= (uint8_t) (LSM303C_InitStruct->ZOperativeMode |
                        LSM303C_InitStruct->Endianness);

    // Configure MEMS: Block data update
    ctrl5 |= (uint8_t) (LSM303C_InitStruct->BlockDataUpdate);

    // Write value to MEMS CTRL_REG1 register
    LSM303C_Write(&ctrl3, LSM303C_CTRL_REG3_M, 1);
    // Write value to MEMS CTRL_REG1 register
    LSM303C_Write(&ctrl1, LSM303C_CTRL_REG1_M, 1);
    // Write value to MEMS CTRL_REG1 register
    LSM303C_Write(&ctrl2, LSM303C_CTRL_REG2_M, 1);
    // Write value to MEMS CTRL_REG4 register
    LSM303C_Write(&ctrl4, LSM303C_CTRL_REG4_M, 1);
    // Write value to MEMS CTRL_REG4 register
    LSM303C_Write(&ctrl5, LSM303C_CTRL_REG5_M, 1);
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

  // Set chip select Low at the start of the transmission
  BSP_MAG_CS_LOW();

  // Send the Address of the indexed register
  HAL_SPI_Transmit(&hspi1, &WriteAddr, 1, SpiTimeout);

  // Send the data that will be written into the device (MSB First)
  while(NumByteToWrite >= 0x01)
  {
    HAL_SPI_Transmit(&hspi1, pBuffer, 1, SpiTimeout);
    NumByteToWrite--;
    pBuffer++;
  }

  // Set chip select High at the end of the transmission
  BSP_MAG_CS_HIGH();
}

void LSM303C_Read(uint8_t* pBuffer, uint8_t ReadAddr, uint16_t NumByteToRead)
{
  uint8_t dummy = LSM303C_DUMMY_BYTE;
  if(NumByteToRead > 0x01) {
    ReadAddr |= (uint8_t)(LSM303C_READWRITE_CMD | LSM303C_MULTIPLEBYTE_CMD);
  } else {
    ReadAddr |= (uint8_t)LSM303C_READWRITE_CMD;
  }

  // Set chip select Low at the start of the transmission
  BSP_MAG_CS_LOW();

  // Send the Address of the indexed register
  HAL_SPI_Transmit(&hspi1, &ReadAddr, 1, SpiTimeout);

  // Receive the data that will be read from the device (MSB First)
  while(NumByteToRead > 0x00)
  {
    HAL_SPI_TransmitReceive(&hspi1, &dummy, pBuffer, 1, SpiTimeout);

    NumByteToRead--;
    pBuffer++;
  }

  // Set chip select High at the end of the transmission
  BSP_MAG_CS_HIGH();
}