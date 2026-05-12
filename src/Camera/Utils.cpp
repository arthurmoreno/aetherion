// Camera/Utils.cpp — see `Camera/Utils.hpp` for the no-deps contract.

#include "Camera/Utils.hpp"

bool isMouseWithin(int mx, int my, int x, int y, int width, int height) {
  return x <= mx && mx <= x + width && y <= my && my <= y + height;
}
