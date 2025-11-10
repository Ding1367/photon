#ifndef __INPUT_H__
#define __INPUT_H__

#define PHOTON_INVALID_KEY (-2763)
#define PHOTON_KUP 0601
#define PHOTON_KDOWN 0602
#define PHOTON_KLEFT 0603
#define PHOTON_KRIGHT 0604
#define PHOTON_KHOME 0605
#define PHOTON_KEND 0606

int photon_input_read_key(void);

#endif//__INPUT_H__
