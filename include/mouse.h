#ifndef MOUSE_H
#define MOUSE_H

/* Mouse state structure */
typedef struct {
    int x, y;                    /* Current position */
    int delta_x, delta_y;        /* Movement deltas */
    unsigned char buttons;       /* Button states (bit 0=left, bit 1=right, bit 2=middle) */
} mouse_state_t;

/* Mouse function prototypes */
void init_mouse(void);
void mouse_handler(void);
mouse_state_t* get_mouse_state(void);

/* Mouse button constants */
#define MOUSE_LEFT_BUTTON   (1 << 0)
#define MOUSE_RIGHT_BUTTON  (1 << 1)
#define MOUSE_MIDDLE_BUTTON (1 << 2)

#endif