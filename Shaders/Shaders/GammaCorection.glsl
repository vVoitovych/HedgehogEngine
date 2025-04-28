#ifndef GAMMA_CORRECTION
#define GAMMA_CORRECTION

const float screenGamma = 2.2; // Assume the monitor is calibrated to the sRGB color space

vec3 LinearToGamma(vec3 colorLinear)
{
	return pow(colorLinear, vec3(1.0 / screenGamma));
}

vec3 GammaToLinear(vec3 colorGamma)
{
	return pow(colorGamma, vec3(screenGamma));
}


#endif

