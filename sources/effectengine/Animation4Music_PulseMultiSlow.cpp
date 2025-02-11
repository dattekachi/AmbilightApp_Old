/* Animation4Music_PulseMultiSlow.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2023 awawa-dev
*
*  Project homesite: http://ambilightled.com
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#include <effectengine/Animation4Music_PulseMultiSlow.h>
#include <base/SoundCapture.h>

Animation4Music_PulseMultiSlow::Animation4Music_PulseMultiSlow() :
	AnimationBaseMusic(AMUSIC_PULSEMULTISLOW),
	_internalIndex(0),
	_oldMulti(0)
{

};

EffectDefinition Animation4Music_PulseMultiSlow::getDefinition()
{
	EffectDefinition ed(true, EffectFactory<Animation4Music_PulseMultiSlow>);
	ed.name = AMUSIC_PULSEMULTISLOW;
	return ed;
}

void Animation4Music_PulseMultiSlow::Init(
	QImage& ambilightImage,
	int ambilightLatchTime
)
{
	SetSleepTime(15);
}

bool Animation4Music_PulseMultiSlow::Play(QPainter* painter)
{
	return false;
}

bool Animation4Music_PulseMultiSlow::hasOwnImage()
{
	return true;
};

bool Animation4Music_PulseMultiSlow::getImage(Image<ColorRgb>& newImage)
{
	bool newData = false;
	auto r = _soundCapture->hasResult(this, _internalIndex, NULL, &newData, NULL, &_oldMulti);

	if (r == nullptr || !newData)
		return false;

	QColor color, empty;
	uint32_t maxSingle, average;

	if (!r->GetStats(average, maxSingle, empty, NULL, &color))
		return false;

	int red = color.red();
	int green = color.green();
	int blue = color.blue();

	newImage.fastBox(0, 0, newImage.width() - 1, newImage.height() - 1, red, green, blue);

	return true;
};







