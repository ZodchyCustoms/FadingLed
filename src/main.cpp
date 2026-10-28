#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <my_pins.h>
#include <timer2.h>

#define BUTTON 14
#define LED 11
#define LED2 9
#define LED3 10

// Вершина счета програмного шим:
#define SOFT_PWM_TOP 255
// До скольки наращивать яркость перед 100 % включением:
#define INCREMENT_DUTY_TOP (SOFT_PWM_TOP)
// Скорость изменения яркости:
#define FADE_SPEED 200

// Для тестов
volatile bool flag = false;

class FaddingLed
{
public:
  FaddingLed(uint8_t pin, uint16_t speed)
  {
    this->_pin = pin;
    this->_countTop = speed;
    setPinOutput(_pin);
    pwm.init(SOFT_PWM_TOP, 0, _pin);
  }

  void engineMain()
  {
    _go();
    _update();
  }

  void on()
  {
    pwm.duty = INCREMENT_DUTY_TOP;
    _newState = ON;
  }

  // Яркость в процентах
  void on(uint8_t brightness)
  {
    if (brightness > 100)
      brightness = 100;
    pwm.duty = ( (SOFT_PWM_TOP * brightness) / 100 );
    _newState = FADE_HOLD;
  }

  void fadeOn()
  {
    _newState = FADE_ON;
  }

  void off()
  {
    _newState = OFF;
  }

  void fadeOff()
  {
    _newState = FADE_OFF;
  }

  void toggle()
  {
    switch (_currentLedState)
    {
      case true:
      _newState = OFF;
      break;
      case false:
      _newState = ON;
      break;
    };
  }

  void fadeToggle()
  {
    switch (_currentLedState)
    {
      case true:
      _newState = FADE_OFF;
      break;
      case false:
      _newState = FADE_ON;
      break;
    };
  }

  void engineFade()
  {
    switch (_fadeDirection)
    {
      case FADDING_OFF:
      break;
      case FADDING_UP:
      pwm.tick();
      if (_count < _countTop)   // Пора ли увеличить скважность
        _count++;
      else
      {
        switch (pwm.incrementDuty())
        {
          case true:
          _count = 0;
          break;
          case false:
          _fadeDirection = FADDING_OFF;
          _currentLedState = true;
          setPin(_pin, _currentLedState);
          //_newState = ON;   // Опасный момент - можно проскочить go()
          PORTB ^= (1 << PB5);
          flag = true;
          break;
        };
      }
      break;
      case FADDING_DOWN:
      pwm.tick();
      if (_count < _countTop)
        _count++;
      else
      {
        switch (pwm.decrementDuty())
        {
          case true:
          _count = 0;
          break;
          case false:
          _fadeDirection = FADDING_OFF;
          _currentLedState = false;
          setPin(_pin, _currentLedState);
          // _newState = OFF;    // Опасный момент - можно проскочить go()
          PORTB ^= (1 << 5);
          flag = true;
          break;
        };
      }
      break;
      case FADDING_HOLD:
      pwm.tick();
      break;
    };
  }

private:

  enum State
  {
    START,                // Стартовое состояние
    WAIT,                 // Ожидание первого действия
    ON,                   // Включен
    OFF,                  // Выключен
    FADE_ON,              // Плавно включается
    FADE_OFF,             // Плавно выключается
    FADE_HOLD             // Светит с заданной яркостью
  };

  enum FadeDirection
  {
    FADDING_OFF,
    FADDING_UP,
    FADDING_DOWN,
    FADDING_HOLD
  };

  bool _currentLedState = false;
  uint8_t _pin;
  uint16_t _count = 0, _countTop;   // Счетчик скорости изменения яркости. Неудачное название?
  State _state = START, _newState = WAIT;
  FadeDirection _fadeDirection = FADDING_OFF;

  void _go()
  {
    switch (_state)
    {
      case START:                 // -
      break;
      case WAIT:                  // -
      break;
      case ON:                    // Установить высокий уровень на ноге
      _currentLedState = true;
      setPin(_pin, _currentLedState);
      break;
      case OFF:                   // Установить низкий уровень на ноге
      _currentLedState = false;
      setPin(_pin, _currentLedState);
      break;
      case FADE_ON:               // Запустить плавное нарастание яркости
      _fadeDirection = FADDING_UP;
      break;
      case FADE_OFF:              // Запустить плавное понижение яркости
      _fadeDirection = FADDING_DOWN;
      break;
      case FADE_HOLD:             // Запустить шим на заданное
      _fadeDirection = FADDING_HOLD;
      break;
    };
  }

  void _update()
  {
    switch (_state)
    {
      case START:                 // Безусловный переход в WAIT
      _state = WAIT;
      break;
      case WAIT:                  // Если запрошено новое состояние (_newState != _state) то перейти в соответсвующее состояние
      if (_state != _newState)
        _state = _newState;
      break;
      case ON:                    // Перейти в WAIT
      _newState = _state = WAIT;
      break;
      case OFF:                   // Перейти в WAIT
      _newState = _state = WAIT;
      break;
      case FADE_ON:               // Перейти в WAIT
      _newState = _state = WAIT;
      break;
      case FADE_OFF:              // Перейти в WAIT
      _newState = _state = WAIT;
      break;
      case FADE_HOLD:             // Перейти в WAIT
      _newState = _state = WAIT;
      break;
    };
  }

// ###############################################################################

struct ProgrammPwmChannel
{
  volatile uint8_t duty;
  volatile uint8_t counter = 0;
  uint8_t top, pin;

  void init(uint8_t top, uint8_t duty, uint8_t pin)
  {
    this->top = top;
    this->duty = duty;
    this->pin = pin;
  }

  void tick()
  {
    if (counter != top)
    {
      if (counter == duty)
        setPin(pin, false);
      counter++;
    } else
    {
      counter = 0;
      setPin(pin, true);
    }
  }

  // Инкрементировать скважность 
  bool incrementDuty()
  {
    switch (duty)
    {
      default:
      duty++;
      return true;
      case INCREMENT_DUTY_TOP:
      return false;
    };
  }

  // Декрементировать скважность НЕ РАБОТАЕТ
  bool decrementDuty()
  {
    switch (duty)
    {
      default:
      duty--;
      return true;
      case 0:
      return false;
    };
  }

} pwm;
// ###############################################################################
};


void init();

FaddingLed led(LED, FADE_SPEED);
FaddingLed led2(LED2, FADE_SPEED / 2);
FaddingLed led3(LED3, FADE_SPEED);

ISR(TIMER2_COMPA_vect)
{
  led.engineFade();
  led2.engineFade();
  led3.engineFade();
  switch (flag)
  {
    case true:
    flag = false;
    led.fadeToggle();
    led2.fadeToggle();
    break;
    case false:
    break;
  };
}


int main()
{
  init();
  sei();

  led.on();
  led2.fadeOn();
  led3.on(5);
  

  while (true)
  {
    led.engineMain();
    led2.engineMain();
    led3.engineMain();
  }
  return 0;
}


void init()
{
  DDRB = 255;
  timer2.init(Timer2::Mode::Ctc, 1);
  timer2.setInrerrupt(Timer2::Interrupt::CompareA, 160);    // 100 kHz - OCR2A, CTC
}

