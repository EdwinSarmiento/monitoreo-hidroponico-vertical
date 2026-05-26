#ifndef BOOT_RESET_H
#define BOOT_RESET_H

// Tarea en Core 0: sondea GPIO0 aunque el loop principal este bloqueado (I2C, pH, etc.)
void startBootResetMonitor();

void pollBootReset();

// Espera cediendo CPU; el sondeo del BOOT lo hace la tarea en Core 0
void delayWithBootPoll(unsigned long ms);

#endif
