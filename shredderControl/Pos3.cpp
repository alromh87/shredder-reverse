#include "Pos3.h"

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

  bool up = this->debouncerUp.read() == HIGH ? true : false;
  bool down = this->debouncerDown.read() == LOW ? true : false;

  int newDirection = 0;

  if (up)
  {
    newDirection = 1; // forward
  }
  if (down)
  {
    newDirection = 2; // reverse
  }
  if (!up && !down)
  {
    newDirection = 0; // stop
  }

  if (up && down)
  {
    newDirection = -1; // invalid
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
