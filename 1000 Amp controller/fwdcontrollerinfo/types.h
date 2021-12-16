/*********************************************************************
* File Name     : types.h 
* Description   : ISO (Posix) typedef file for pic
                : Misra Rule 6.3 (advisory)                      

* Revision      : 1.0 
* Date          : 21/08/05 
*********************************************************************/

#ifndef TYPES_H
#define TYPES_H
            
/*--- Standard type definitions. ---*/

/* Plain char, only to be used for the storage and use of character values. Misra Rule 6.1 (Required) */

typedef unsigned char   uint8_t;    /* unsigned 8 bit type definition */
typedef signed char     int8_t;     /* signed 8 bit type definition */
typedef unsigned int    uint16_t;   /* unsigned 16 bit type definition */
typedef signed int      int16_t;    /* signed 16 bit type definition */
typedef unsigned long   uint32_t;   /* unsigned 32 bit type definition */
typedef signed long     int32_t;    /* signed 32 bit type definition */
typedef unsigned int    Bool;       /* Bool type definition */

/*--- Standard constant definition. ---*/

#define False ((uint16_t)0x0000U)
#define True  ((uint16_t)0x0001U)

/*--- End of file. ---*/

#endif


