#define DEFAULT_VIEW_DISTANCE 20.0

// args
// - car speed, meters per second
// - distance outlook, meters
// - centriptal acceleration (left / right), meters per second ^2, bounds -30, 30
// returns
// - offset of the pupil as fraction of the radius of the white canvas [-1, 1]

double pupilHorizonalOffset(double carSpeed, double viewDistance, double centripalAcceration);