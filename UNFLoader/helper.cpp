#include "main.h"
#include "helper.h"
#include "term.h"
#include "device.h"
#include "debug.h"
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef LINUX
    #include "Include/curses.h"
    #include "Include/curspriv.h"
    #include "Include/panel.h"
#else
    #include <curses.h>
    #include <sys/time.h>
    #include <termios.h>
#endif


/*********************************
             Globals
*********************************/

// Useful constants for converting between enums and strings
const char* cart_strings[] = {"64Drive HW1", "64Drive HW2", "EverDrive", "SC64"}; // In order of the CartType enums
const int   cart_strcount = sizeof(cart_strings)/sizeof(cart_strings[0]);
const char* cic_strings[] = {"6101", "6102", "7101", "7102", "X103", "X105", "X106", "5101"}; // In order of the CICType enums
const int   cic_strcount = sizeof(cic_strings)/sizeof(cic_strings[0]);
const char* save_strings[] = {"EEPROM 4Kbit", "EEPROM 16Kbit", "SRAM 256Kbit", "FlashRAM 1Mbit", "SRAM 768Kbit", "FlashRAM 1Mbit (PokeStdm2)"}; // In order of the SaveType enums
const int   save_strcount = sizeof(save_strings)/sizeof(save_strings[0]);


/*==============================
    terminate
    Stops the program and prints "Press any key to continue..."
    @param A string to print
    @param Variadic arguments to print as well
==============================*/

void terminate(const char* reason, ...)
{
    va_list args;
    va_start(args, reason);

    // Print why we're ending
    if (reason != NULL && strcmp(reason, ""))
    {
        char temp[255];
        vsprintf(temp, reason, args);
        log_colored("Error: %s", CRDEF_ERROR, temp);
    }
    log_colored("\n", CRDEF_ERROR);
    va_end(args);

    // Close output debug file if it exists
    if (debug_getdebugout() != NULL)
        debug_closedebugout();

    // Close the flashcart if it's open
    if (device_isopen())
        device_close();

    // Pause the program
    log_colored("Press any key to continue...\n", CRDEF_INPUT);
    if (!term_isusingcurses())
    {
        #ifndef LINUX
            system("pause > nul");
        #else
            struct termios info, orig;
            tcgetattr(0, &info);
            tcgetattr(0, &orig);
            info.c_lflag &= ~(ICANON | ECHO);
            info.c_cc[VMIN] = 1;
            info.c_cc[VTIME] = 0;
            tcsetattr(0, TCSANOW, &info);
            getchar();
            tcsetattr(0, TCSANOW, &orig);
        #endif
    }
    else
        getch();

    // End
    global_terminating = true;
    term_end();
    exit(-1);
}

/*==============================
    progressbar_draw
    Draws a fancy progress bar
    @param The text to print before the progress bar
    @param The color to draw the progress bar with
    @param The percentage of completion, from 0 to 1
==============================*/

void progressbar_draw(const char* text, short color, float percent)
{
    int i;
    int prog_size = 16;
    int blocks_done = (int)(percent*prog_size);

    // Print the head of the progress bar
    log_replace("%s [", color, text);

    // Draw the progress bar itself
    for(i=0; i<blocks_done; i++) 
    {
        #ifndef LINUX
            log_colored("%c", color, 219);
        #else
            log_colored("\xe2\x96\x88", color);
        #endif
    }
    for(; i<prog_size; i++) 
    {
        #ifndef LINUX
            log_colored("%c", color, 176);
        #else
            log_colored("\xe2\x96\x91", color);
        #endif
    }

    // Print the butt of the progress bar
    log_colored("] %.02f%%\n", color, percent*100.0f);
}


/*==============================
    time_miliseconds
    Retrieves the current system
    time in miliseconds.
    Needed because clock() wasn't
    working properly???
    @return The time in miliseconds
==============================*/

uint64_t time_miliseconds()
{
    #ifndef LINUX
        SYSTEMTIME st;
        GetSystemTime(&st);
        return (uint64_t)st.wMilliseconds;
    #else
        struct timeval tv;
        gettimeofday(&tv,NULL);
        return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
    #endif
}


/*==============================
    cart_strtotype
    Reads a string and converts it 
    to a valid CartType enum
    @param  The string with the cart 
            to parse
    @return The CartType enum
==============================*/

CartType cart_strtotype(const char* cartstring)
{
    // If the cart string is a single number, then it's pretty easy to get the cart enum
    if (cartstring[0] >= ('0'+((int)CART_64DRIVE1)) && cartstring[0] <= ('0'+((int)CART_SC64)) && cartstring[1] == '\0')
        return (CartType)(cartstring[0]-'0');

    // Check if the user, for some reason, wrote the entire CIC string out
    for (int i=0; i<cart_strcount; i++)
        if (!strcmp(cart_strings[i], cartstring))
            return (CartType)(i+1);

    // Otherwise, stop
    terminate("Unknown flashcart type '%s'", cartstring);
    return CART_NONE; // Doesn't actually return since terminate stops the program first
}


/*==============================
    cart_typetostr
    Reads a CartType enum and converts it
    to a nice string. Assumes a non-NONE
    CartType is given!
    @param  The enum with the cart 
            to convert
    @return The cart name stringified
==============================*/

const char* cart_typetostr(CartType cartenum)
{
    return cart_strings[(int)cartenum-1];
}


/*==============================
    cic_strtotype
    Reads a string and converts it 
    to a valid CICType enum
    @param  The string with the CIC 
            to parse
    @return The CICType enum
==============================*/

CICType cic_strtotype(const char* cicstring)
{
    // If the CIC string is a single number, then it's pretty easy to get the CIC enum
    if (cicstring[0] >= ('0'+((int)CIC_6101)) && cicstring[0] <= ('0'+((int)CIC_5101)) && cicstring[1] == '\0')
    {
        return (CICType)(cicstring[0]-'0');
    }

    // Check if the user, for some reason, wrote the entire CIC string out
    for (int i=0; i<cic_strcount; i++)
        if (!strcmp(cic_strings[i], cicstring))
            return (CICType)i;

    // Otherwise, stop
    terminate("Unknown CIC '%s'", cicstring);
    return CIC_NONE; // Doesn't actually return since terminate stops the program first
}


/*==============================
    cic_typetostr
    Reads a CICType enum and converts it
    to a nice string. Assumes a non-NONE
    CICType is given!
    @param  The enum with the CIC 
            to convert
    @return The CIC name stringified
==============================*/

const char* cic_typetostr(CICType cicenum)
{
    return cic_strings[(int)cicenum];
}


/*==============================
    save_strtotype
    Reads a string and converts it 
    to a valid SaveType enum
    @param  The string with the save type
            to parse
    @return The SaveType enum
==============================*/

SaveType save_strtotype(const char* savestring)
{
    // If the save string is a single number, then it's pretty easy to get the save enum
    if (savestring[0] >= ('0'+((int)SAVE_EEPROM4K)) && savestring[0] <= ('0'+((int)SAVE_FLASHRAMPKMN)) && savestring[1] == '\0')
        return (SaveType)(savestring[0]-'0');

    // Check if the user, for some reason, wrote the entire save string out
    for (int i=0; i<save_strcount; i++)
        if (!strcmp(save_strings[i], savestring))
            return (SaveType)(i+1);

    // Otherwise, stop
    terminate("Unknown save type '%s'", savestring);
    return SAVE_NONE; // Doesn't actually return since terminate stops the program first
}


/*==============================
    save_typetostr
    Reads a SaveType enum and converts it
    to a nice string. Assumes a non-NONE
    SaveType is given!
    @param  The enum with the save type
            to convert
    @return The save type name stringified
==============================*/

const char* save_typetostr(SaveType saveenum)
{
    return save_strings[(int)saveenum-1];
}


/*==============================
    file_lastmodtime
    Gets the last modification time of
    a file. This is usually done via stat,
    but it is broken on WinXP:
    https://stackoverflow.com/questions/32452777/visual-c-2015-express-stat-not-working-on-windows-xp
    @param  Path to the file to check the 
            timestamp of
    @return The file modification time
==============================*/

time_t file_lastmodtime(const char* path)
{
    struct stat finfo;
    #ifndef LINUX
        LARGE_INTEGER lt;
        WIN32_FILE_ATTRIBUTE_DATA fdata;
        GetFileAttributesExA(path, GetFileExInfoStandard, &fdata);
        lt.LowPart = fdata.ftLastWriteTime.dwLowDateTime;
        lt.HighPart = (long)fdata.ftLastWriteTime.dwHighDateTime;
        finfo.st_mtime = (time_t)(lt.QuadPart*1e-7);
    #else
        stat(path, &finfo);
    #endif
    return finfo.st_mtime;
}


/*==============================
    gen_filename
    Generates a unique ending for a filename
    Remember to free the memory when finished!
    @param  The filename
    @param  The file extension
    @return The unique string
==============================*/

#define DATESIZE 64//7*2+1
char* gen_filename(const char* filename, const char* fileext)
{
    static int increment = 0;
    static int lasttime = 0;
    char* finalname = NULL;
    char* extraname = NULL;
    int curtime = 0;
    time_t t = time(NULL);
    struct tm tm;
    struct tm* tmp = &tm;

    // Get the time
    tm = *localtime(&t);
    curtime = tmp->tm_hour*60*60+tmp->tm_min*60+tmp->tm_sec;

    // Increment the last value if two files were created at the same second
    if (lasttime != curtime)
    {
        increment = 0;
        lasttime = curtime;
    }
    else
        increment++;

    // Generate the unique string
    extraname = (char*)malloc(DATESIZE);
    if (extraname == NULL)
        return NULL;
    sprintf(extraname, "%02d%02d%02d%02d%02d%02d%02d", 
                 (tmp->tm_year+1900)%100, tmp->tm_mon+1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, increment%100);

    // Generate the final name
    if (debug_getbinaryout() != NULL)
    {
        finalname = (char*)calloc(snprintf(NULL, 0, "%s%s-%s.%s", debug_getbinaryout(), filename, extraname, fileext) + 1, 1);
        if (finalname == NULL)
            return NULL;
        sprintf(finalname, "%s%s-%s.%s", debug_getbinaryout(), filename, extraname, fileext);
    }
    else
    {
        finalname = (char*)calloc(snprintf(NULL, 0, "%s-%s.%s", filename, extraname, fileext) + 1, 1);
        if (finalname == NULL)
            return NULL;
        sprintf(finalname, "%s-%s.%s", filename, extraname, fileext);
    }
    free(extraname);
    return finalname;
}



/*==============================
    handle_deviceerror
    Stops the program with a useful
    error message if the deive encounters
    an error
==============================*/

void handle_deviceerror(DeviceError err)
{
    switch(err)
    {
        case DEVICEERR_USBBUSY:
            terminate("USB Device not ready.");
            break;
        case DEVICEERR_NODEVICES:
            terminate("No FTDI USB devices found.");
            break;
        case DEVICEERR_CARTFINDFAIL:
            if (device_getcart() == CART_NONE)
            {
                #ifndef LINUX
                    terminate("No flashcart detected");
                #else
                    terminate("No flashcart detected. Are you running sudo?");
                #endif
            }
            else
                terminate("Requested flashcart not detected.");
            break;
        case DEVICEERR_CANTOPEN:
            terminate("Could not open USB device.");
            break;
        case DEVICEERR_RESETFAIL:
            terminate("Unable to reset USB device.");
            break;
        case DEVICEERR_RESETPORTFAIL:
            terminate("Unable to reset USB port.");
            break;
        case DEVICEERR_TIMEOUTSETFAIL:
            terminate("Unable to set flashcart timeouts.");
            break;
        case DEVICEERR_PURGEFAIL:
            terminate("Unable to purge USB contents.");
            break;
        case DEVICEERR_READFAIL:
            terminate("Unable to read from flashcart.");
            break;
        case DEVICEERR_WRITEFAIL:
            terminate("Unable to write to flashcart.");
            break;
        case DEVICEERR_WRITEZERO:
            terminate("Zero bytes were written to flashcart.");
            break;
        case DEVICEERR_CLOSEFAIL:
            terminate("Unable to close flashcart.");
            break;
        case DEVICEERR_BITMODEFAIL_RESET:
            terminate("Unable to set bitmode %d.", FT_BITMODE_RESET);
            break;
        case DEVICEERR_BITMODEFAIL_SYNCFIFO:
            terminate("Unable to set bitmode %d.", FT_BITMODE_SYNC_FIFO);
            break;
        case DEVICEERR_SETDTRFAIL:
            terminate("Unable to set DTR line.");
            break;
        case DEVICEERR_CLEARDTRFAIL:
            terminate("Unable to clear DTR line.");
            break;
        case DEVICEERR_TXREPLYMISMATCH:
            terminate("Actual bytes written amount is different than desired.");
            break;
        case DEVICEERR_READCOMPSIGFAIL:
            terminate("Unable to read completion signal.");
            break;
        case DEVICEERR_NOCOMPSIG:
            terminate("Did not receive completion signal.");
            break;
        case DEVICEERR_READPACKSIZEFAIL:
            terminate("Unable to read packet size.");
            break;
        case DEVICEERR_BADPACKSIZE:
            terminate("Wrong read packet size.");
            break;
        case DEVICEERR_MALLOCFAIL:
            terminate("Malloc failure.");
            return;
        case DEVICEERR_UPLOADCANCELLED:
            log_replace("Upload cancelled by the user.\n", CRDEF_PROGRAM);
            return;
        case DEVICEERR_TIMEOUT:
            terminate("Flashcart timed out.");
            break;
        case DEVICEERR_64D_8303USB:
            terminate("The 8303 CIC is not supported through USB.");
            break;
        case DEVICEERR_64D_BADCMP:
            terminate("Received bad CMP signal.");
            break;
        case DEVICEERR_64D_CANTDEBUG:
            terminate("Please upgrade to firmware 2.05 or higher to access USB debugging.");
            break;
        case DEVICEERR_64D_BADDMA:
            terminate("Unexpected DMA header.");
            break;
        case DEVICEERR_SC64_CTRLRESETFAIL:
            terminate("Couldn't perform SC64 controller reset.");
            break;
        case DEVICEERR_SC64_CTRLRELEASEFAIL:
            terminate("Couldn't release SC64 controller reset.");
            break;
        case DEVICEERR_SC64_FIRMWARECHECKFAIL:
            terminate("Couldn't get SC64 firmware version.");
            break;
        case DEVICEERR_SC64_FIRMWAREUNKNOWN:
            terminate("Unknown SC64 firmware version.");
            break;
        default:
            if (err != DEVICEERR_OK && err != DEVICEERR_NOTCART)
                log_colored("Unhandled device error '%d'.\n", CRDEF_ERROR, err);
            return;
    }
}