#ifndef F22_HOVER_SIM_H
#define F22_HOVER_SIM_H
#include "hover.h"
void HoverSim_Init(HoverSample *sample);
/* Idealized software plant for teaching/testing, never real flight evidence. */
void HoverSim_Step(HoverSample *sample, const HoverOutput *output, float dt);
#endif
