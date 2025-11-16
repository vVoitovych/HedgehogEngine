#include <ctime>
volatile time_t forceRebuild = std::time(nullptr);
