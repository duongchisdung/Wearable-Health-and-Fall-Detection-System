#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// $[CMU]
// [CMU]$

// $[LFXO]
// [LFXO]$

// $[KEYSCAN]
// [KEYSCAN]$

// $[PRS.ASYNCH0]
// [PRS.ASYNCH0]$

// $[PRS.ASYNCH1]
// [PRS.ASYNCH1]$

// $[PRS.ASYNCH2]
// [PRS.ASYNCH2]$

// $[PRS.ASYNCH3]
// [PRS.ASYNCH3]$

// $[PRS.ASYNCH4]
// [PRS.ASYNCH4]$

// $[PRS.ASYNCH5]
// [PRS.ASYNCH5]$

// $[PRS.ASYNCH6]
// [PRS.ASYNCH6]$

// $[PRS.ASYNCH7]
// [PRS.ASYNCH7]$

// $[PRS.ASYNCH8]
// [PRS.ASYNCH8]$

// $[PRS.ASYNCH9]
// [PRS.ASYNCH9]$

// $[PRS.ASYNCH10]
// [PRS.ASYNCH10]$

// $[PRS.ASYNCH11]
// [PRS.ASYNCH11]$

// $[PRS.ASYNCH12]
// [PRS.ASYNCH12]$

// $[PRS.ASYNCH13]
// [PRS.ASYNCH13]$

// $[PRS.ASYNCH14]
// [PRS.ASYNCH14]$

// $[PRS.ASYNCH15]
// [PRS.ASYNCH15]$

// $[PRS.SYNCH0]
// [PRS.SYNCH0]$

// $[PRS.SYNCH1]
// [PRS.SYNCH1]$

// $[PRS.SYNCH2]
// [PRS.SYNCH2]$

// $[PRS.SYNCH3]
// [PRS.SYNCH3]$

// $[GPIO]
// GPIO SWV on PA03
#ifndef GPIO_SWV_PORT                           
#define GPIO_SWV_PORT                            SL_GPIO_PORT_A
#endif
#ifndef GPIO_SWV_PIN                            
#define GPIO_SWV_PIN                             3
#endif

// [GPIO]$

// $[TIMER0]
// [TIMER0]$

// $[TIMER1]
// [TIMER1]$

// $[TIMER2]
// [TIMER2]$

// $[TIMER3]
// TIMER3 CC0 on PC12
#ifndef TIMER3_CC0_PORT                         
#define TIMER3_CC0_PORT                          SL_GPIO_PORT_C
#endif
#ifndef TIMER3_CC0_PIN                          
#define TIMER3_CC0_PIN                           12
#endif

// [TIMER3]$

// $[TIMER4]
// [TIMER4]$

// $[TIMER5]
// [TIMER5]$

// $[TIMER6]
// [TIMER6]$

// $[TIMER7]
// [TIMER7]$

// $[TIMER8]
// [TIMER8]$

// $[TIMER9]
// [TIMER9]$

// $[EUSART1]
// EUSART1 CS on PC00
#ifndef EUSART1_CS_PORT                         
#define EUSART1_CS_PORT                          SL_GPIO_PORT_C
#endif
#ifndef EUSART1_CS_PIN                          
#define EUSART1_CS_PIN                           0
#endif

// EUSART1 RX on PC02
#ifndef EUSART1_RX_PORT                         
#define EUSART1_RX_PORT                          SL_GPIO_PORT_C
#endif
#ifndef EUSART1_RX_PIN                          
#define EUSART1_RX_PIN                           2
#endif

// EUSART1 SCLK on PC01
#ifndef EUSART1_SCLK_PORT                       
#define EUSART1_SCLK_PORT                        SL_GPIO_PORT_C
#endif
#ifndef EUSART1_SCLK_PIN                        
#define EUSART1_SCLK_PIN                         1
#endif

// EUSART1 TX on PC03
#ifndef EUSART1_TX_PORT                         
#define EUSART1_TX_PORT                          SL_GPIO_PORT_C
#endif
#ifndef EUSART1_TX_PIN                          
#define EUSART1_TX_PIN                           3
#endif

// [EUSART1]$

// $[EUSART2]
// EUSART2 RX on PD09
#ifndef EUSART2_RX_PORT                         
#define EUSART2_RX_PORT                          SL_GPIO_PORT_D
#endif
#ifndef EUSART2_RX_PIN                          
#define EUSART2_RX_PIN                           9
#endif

// EUSART2 TX on PD08
#ifndef EUSART2_TX_PORT                         
#define EUSART2_TX_PORT                          SL_GPIO_PORT_D
#endif
#ifndef EUSART2_TX_PIN                          
#define EUSART2_TX_PIN                           8
#endif

// [EUSART2]$

// $[EUSART3]
// [EUSART3]$

// $[USART0]
// USART0 CTS on PB07
#ifndef USART0_CTS_PORT                         
#define USART0_CTS_PORT                          SL_GPIO_PORT_B
#endif
#ifndef USART0_CTS_PIN                          
#define USART0_CTS_PIN                           7
#endif

// USART0 RTS on PB08
#ifndef USART0_RTS_PORT                         
#define USART0_RTS_PORT                          SL_GPIO_PORT_B
#endif
#ifndef USART0_RTS_PIN                          
#define USART0_RTS_PIN                           8
#endif

// USART0 RX on PB06
#ifndef USART0_RX_PORT                          
#define USART0_RX_PORT                           SL_GPIO_PORT_B
#endif
#ifndef USART0_RX_PIN                           
#define USART0_RX_PIN                            6
#endif

// USART0 TX on PB05
#ifndef USART0_TX_PORT                          
#define USART0_TX_PORT                           SL_GPIO_PORT_B
#endif
#ifndef USART0_TX_PIN                           
#define USART0_TX_PIN                            5
#endif

// [USART0]$

// $[USART1]
// [USART1]$

// $[USART2]
// [USART2]$

// $[I2C1]
// I2C1 SCL on PC11
#ifndef I2C1_SCL_PORT                           
#define I2C1_SCL_PORT                            SL_GPIO_PORT_C
#endif
#ifndef I2C1_SCL_PIN                            
#define I2C1_SCL_PIN                             11
#endif

// I2C1 SDA on PC10
#ifndef I2C1_SDA_PORT                           
#define I2C1_SDA_PORT                            SL_GPIO_PORT_C
#endif
#ifndef I2C1_SDA_PIN                            
#define I2C1_SDA_PIN                             10
#endif

// [I2C1]$

// $[I2C2]
// [I2C2]$

// $[I2C3]
// [I2C3]$

// $[LCD]
// [LCD]$

// $[LETIMER0]
// [LETIMER0]$

// $[IADC0]
// [IADC0]$

// $[ACMP0]
// [ACMP0]$

// $[ACMP1]
// [ACMP1]$

// $[VDAC0]
// [VDAC0]$

// $[VDAC1]
// [VDAC1]$

// $[PCNT0]
// [PCNT0]$

// $[HFXO0]
// [HFXO0]$

// $[I2C0]
// I2C0 SCL on PC04
#ifndef I2C0_SCL_PORT                           
#define I2C0_SCL_PORT                            SL_GPIO_PORT_C
#endif
#ifndef I2C0_SCL_PIN                            
#define I2C0_SCL_PIN                             4
#endif

// I2C0 SDA on PC05
#ifndef I2C0_SDA_PORT                           
#define I2C0_SDA_PORT                            SL_GPIO_PORT_C
#endif
#ifndef I2C0_SDA_PIN                            
#define I2C0_SDA_PIN                             5
#endif

// [I2C0]$

// $[EUSART0]
// EUSART0 CTS on PA09
#ifndef EUSART0_CTS_PORT                        
#define EUSART0_CTS_PORT                         SL_GPIO_PORT_A
#endif
#ifndef EUSART0_CTS_PIN                         
#define EUSART0_CTS_PIN                          9
#endif

// EUSART0 RTS on PA08
#ifndef EUSART0_RTS_PORT                        
#define EUSART0_RTS_PORT                         SL_GPIO_PORT_A
#endif
#ifndef EUSART0_RTS_PIN                         
#define EUSART0_RTS_PIN                          8
#endif

// EUSART0 RX on PA06
#ifndef EUSART0_RX_PORT                         
#define EUSART0_RX_PORT                          SL_GPIO_PORT_A
#endif
#ifndef EUSART0_RX_PIN                          
#define EUSART0_RX_PIN                           6
#endif

// EUSART0 TX on PA05
#ifndef EUSART0_TX_PORT                         
#define EUSART0_TX_PORT                          SL_GPIO_PORT_A
#endif
#ifndef EUSART0_TX_PIN                          
#define EUSART0_TX_PIN                           5
#endif

// [EUSART0]$

// $[PTI]
// [PTI]$

// $[MODEM]
// [MODEM]$

// $[CUSTOM_PIN_NAME]
#ifndef _PORT                                   
#define _PORT                                    SL_GPIO_PORT_A
#endif
#ifndef _PIN                                    
#define _PIN                                     0
#endif

#ifndef 12_PORT                                 
#define 12_PORT                                  SL_GPIO_PORT_B
#endif
#ifndef 12_PIN                                  
#define 12_PIN                                   5
#endif

#ifndef 14_PORT                                 
#define 14_PORT                                  SL_GPIO_PORT_B
#endif
#ifndef 14_PIN                                  
#define 14_PIN                                   6
#endif

#ifndef 15_PORT                                 
#define 15_PORT                                  SL_GPIO_PORT_C
#endif
#ifndef 15_PIN                                  
#define 15_PIN                                   4
#endif

#ifndef 16_PORT                                 
#define 16_PORT                                  SL_GPIO_PORT_C
#endif
#ifndef 16_PIN                                  
#define 16_PIN                                   5
#endif

#ifndef 10_PORT                                 
#define 10_PORT                                  SL_GPIO_PORT_C
#endif
#ifndef 10_PIN                                  
#define 10_PIN                                   10
#endif

#ifndef 8_PORT                                  
#define 8_PORT                                   SL_GPIO_PORT_C
#endif
#ifndef 8_PIN                                   
#define 8_PIN                                    11
#endif

#ifndef 6_PORT                                  
#define 6_PORT                                   SL_GPIO_PORT_C
#endif
#ifndef 6_PIN                                   
#define 6_PIN                                    12
#endif

#ifndef 11_PORT                                 
#define 11_PORT                                  SL_GPIO_PORT_D
#endif
#ifndef 11_PIN                                  
#define 11_PIN                                   8
#endif

#ifndef 13_PORT                                 
#define 13_PORT                                  SL_GPIO_PORT_D
#endif
#ifndef 13_PIN                                  
#define 13_PIN                                   9
#endif

// [CUSTOM_PIN_NAME]$

#endif // PIN_CONFIG_H

