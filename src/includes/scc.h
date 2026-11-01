/*
  Previous - scc.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#ifndef PREV_SCC_H
#define PREV_SCC_H

extern void SCC_ControlA_Read(void);
extern void SCC_ControlA_Write(void);
extern void SCC_ControlB_Read(void);
extern void SCC_ControlB_Write(void);
extern void SCC_DataA_Read(void);
extern void SCC_DataA_Write(void);
extern void SCC_DataB_Read(void);
extern void SCC_DataB_Write(void);
extern void SCC_Clock_Read(void);
extern void SCC_Clock_Write(void);

extern void SCC_Reset(void);

extern void SCC_IO_Handler(void);

/* Host side of the serial ports, see ConfigureParams.Serial */
extern void SCC_Init(void);
extern void SCC_UnInit(void);
/* Quit the emulator when the guest sends 'text' on a serial port */
extern void SCC_SetExitText(const char *text);

extern void scc_receive(int ch, uint8_t val);

#endif /* PREV_SCC_H */
