#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_WAVESPULSESLOW "Music: pulse waves for LED strip (MULTI COLOR SLOW)"

class Animation4Music_WavesPulseSlow : public AnimationBaseMusic
{
public:

	Animation4Music_WavesPulseSlow();

	void Init(
		QImage& ambilightImage,
		int ambilightLatchTime) override;

	bool Play(QPainter* painter) override;

	static EffectDefinition getDefinition();

	bool hasOwnImage() override;
	bool getImage(Image<ColorRgb>& image) override;

private:
	uint32_t _internalIndex;
	int 	 _oldMulti;
	QList<QColor> _buffer;
};
