#ifndef __KBD_H__
#define __KBD_H__

#include <stdbool.h>

#define ALT_MOD     0x38
#define CTRL_MOD    0x1D

typedef uint8_t scancode_t;

bool kbd_state_shift(void);
bool kbd_state_ctrl(void);
bool kbd_state(scancode_t scan_id);

scancode_t kbd_wait_scan(bool release_too);

/************  events  ***************/

typedef void (*kbd_event_f)(scancode_t);

void kbd_set_onpress(kbd_event_f onpress);
void kbd_set_onrelease(kbd_event_f onrelease);


/************  buffer   **************/

scancode_t kbd_pop_scancode(void);
void kbd_buf_clear(void);

/************  layouts  *************/
struct kbd_layout;

// if layout is null, uses default
char translate_from_scan(const struct kbd_layout *layout, scancode_t scan_code);

char kbd_getchar(void);

void keyboard_irq();
void kbd_setup(void);

#endif //__KBD_H__
