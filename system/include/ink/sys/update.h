/*
 * System upgrades (system services design §12): over the air or from the card, one kind of package, one check, one installation.
 * Nothing is written to a slot before the whole package has been checked, and the old system stays until the new one has
 * proved itself: if it does not, the bootloader goes back. The AI can find out and fetch; a person installs.
 */
#ifndef INK_SYS_UPDATE_H
#define INK_SYS_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ink_update_init(void);
/** At the end of boot, in the boot task. A new version proves itself here; a package on the card is looked for. */
void ink_update_boot_check(void);
/** More for a new version to prove, on the background executor (it has a minute in all). Up to 4; before the boot check. */
int ink_update_self_check_add(bool (*check)(void));

#ifdef __cplusplus
}
#endif
#endif
