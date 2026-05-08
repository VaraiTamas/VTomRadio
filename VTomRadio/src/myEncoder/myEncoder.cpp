// based on https://github.com/igorantolic/ai-esp32-rotary-encoder code
//
//

#include "esp_log.h"
#define LOG_TAG "myEncoder"

#include "myEncoder.h"

void IRAM_ATTR myEncoder::readEncoder_ISR()
{

	unsigned long now = millis();
	portENTER_CRITICAL_ISR(&(this->mux));
	if (this->isEnabled)
	{
		// code from https://www.circuitsathome.com/mcu/reading-rotary-encoder-on-arduino/
		/**/
		this->old_AB <<= 2; //remember previous state

		int8_t ENC_PORT = ((digitalRead(this->encoderBPin)) ? (1 << 1) : 0) | ((digitalRead(this->encoderAPin)) ? (1 << 0) : 0);

		this->old_AB |= (ENC_PORT & 0x03); //add current state

		//this->encoder0Pos += ( this->enc_states[( this->old_AB & 0x0f )]);
		int8_t currentDirection = (this->enc_states[(this->old_AB & 0x0f)]); //-1,0 or 1

		if (currentDirection != 0)
		{
			long prevRotaryPosition = this->encoder0Pos / this->encoderSteps;
			this->encoder0Pos += currentDirection;
			long newRotaryPosition = this->encoder0Pos / this->encoderSteps;

			if (newRotaryPosition != prevRotaryPosition && rotaryAccelerationCoef > 1)
			{
				//additional movements cause acceleration?
				// at X ms, there should be no acceleration.
				unsigned long accelerationLongCutoffMillis = 200;
				// at Y ms, we want to have maximum acceleration
				unsigned long accelerationShortCutffMillis = 4;

				// compute linear acceleration
				if (currentDirection == lastMovementDirection &&
					currentDirection != 0 &&
					lastMovementDirection != 0)
				{
					// ... but only of the direction of rotation matched and there
					// actually was a previous rotation.
					unsigned long millisAfterLastMotion = now - lastMovementAt;

					if (millisAfterLastMotion < accelerationLongCutoffMillis)
					{
						if (millisAfterLastMotion < accelerationShortCutffMillis)
						{
							millisAfterLastMotion = accelerationShortCutffMillis; // limit to maximum acceleration
						}
						// Keep acceleration predictable: never inject more than one extra step per detent.
						unsigned long accelerationBoost = rotaryAccelerationCoef / millisAfterLastMotion;
						if (accelerationBoost > 1)
						{
							accelerationBoost = 1;
						}
						if (currentDirection > 0)
						{
							this->encoder0Pos += accelerationBoost;
						}
						else
						{
							this->encoder0Pos -= accelerationBoost;
						}
					}
				}
				this->lastMovementAt = now;
				this->lastMovementDirection = currentDirection;
			}

			//respect limits
			if (this->encoder0Pos > (this->_maxEncoderValue))
				this->encoder0Pos = this->_circleValues ? this->_minEncoderValue : this->_maxEncoderValue;
			if (this->encoder0Pos < (this->_minEncoderValue))
				this->encoder0Pos = this->_circleValues ? this->_maxEncoderValue : this->_minEncoderValue;
		}
	}
	portEXIT_CRITICAL_ISR(&(this->mux));
}


myEncoder::myEncoder(uint8_t encoder_APin, uint8_t encoder_BPin, uint8_t encoderSteps, bool internalPullup)
{
	this->old_AB = 0;

	this->encoderAPin = encoder_APin;
	this->encoderBPin = encoder_BPin;
	this->encoderSteps = encoderSteps;

	pinMode(this->encoderAPin, internalPullup?INPUT_PULLUP:INPUT);
	pinMode(this->encoderBPin, internalPullup?INPUT_PULLUP:INPUT);
}

void myEncoder::setBoundaries(long minEncoderValue, long maxEncoderValue, bool circleValues)
{
	this->_minEncoderValue = minEncoderValue * this->encoderSteps;
	this->_maxEncoderValue = maxEncoderValue * this->encoderSteps;

	this->_circleValues = circleValues;
}

long myEncoder::readEncoder()
{
	return (this->encoder0Pos / this->encoderSteps);
}

void myEncoder::setEncoderValue(long newValue)
{
	reset(newValue);
}

long myEncoder::encoderChanged()
{
	long _encoder0Pos = readEncoder();
	long encoder0Diff = _encoder0Pos - this->lastReadEncoder0Pos;

	this->lastReadEncoder0Pos = _encoder0Pos;
	if (encoder0Diff > 1)
		return 1;
	if (encoder0Diff < -1)
		return -1;
	return encoder0Diff;
}

void myEncoder::setup(void (*ISR_callback)(void))
{
	attachInterrupt(digitalPinToInterrupt(this->encoderAPin), ISR_callback, CHANGE);
	attachInterrupt(digitalPinToInterrupt(this->encoderBPin), ISR_callback, CHANGE);
}

void myEncoder::begin()
{
	this->lastReadEncoder0Pos = 0;
}

void myEncoder::reset(long newValue_)
{
	newValue_ = newValue_ * this->encoderSteps;
	this->encoder0Pos = newValue_;
	this->lastReadEncoder0Pos = this->encoder0Pos;
	if (this->encoder0Pos > this->_maxEncoderValue)
		this->encoder0Pos = this->_circleValues ? this->_minEncoderValue : this->_maxEncoderValue;
	if (this->encoder0Pos < this->_minEncoderValue)
		this->encoder0Pos = this->_circleValues ? this->_maxEncoderValue : this->_minEncoderValue;
}

void myEncoder::enable()
{
	this->isEnabled = true;
}
void myEncoder::disable()
{
	this->isEnabled = false;
}
