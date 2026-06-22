```c
/*!
 * \file main.c
 * \brief Eeprom Click example
 *
 * # Description
 * This is a example which demonstrates the use of EEPROM Click board.
 *
 * The demo application is composed of two sections :
 *
 * ## Application Init 
 * Initializes peripherals and pins used by EEPROM Click.
 * Initializes SPI serial interface and puts a device to the initial state.
 *
 * ## Application Task
 * First page of memory block 1 will be written with data values starting from
 * 1 to 16. This memory page will be read by the user, to verify successfully
 * data writing. Data writing to memory will be protected upon memory writing,
 * and before memory reading.
 *
 * \author Nemanja Medakovic
 *
 */
// ------------------------------------------------------------------- INCLUDES

#include <string.h>
#include "board.h"
#include "log.h"
#include "eeprom.h"

#ifndef MIKROBUS_POSITION_EEPROM
    #define MIKROBUS_POSITION_EEPROM MIKROBUS_1
#endif


// ------------------------------------------------------------------ VARIABLES

static eeprom_t eeprom;
static log_t logger;

// ------------------------------------------------------ APPLICATION FUNCTIONS

void application_init( void )
{
    eeprom_cfg_t eeprom_cfg;
    log_cfg_t log_cfg;

    //  Click initialization.
    eeprom_cfg_setup( &eeprom_cfg );
    EEPROM_MAP_MIKROBUS( eeprom_cfg, MIKROBUS_POSITION_EEPROM );
    eeprom_init( &eeprom, &eeprom_cfg );

    /** 
     * Logger initialization.
     * Default baud rate: 115200
     * Default log level: LOG_LEVEL_DEBUG
     * @note If USB_UART_RX and USB_UART_TX 
     * are defined as HAL_PIN_NC, you will 
     * need to define them manually for log to work. 
     * See @b LOG_MAP_USB_UART macro definition for detailed explanation.
     */
    LOG_MAP_USB_UART( log_cfg );
    log_init( &logger, &log_cfg );
    log_info( &logger, "---- Application Init ----" );
}

void application_task( void )
{
    uint8_t transfer_data[ EEPROM_NBYTES_PAGE ];
    uint8_t read_buff[ EEPROM_NBYTES_PAGE ] = { 0 };
    uint8_t cnt;
    uint8_t tmp = EEPROM_BLOCK_ADDR_START;

    transfer_data[ EEPROM_BLOCK_ADDR_START ] = 1;

    for (cnt = EEPROM_BLOCK_ADDR_START + 1; cnt < EEPROM_NBYTES_PAGE; cnt++)
    {
        transfer_data[ cnt ] = transfer_data[ cnt - 1 ] + 1;
    }

    log_printf( &logger, "---- Writing data to EEPROM ----\r\n" );
    eeprom_write_enable( &eeprom );
    eeprom_write_page( &eeprom, tmp, transfer_data );
    eeprom_write_protect( &eeprom );
    log_printf( &logger, "---- Write complete ----\r\n" );

    Delay_ms ( 1000 );
    memset( transfer_data, 0, sizeof(transfer_data) );

    log_printf( &logger, "---- Reading data from EEPROM ----\r\n" );
    eeprom_read_sequential( &eeprom, EEPROM_BLOCK_ADDR_START, EEPROM_NBYTES_PAGE, read_buff );

    log_printf( &logger, "Read values: " );
    for (cnt = EEPROM_BLOCK_ADDR_START; cnt < EEPROM_NBYTES_PAGE; cnt++)
    {
        log_printf( &logger, " %u", ( uint16_t )read_buff[ cnt ] );
        Delay_ms ( 300 );
    }
    log_printf( &logger, "\r\n" );

    // Verification: check if read data matches what was expected (1,2,3...16)
    uint8_t success = 1;
    uint8_t expected = 1;
    for (cnt = EEPROM_BLOCK_ADDR_START; cnt < EEPROM_NBYTES_PAGE; cnt++)
    {
        if ( read_buff[ cnt ] != expected )
        {
            success = 0;
        }
        expected++;
    }

    if ( success )
    {
        log_printf( &logger, "EEPROM TEST: SUCCESS - Data matches!\r\n\r\n" );
    }
    else
    {
        log_printf( &logger, "EEPROM TEST: FAILED - Data mismatch!\r\n\r\n" );
    }
}

int main ( void ) 
{
    /* Do not remove this line or clock might not be set correctly. */
    #ifdef PREINIT_SUPPORTED
    preinit();
    #endif
    
    application_init( );
    
    for ( ; ; ) 
    {
        application_task( );
    }

    return 0;
}

// ------------------------------------------------------------------------ END

```