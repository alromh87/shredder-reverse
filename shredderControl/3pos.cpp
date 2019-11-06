#include "3pos.h"

Pos3::Pos3(int upPin, int downPin)
{
  this->upPin = upPin;
  this->downPin = downPin;
}

int Pos3::setup()
{

  this->debouncerUp = Bounce();
  this->debouncerUp.attach(this->upPin, INPUT_PULLUP);
  this->debouncerUp.interval(25);

  this->debouncerDown = Bounce();
  this->debouncerDown.attach(this->downPin, INPUT_PULLUP);
  this->debouncerDown.interval(25);
  return 0;
}

int Pos3::read()
{

  this->debouncerUp.update();
  this->debouncerDown.update();

  bool up = this->debouncerUp.read() == 0 ? true : false;
  bool down = this->debouncerDown.read() == 0 ? true : false;

  int newDirection = 0;

  if (up)
  {
    newDirection = FORWARD;
  }
  if (down)
  {
    newDirection = REVERSE;
  }
  if (!up && !down)
  {
    newDirection = STOP;
  }
  return newDirection;
}

int Pos3::loop()
{
  int newDirection = this->read();

  if (newDirection != this->switch_pos)
  {
    this->last_switch = this->switch_pos;
  }
  this->switch_pos = newDirection;

  return this->switch_pos;
}
