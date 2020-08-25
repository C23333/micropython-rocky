/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2016 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>

#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "fsl_iomuxc.h"
#include "led.h"
#include "pin.h"
#include "genhdr/pins.h"
#include "aia_cmm/cfg_mux_mgr.h"

#if defined(MICROPY_HW_LED1)

/// \moduleref pyb
/// \class LED - LED object
///
/// The LED object controls an individual LED (Light Emitting Diode).

// the default is that LEDs are not inverted, and pin driven high turns them on
#ifndef MICROPY_HW_LED_INVERTED
#define MICROPY_HW_LED_INVERTED (0)
#endif

typedef struct _pyb_led_obj_t {
    mp_obj_base_t base;
    mp_uint_t led_id;
    const pin_obj_t *led_pin;
    MuxItem_t mux;
} pyb_led_obj_t;

pyb_led_obj_t pyb_led_obj[] = {
    {{&pyb_led_type}, 1, &MICROPY_HW_LED1},
#if defined(MICROPY_HW_LED2)
    {{&pyb_led_type}, 2, &MICROPY_HW_LED2},
#if defined(MICROPY_HW_LED3)
    {{&pyb_led_type}, 3, &MICROPY_HW_LED3},
#if defined(MICROPY_HW_LED4)
    {{&pyb_led_type}, 4, &MICROPY_HW_LED4},
#endif
#endif
#endif
};
#define NUM_LEDS MP_ARRAY_SIZE(pyb_led_obj)

void led_init(void) {
    /* Turn off LEDs and initialize */
    for (int led = 0; led < NUM_LEDS; led++) {
        const pin_obj_t *led_pin = pyb_led_obj[led].led_pin;
		mp_hal_ConfigGPIO(led_pin, GPIO_MODE_OUTPUT_PP, 1);    }
}


#if defined(MICROPY_HW_LED1_PWM) \
    || defined(MICROPY_HW_LED2_PWM) \
    || defined(MICROPY_HW_LED3_PWM) \
    || defined(MICROPY_HW_LED4_PWM)

// The following is semi-generic code to control LEDs using PWM.
// It currently supports TIM1, TIM2 and TIM3, channels 1-4.
// Configure by defining the relevant MICROPY_HW_LEDx_PWM macros in mpconfigboard.h.
// If they are not defined then PWM will not be available for that LED.

#define LED_PWM_ENABLED (1)

#ifndef MICROPY_HW_LED1_PWM
#define MICROPY_HW_LED1_PWM { NULL, 0, 0, 0 }
#endif
#ifndef MICROPY_HW_LED2_PWM
#define MICROPY_HW_LED2_PWM { NULL, 0, 0, 0 }
#endif
#ifndef MICROPY_HW_LED3_PWM
#define MICROPY_HW_LED3_PWM { NULL, 0, 0, 0 }
#endif
#ifndef MICROPY_HW_LED4_PWM
#define MICROPY_HW_LED4_PWM { NULL, 0, 0, 0 }
#endif

#define LED_PWM_TIM_PERIOD (10000) // TIM runs at 1MHz and fires every 10ms

typedef struct _led_pwm_config_t {
	union{
		GPT_Type *pGPT;
		TMR_Type *pTMR;
		PWM_Type *pPWM;
	};
	uint8_t tim_type; // 0 = GPT, 1 = TMR, 2 = (Flex)PWM
    uint8_t tim_id;
    uint8_t tim_channel;	// encoded channel, different timer has different interpretation
    uint8_t alt_func;	// alt func index of pin mux
} led_pwm_config_t;

const led_pwm_config_t led_pwm_config[] = {
    MICROPY_HW_LED1_PWM,
    MICROPY_HW_LED2_PWM,
    MICROPY_HW_LED3_PWM,
    MICROPY_HW_LED4_PWM,
};

STATIC uint8_t led_pwm_state = 0;

static inline bool led_pwm_is_enabled(int led) {
    return (led_pwm_state & (1 << led)) != 0;
}

// this function has a large stack so it should not be inlined
void led_pwm_init(int led) __attribute__((noinline));
void led_pwm_init(int led) {
    // const pin_obj_t *led_pin = pyb_led_obj[led - 1].led_pin;

    // >>> rocky: todos
	// <<<
    // indicate that this LED is using PWM
    led_pwm_state |= 1 << led;
}

STATIC void led_pwm_deinit(int led) {
    // make the LED's pin a standard GPIO output pin
    const pin_obj_t *led_pin = pyb_led_obj[led - 1].led_pin;
    led_pin = led_pin;
}

#else
#define LED_PWM_ENABLED (0)
#endif

void led_state(pyb_led_t led, int state) {
    if (led < 1 || led > NUM_LEDS) {
        return;
    }

    const pin_obj_t *led_pin = pyb_led_obj[led - 1].led_pin;
    //printf("led_state(%d,%d)\n", led, state);
	#ifdef BOARD_OMVRT1
	if (led == 4)
		state = !state;	// IR led is inverted
	#endif
    if (state == 0) {
        // turn LED off
        MICROPY_HW_LED_OFF(led_pin);
    } else {
        // turn LED on
        MICROPY_HW_LED_ON(led_pin);
    }

    #if LED_PWM_ENABLED
    if (led_pwm_is_enabled(led)) {
        led_pwm_deinit(led);
    }
    #endif
}

void led_toggle(pyb_led_t led) {
    if (led < 1 || led > NUM_LEDS) {
        return;
    }

    #if LED_PWM_ENABLED
    if (led_pwm_is_enabled(led)) {
        // if PWM is enabled then LED has non-zero intensity, so turn it off
        led_state(led, 0);
        return;
    }
    #endif

    const pin_obj_t *led_pin = pyb_led_obj[led - 1].led_pin;
	mp_hal_pin_toggle(led_pin);
}

int led_get_intensity(pyb_led_t led) {
    if (led < 1 || led > NUM_LEDS) {
        return 0;
    }

    #if LED_PWM_ENABLED
    if (led_pwm_is_enabled(led)) {
        // const led_pwm_config_t *pwm_cfg = &led_pwm_config[led - 1];
    }
    #endif
    // const pin_obj_t *led_pin = pyb_led_obj[led - 1].led_pin;
	return 0;
}

void led_set_intensity(pyb_led_t led, mp_int_t intensity) {
    #if LED_PWM_ENABLED
    if (intensity > 0 && intensity < 255) {
//        const led_pwm_config_t *pwm_cfg = &led_pwm_config[led - 1];
    }
    #endif

    // intensity not supported for this LED; just turn it on/off
    led_state(led, intensity > 0);
}

void led_debug(int n, int delay) {
    led_state(1, n & 1);
    // led_state(2, n & 2);
    // led_state(3, n & 4);
    // led_state(4, n & 8);
    mp_hal_delay_ms(delay);
}

/******************************************************************************/
/* Micro Python bindings                                                      */

void led_obj_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    pyb_led_obj_t *self = self_in;
    mp_printf(print, "LED(%lu)", self->led_id);
}

/// \classmethod \constructor(id)
/// Create an LED object associated with the given LED:
///
///   - `id` is the LED number, 1-4.
STATIC mp_obj_t led_obj_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    // check arguments
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    // get led number
    mp_int_t led_id = mp_obj_get_int(args[0]);

    // check led number
    if (!(1 <= led_id && led_id <= NUM_LEDS)) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "LED(%d) does not exist", led_id));
    }

    pyb_led_obj_t* self;
    // create new UART object
    
    self =  gc_alloc(sizeof(*self), GC_ALLOC_FLAG_HAS_FINALISER);


    Mux_Take(self, "led", led_id, "-", &self->mux);
    if (self->mux.pPinObj == 0) {
        /* downward compatible with previous hard-coded pin object mapping
        drop the allocated pyb_led_obj_t instance to GC */
        self = &pyb_led_obj[led_id - 1];
        // nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "LED(%d) can't take reqruied pin", led_id));
        self->mux.pPinObj = self->led_pin;
        strcpy(self->mux.szComboKey, "-");
        strcpy(self->mux.szHint, "-");
        return self;            
    }
    self->base.type = &pyb_led_type;
    self->led_id = led_id;    
    self->led_pin = self->mux.pPinObj;
    mp_hal_ConfigGPIO(self->led_pin, GPIO_MODE_OUTPUT_PP, 1);
    // return static led object
    return self;
}

/// \method on()
/// Turn the LED on.
mp_obj_t led_obj_on(mp_obj_t self_in) {
    pyb_led_obj_t *self = self_in;
    mp_hal_pin_low(self->led_pin);
    return mp_const_none;
}

/// \method off()
/// Turn the LED off.
mp_obj_t led_obj_off(mp_obj_t self_in) {
    pyb_led_obj_t *self = self_in;
    mp_hal_pin_high(self->led_pin);
    return mp_const_none;
}

/// \method toggle()
/// Toggle the LED between on and off.
mp_obj_t led_obj_toggle(mp_obj_t self_in) {
    pyb_led_obj_t *self = self_in;
    led_toggle(self->led_id);
    return mp_const_none;
}

mp_obj_t led_obj_del(mp_obj_t self_in) {
    pyb_led_obj_t *self = self_in;
    Mux_Give(&self->mux);
    return mp_const_none;
}

/// \method intensity([value])
/// Get or set the LED intensity.  Intensity ranges between 0 (off) and 255 (full on).
/// If no argument is given, return the LED intensity.
/// If an argument is given, set the LED intensity and return `None`.
mp_obj_t led_obj_intensity(mp_uint_t n_args, const mp_obj_t *args) {
    pyb_led_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(led_get_intensity(self->led_id));
    } else {
        led_set_intensity(self->led_id, mp_obj_get_int(args[1]));
        return mp_const_none;
    }
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(led_obj_on_obj, led_obj_on);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(led_obj_off_obj, led_obj_off);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(led_obj_toggle_obj, led_obj_toggle);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(led_obj_del_obj, led_obj_del);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(led_obj_intensity_obj, 1, 2, led_obj_intensity);

STATIC const mp_rom_map_elem_t led_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_on), MP_ROM_PTR(&led_obj_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_off), MP_ROM_PTR(&led_obj_off_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&led_obj_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_toggle), MP_ROM_PTR(&led_obj_toggle_obj) },
    { MP_ROM_QSTR(MP_QSTR_intensity), MP_ROM_PTR(&led_obj_intensity_obj) },
};

STATIC MP_DEFINE_CONST_DICT(led_locals_dict, led_locals_dict_table);

const mp_obj_type_t pyb_led_type = {
    { &mp_type_type },
    .name = MP_QSTR_LED,
    .print = led_obj_print,
    .make_new = led_obj_make_new,
    .locals_dict = (mp_obj_dict_t*)&led_locals_dict,
};

#else
// For boards with no LEDs, we leave an empty function here so that we don't
// have to put conditionals everywhere.
void led_init(void) {
}
void led_state(pyb_led_t led, int state) {
}
void led_toggle(pyb_led_t led) {
}
#endif  // defined(MICROPY_HW_LED1)
