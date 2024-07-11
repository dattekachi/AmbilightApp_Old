#pragma once

#include <effectengine/AnimationBaseMusic.h>

#define AMUSIC_STEREORED "Music: stereo for LED strip (RED)"

class Animation4Music_StereoRed : public AnimationBaseMusic
{
public:

	Animation4Music_StereoRed();

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
};
