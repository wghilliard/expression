#include <Arduino.h>

const double flair = 3;

double getSign(double x) {
  return (x > 0.0) - (x < 0.0);
}

double pupilHorizonalOffset(double carSpeed, double viewDistance, double centripalAcceration) {
  if (centripalAcceration < 0.001)
  {
    return 0.0;
  }

  double carSpeedSquared = carSpeed * carSpeed;
  double absCentripalAcceration = abs(centripalAcceration);
  double signCentripalAcceration = getSign(centripalAcceration);

  double turningRadius = carSpeedSquared / absCentripalAcceration;
  double deflectionMeters = turningRadius - sqrt((turningRadius * turningRadius) - (viewDistance * viewDistance));
  double pupilHorizonalOffsetValue = deflectionMeters / viewDistance * signCentripalAcceration;

  Serial.printf("carSpeedSquared=%f signCentripalAcceration=%f absCentripalAcceration=%f pupilHorizonalOffsetValue=%f\n", carSpeedSquared, signCentripalAcceration, absCentripalAcceration, pupilHorizonalOffsetValue);
  // return ((carSpeedSquared / (viewDistance * absCentripalAcceration)) - 1.0) * signCentripalAcceration;
  return pupilHorizonalOffsetValue * flair;
}