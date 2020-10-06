#define MOUTH_CNT 6
#define LED_CNT 52

#define LED_brightness   100
#define LED_brightness_0 33


// Тут верх и низ перевернуты. Так распаял ленту.
const uint8_t mouths[MOUTH_CNT][LED_CNT] = { {
      0, 0, 0, 0,
   0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,
0, 1, 1, 1, 1, 1, 1, 0,
1, 1, 0, 0, 0, 0, 1, 1,
0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0,
      0, 0, 0, 0
}, {
      0, 0, 0, 0,
   0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,
0, 1, 1, 1, 1, 1, 1, 0,
1, 1, 0, 0, 0, 0, 1, 1,
0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0,
      0, 0, 0, 0
}, {
      0, 0, 0, 0,
   0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,
1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 0, 0, 1, 1, 1,
0, 0, 0, 1, 1, 0, 0, 0,
   0, 0, 0, 0, 0, 0,
      0, 0, 0, 0
}, {
      0, 0, 0, 0,
   0, 0, 0, 0, 0, 0,
0, 0, 1, 1, 1, 1, 0, 0,
1, 1, 0, 0, 0, 0, 1, 1,
1, 1, 0, 0, 0, 0, 1, 1,
0, 0, 1, 1, 1, 1, 0, 0,
   0, 0, 0, 0, 0, 0,
      0, 0, 0, 0
}, {
      0, 0, 0, 0,
   0, 1, 1, 1, 1, 0,
0, 1, 0, 0, 0, 0, 1, 0,
1, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 1,
0, 1, 0, 0, 0, 0, 1, 0,
   0, 1, 1, 1, 1, 0,
      0, 0, 0, 0
}, {
      1, 1, 1, 1,
   1, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 1,
1, 0, 0, 0, 0, 0, 0, 1,
   1, 0, 0, 0, 0, 1,
      1, 1, 1, 1
} };



uint8_t leds_info[LED_CNT * 3];



/*****/

#define MIC_PIN 0   // микрофона pin
#define MIC_CNT 100 // количество измерений для определение громкости
#define VOL_THR 25  // порог тишины
#define VOL_MAX 200 // максимальная громкость


int get_volume () {
  int m = 0;
  for (int i = 0; i < MIC_CNT; i++) {
    int v = analogRead(MIC_PIN);
    if (v > m) m = v;
  }
  return m;
}


int filter_volume (int current) {
  static int filtered = 0;
  filtered += (current - filtered) * 0.95; // скользящий фильтр

  static int background = VOL_THR; // фоновые звуки

  int volume = filtered - background;

  if (volume > 0) {
    background += (filtered - background) * 0.005;
  } else {
    volume = 0;
  }

  if (background > filtered) {
    background = filtered;
  }

  return volume;
}


int get_mouth (int volume, int max_mouth_index) {
  int index = filter_volume(volume) * max_mouth_index / (VOL_MAX - VOL_THR);
  return index < max_mouth_index ? index : max_mouth_index;
}


// Медианный фильтр 
int median_3_filter (int v) {
  static int a = 0, b = 0, c = 0;
  a = b;
  b = c;
  c = v;
  if (a <= b && a <= c) {
    return b <= c ? b : c;
  } else if (b <= a && b <= c) {
    return a <= c ? a : c;
  } else {
    return a <= b ? a : b;
  }
}


/*****/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>


#if !(F_CPU == 8000000 || F_CPU == 16000000)
  #error "It is only for 8MHz or 16MHz F_CPU."
#endif


#define ws2812_PORTREG  PORTB
#define ws2812_DDRREG   DDRB
#define ws2812_PIN      2   // Data out pin. Это 10 пин


void ws2812 (uint8_t *data, uint16_t data_len, uint8_t pin_mask) {
   
  ws2812_DDRREG |= pin_mask; // Enable output
  
  uint8_t mask_l  =~ pin_mask & ws2812_PORTREG;
  uint8_t mask_h  =  pin_mask | ws2812_PORTREG;
  
  uint8_t sreg_old = SREG;
  cli();  

  while (data_len--) {
    uint8_t cur_byte = *data++;
    int8_t bite_ctr;
    
    asm volatile(
    "       ldi  %[bite_ctr], 8 \n\t"      // 1 tick; bite_ctr = 0

    "loop:              \n\t"
    "       out   %[port], %[mask_h] \n\t" // 1 tick; port = mask_h
    "       nop         \n\t"              // 1 tick 
#if F_CPU == 16000000
    "       nop         \n\t"              // 1 tick
    "       nop         \n\t"              // 1 tick
    "       nop         \n\t"              // 1 tick
#endif
    // 2 tick
    "       sbrs %[cur_byte], 7     \n\t"  // jump over the next if 7 bit is set
    "       out  %[port], %[mask_l] \n\t"  // port = mask_l

    "       lsl  %[cur_byte]        \n\t"  // 1 tick; <<
    "       nop         \n\t"              // 1 tick
#if F_CPU == 16000000
    "       nop         \n\t"              // 1 tick
    "       nop         \n\t"              // 1 tick
    "       nop         \n\t"              // 1 tick
    "       nop         \n\t"              // 1 tick
#endif
    "       out  %[port], %[mask_l]  \n\t" // 1 tick; port = mask_l
#if F_CPU == 16000000
    "       nop         \n\t"              // 1 tick
    "       nop         \n\t"              // 1 tick
    "       nop         \n\t"              // 1 tick
    "       nop         \n\t"              // 1 tick
#endif
    "       dec  %[bite_ctr] \n\t"         // 1 tick; bite_ctr--
    "       brne loop        \n\t"         // 2 tick if go; go to loop if <>

    : [bite_ctr] "=&d" (bite_ctr)
    : [cur_byte] "r"   (cur_byte),
      [port]     "I"   (_SFR_IO_ADDR(ws2812_PORTREG)),
      [mask_h]   "r"   (mask_h),
      [mask_l]   "r"   (mask_l)
    );
  }
  
  SREG = sreg_old;
}


/*****/

void setup() {
}

void loop() {
  int volume = get_volume();
  int mouth_index = get_mouth(volume, MOUTH_CNT - 1);
  int mouth_index_f = median_3_filter(mouth_index);

  const uint8_t *mouth = mouths[mouth_index_f];
  uint8_t brightness = mouth_index_f == 0 ? LED_brightness_0 : LED_brightness;

  for (uint16_t i = 0; i < LED_CNT; i++) {
    if (mouth[i]) {
      leds_info[3*i]   = brightness;
      leds_info[3*i+1] = brightness;
      leds_info[3*i+2] = brightness;
    } else {
      leds_info[3*i]   = 0;
      leds_info[3*i+1] = 0;
      leds_info[3*i+2] = 0;
    }
  }

  ws2812(leds_info, LED_CNT * 3, _BV(ws2812_PIN));
  
  delay(10);
}
