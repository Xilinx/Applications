#ifndef ENC_GETRESIDULE_H_
#define ENC_GETRESIDULE_H_

int GetPredModesResiduleForFrameOcl(int width, int height, int bits,
                                     const uint32_t* const argb, const uint32_t* const argb_scratch,
                                     int exact, int* const histo_array);

#endif  /* ENC_GETRESIDULE_H_ */
