#ifndef AUDIO_H
#define AUDIO_H

#include <SDL.h>

/* Használható hangok flag-jei */
#define SOUND_SHOT 0x00000001 /* Lövéshang */
#define SOUND_DEATH 0x00000002 /* Halálhang */
#define SOUND_BONUSWPN 0x00000004 /* Bónuszfegyverek hangja */
#define SOUND_MENUBTN 0x10000000 /* Menühang */

/* Hangok frekvenciái */
#define BUTTON_FREQ 1000 /* A gombnyomás hangjának frekvenciája */
#define SHOT_FREQ_DISTORT 1500 /* A lövés torz komponense */
#define SHOT_FREQ_CONTINOUS 6000 /* A lövés folyamatos komponense */
#define DEATH_FREQ_DISTORT 4200 /* A halál torz komponense */
#define DEATH_FREQ_CONTINOUS 5200 /* A halál folyamatos komponense */
#define BONUS_FREQ_DISTORT 4900 /* A bónusz torz komponense */
#define BONUS_FREQ_CONTINOUS 5250 /* A bónusz folyamatos komponense */

/* A hangfunkciók leírása az audio.c-ben található */
void AudioCallback(void*, Uint8*, int);

#endif /* AUDIO_H */
